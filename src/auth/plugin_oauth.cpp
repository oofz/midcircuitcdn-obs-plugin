/*
 * MidcircuitCDN OBS Plugin — OAuth Loopback Server (Implementation)
 * ────────────────────────────────────────────────────────────────────────────
 * Manages the full Discord OAuth credential handoff via a temporary local
 * HTTP server. The flow:
 *
 *   1. Bind ephemeral port, generate CSRF state token.
 *   2. Open browser to midcircuitcdn.com/login?plugin_auth_port=PORT&state=STATE
 *   3. User completes Discord login on the Next.js frontend.
 *   4. Next.js redirects to http://localhost:PORT/auth?slug=...&key=...&url=...&state=STATE
 *   5. We validate state, extract creds, serve success page, invoke callback.
 */

#include "plugin_oauth.hpp"
#include "../plugin-macros.h"

#include <obs-module.h>

#include <string>
#include <random>
#include <sstream>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <cstring>
#include <algorithm>
#include <map>

/* ── Platform socket includes ─────────────────────────────────────────────── */
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET socket_t;
#define CLOSE_SOCKET closesocket
#define SOCKET_INVALID INVALID_SOCKET
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
typedef int socket_t;
#define CLOSE_SOCKET close
#define SOCKET_INVALID (-1)
#endif

/* ── Internal State ───────────────────────────────────────────────────────── */
static std::thread g_oauth_thread;
static std::atomic<bool> g_oauth_active{false};
static std::atomic<bool> g_oauth_cancel{false};
static std::mutex g_oauth_mutex;

/* ── Constants ────────────────────────────────────────────────────────────── */
static const char *MCDN_LOGIN_BASE_URL =
	"https://midcircuitcdn.com/login";
static const char *MCDN_DEFAULT_SERVER_URL =
	"rtmp://live.midcircuitcdn.com/live";
static const int OAUTH_TIMEOUT_SECONDS = 300; /* 5 minutes */

/* ── Utility: Generate a random hex string for CSRF state ─────────────── */
static std::string GenerateStateToken(size_t length = 32)
{
	static const char hex[] = "0123456789abcdef";
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dist(0, 15);

	std::string token;
	token.reserve(length);
	for (size_t i = 0; i < length; i++)
		token += hex[dist(gen)];
	return token;
}

/* ── Utility: URL-decode a percent-encoded string ─────────────────────── */
static std::string UrlDecode(const std::string &src)
{
	std::string result;
	result.reserve(src.size());
	for (size_t i = 0; i < src.size(); i++) {
		if (src[i] == '%' && i + 2 < src.size()) {
			int hi = 0, lo = 0;
			if (sscanf(src.substr(i + 1, 2).c_str(), "%x",
				   &hi) == 1) {
				result += static_cast<char>(hi);
				i += 2;
			} else {
				result += src[i];
			}
		} else if (src[i] == '+') {
			result += ' ';
		} else {
			result += src[i];
		}
	}
	return result;
}

/* ── Utility: Parse query string into key=value map ───────────────────── */
static std::map<std::string, std::string>
ParseQueryString(const std::string &query)
{
	std::map<std::string, std::string> params;
	std::istringstream stream(query);
	std::string pair;

	while (std::getline(stream, pair, '&')) {
		auto eq = pair.find('=');
		if (eq != std::string::npos) {
			std::string key = pair.substr(0, eq);
			std::string val = UrlDecode(pair.substr(eq + 1));
			params[key] = val;
		}
	}
	return params;
}

/* ── Utility: Extract the GET path and query from a raw HTTP request ──── */
static bool ParseHttpGet(const std::string &raw, std::string &path,
			 std::string &query)
{
	/* Expect: "GET /auth?slug=...&key=... HTTP/1.1\r\n..." */
	if (raw.substr(0, 4) != "GET ")
		return false;

	auto space = raw.find(' ', 4);
	if (space == std::string::npos)
		return false;

	std::string uri = raw.substr(4, space - 4);
	auto qmark = uri.find('?');
	if (qmark != std::string::npos) {
		path = uri.substr(0, qmark);
		query = uri.substr(qmark + 1);
	} else {
		path = uri;
		query = "";
	}
	return true;
}

/* ── Success HTML page (served to the user's browser) ─────────────────── */
static const char *SUCCESS_HTML = R"html(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <title>MidcircuitCDN — Connected!</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #0f0c29, #302b63, #24243e);
            color: #fff;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
        }
        .card {
            background: rgba(255,255,255,0.06);
            backdrop-filter: blur(20px);
            border: 1px solid rgba(255,255,255,0.1);
            border-radius: 16px;
            padding: 48px 40px;
            text-align: center;
            max-width: 420px;
        }
        .check { font-size: 48px; margin-bottom: 16px; }
        h1 { font-size: 22px; margin-bottom: 8px; }
        p { color: rgba(255,255,255,0.6); font-size: 14px; }
    </style>
    <script>
        // Scrub credentials from browser history immediately
        history.replaceState({}, '', '/');
    </script>
</head>
<body>
    <div class="card">
        <div class="check">✅</div>
        <h1>Connected to MidcircuitCDN</h1>
        <p>Your stream settings have been configured in OBS.<br>You may close this window.</p>
    </div>
</body>
</html>)html";

/* ── Error HTML page ──────────────────────────────────────────────────── */
static const char *ERROR_HTML = R"html(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <title>MidcircuitCDN — Error</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #29100f, #632b2b, #3e2424);
            color: #fff;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
        }
        .card {
            background: rgba(255,255,255,0.06);
            backdrop-filter: blur(20px);
            border: 1px solid rgba(255,255,255,0.1);
            border-radius: 16px;
            padding: 48px 40px;
            text-align: center;
            max-width: 420px;
        }
        .icon { font-size: 48px; margin-bottom: 16px; }
        h1 { font-size: 22px; margin-bottom: 8px; }
        p { color: rgba(255,255,255,0.6); font-size: 14px; }
    </style>
    <script>history.replaceState({}, '', '/');</script>
</head>
<body>
    <div class="card">
        <div class="icon">❌</div>
        <h1>Authentication Failed</h1>
        <p>The CSRF token did not match. Please try again from OBS.</p>
    </div>
</body>
</html>)html";

/* ── HTTP Response helper ─────────────────────────────────────────────── */
static void SendHttpResponse(socket_t client, int status_code,
			     const char *status_text, const char *body)
{
	std::ostringstream resp;
	resp << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
	resp << "Content-Type: text/html; charset=utf-8\r\n";
	resp << "Cache-Control: no-store, no-cache, must-revalidate\r\n";
	resp << "Connection: close\r\n";
	resp << "Content-Length: " << strlen(body) << "\r\n";
	resp << "\r\n";
	resp << body;

	std::string data = resp.str();
	send(client, data.c_str(), (int)data.size(), 0);
}

/* ── Open the user's default browser ──────────────────────────────────── */
static void OpenBrowser(const std::string &url)
{
#ifdef _WIN32
	ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif defined(__APPLE__)
	std::string cmd = "open \"" + url + "\"";
	system(cmd.c_str());
#else
	std::string cmd = "xdg-open \"" + url + "\" &";
	system(cmd.c_str());
#endif
}

/* ── OAuth Thread Entry Point ─────────────────────────────────────────── */
static void OAuthThreadFunc(OAuthCallback on_success, int timeout_seconds)
{
	MCDN_LOG(LOG_INFO, "OAuth loopback thread started");

#ifdef _WIN32
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

	/* 1. Create TCP socket */
	socket_t server_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (server_sock == SOCKET_INVALID) {
		MCDN_LOG(LOG_ERROR, "Failed to create socket");
		g_oauth_active = false;
		return;
	}

	/* Allow port reuse */
	int opt = 1;
#ifdef _WIN32
	setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt,
		   sizeof(opt));
#else
	setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

	/* 2. Bind to ephemeral port (port 0 = OS assigns) */
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = 0; /* Ephemeral */

	if (bind(server_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		MCDN_LOG(LOG_ERROR, "Failed to bind loopback socket");
		CLOSE_SOCKET(server_sock);
		g_oauth_active = false;
		return;
	}

	/* 3. Retrieve the assigned port */
	socklen_t addr_len = sizeof(addr);
	getsockname(server_sock, (struct sockaddr *)&addr, &addr_len);
	int port = ntohs(addr.sin_port);
	MCDN_LOG(LOG_INFO, "OAuth loopback listening on port %d", port);

	listen(server_sock, 1);

	/* 4. Generate CSRF state token */
	std::string state = GenerateStateToken(32);
	MCDN_LOG(LOG_INFO, "OAuth state token generated (%zu chars)",
		 state.size());

	/* 5. Open browser */
	std::ostringstream login_url;
	login_url << MCDN_LOGIN_BASE_URL;
	login_url << "?plugin_auth_port=" << port;
	login_url << "&state=" << state;
	OpenBrowser(login_url.str());
	MCDN_LOG(LOG_INFO, "Opened browser for OAuth");

	/* 6. Set socket timeout for accept() */
#ifdef _WIN32
	DWORD tv = timeout_seconds * 1000;
	setsockopt(server_sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv,
		   sizeof(tv));
#else
	struct timeval tv;
	tv.tv_sec = timeout_seconds;
	tv.tv_usec = 0;
	setsockopt(server_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

	/* 7. Wait for the browser redirect */
	bool success = false;
	while (!g_oauth_cancel && !success) {
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);
		socket_t client = accept(server_sock,
					 (struct sockaddr *)&client_addr,
					 &client_len);

		if (client == SOCKET_INVALID) {
			if (g_oauth_cancel) {
				MCDN_LOG(LOG_INFO, "OAuth cancelled by user");
			} else {
				MCDN_LOG(LOG_WARNING,
					 "OAuth timed out after %d seconds",
					 timeout_seconds);
			}
			break;
		}

		/* Read the HTTP request */
		char buf[4096];
		int n = recv(client, buf, sizeof(buf) - 1, 0);
		if (n <= 0) {
			CLOSE_SOCKET(client);
			continue;
		}
		buf[n] = '\0';

		std::string raw(buf);
		std::string path, query;

		if (!ParseHttpGet(raw, path, query)) {
			/* Not a GET request, ignore (favicon.ico etc.) */
			SendHttpResponse(client, 404, "Not Found",
					 "<h1>Not Found</h1>");
			CLOSE_SOCKET(client);
			continue;
		}

		/* Only process /auth path */
		if (path != "/auth") {
			SendHttpResponse(client, 404, "Not Found",
					 "<h1>Not Found</h1>");
			CLOSE_SOCKET(client);
			continue;
		}

		auto params = ParseQueryString(query);

		/* Validate CSRF state */
		auto it_state = params.find("state");
		if (it_state == params.end() || it_state->second != state) {
			MCDN_LOG(LOG_WARNING,
				 "OAuth CSRF state mismatch — rejecting");
			SendHttpResponse(client, 403, "Forbidden", ERROR_HTML);
			CLOSE_SOCKET(client);
			continue;
		}

		/* Extract credentials */
		PluginCredentials creds;
		creds.stream_slug = params.count("slug") ? params["slug"] : "";
		creds.stream_key = params.count("key") ? params["key"] : "";

		/*
		 * SERVER URL ALLOWLIST — Defense-in-depth
		 *
		 * Even if the web frontend is fully compromised
		 * (DNS hijack, supply chain, rogue deploy), the
		 * plugin will NEVER send the user's stream to a
		 * server outside *.midcircuitcdn.com.
		 *
		 * Any unrecognized URL is silently replaced with
		 * the hardcoded default.
		 */
		std::string raw_url = params.count("url")
					      ? params["url"]
					      : "";
		if (!raw_url.empty() &&
		    raw_url.find("midcircuitcdn.com") != std::string::npos) {
			creds.server_url = raw_url;
		} else {
			if (!raw_url.empty()) {
				MCDN_LOG(LOG_WARNING,
					 "Rejected untrusted server_url: %s "
					 "— using default",
					 raw_url.c_str());
			}
			creds.server_url = MCDN_DEFAULT_SERVER_URL;
		}

		if (creds.stream_key.empty()) {
			MCDN_LOG(LOG_WARNING,
				 "OAuth received redirect but stream_key was empty");
			SendHttpResponse(client, 400, "Bad Request",
					 "<h1>Missing stream key</h1>");
			CLOSE_SOCKET(client);
			continue;
		}

		/* Serve success page */
		SendHttpResponse(client, 200, "OK", SUCCESS_HTML);
		CLOSE_SOCKET(client);

		MCDN_LOG(LOG_INFO,
			 "OAuth success: slug=%s, key=[%zu chars], url=%s",
			 creds.stream_slug.c_str(), creds.stream_key.size(),
			 creds.server_url.c_str());

		/* Fire callback */
		if (on_success)
			on_success(creds);

		success = true;
	}

	CLOSE_SOCKET(server_sock);

#ifdef _WIN32
	WSACleanup();
#endif

	g_oauth_active = false;
	MCDN_LOG(LOG_INFO, "OAuth loopback thread exiting (success=%s)",
		 success ? "true" : "false");
}

/* ── Public API ───────────────────────────────────────────────────────── */

void StartOAuthFlow(OAuthCallback on_success, int timeout_seconds)
{
	std::lock_guard<std::mutex> lock(g_oauth_mutex);

	if (g_oauth_active) {
		MCDN_LOG(LOG_WARNING,
			 "OAuth flow already in progress — ignoring");
		return;
	}

	g_oauth_active = true;
	g_oauth_cancel = false;

	/* Detach existing thread if joinable */
	if (g_oauth_thread.joinable())
		g_oauth_thread.join();

	g_oauth_thread = std::thread(OAuthThreadFunc, on_success,
				     timeout_seconds);
	g_oauth_thread.detach();
}

void CancelOAuthFlow()
{
	g_oauth_cancel = true;
}

bool IsOAuthFlowActive()
{
	return g_oauth_active;
}
