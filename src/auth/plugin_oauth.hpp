/*
 * MidCircuitCDN OBS Plugin — OAuth Loopback Server
 * ────────────────────────────────────────────────────────────────────────────
 * Manages the local HTTP loopback for Discord OAuth credential handoff.
 *
 * Flow:
 *   1. Plugin binds an ephemeral port and generates a CSRF `state` token.
 *   2. Opens browser to: MidCircuitCDN.com/login?plugin_auth_port=PORT&state=STATE
 *   3. User completes Discord OAuth on the Next.js frontend.
 *   4. Next.js redirects to: http://localhost:PORT/auth?slug=...&key=...&state=STATE
 *   5. This server validates `state`, extracts credentials, serves success page.
 */

#pragma once

#include <string>
#include <functional>

struct PluginCredentials {
	std::string stream_slug;
	std::string stream_key;
	std::string server_url;
};

/*
 * Callback type fired when credentials are successfully received.
 */
using OAuthCallback = std::function<void(const PluginCredentials &creds)>;

/*
 * Start the full OAuth flow:
 *   - Binds a local loopback server to an ephemeral port.
 *   - Generates a cryptographic state token.
 *   - Opens the user's default browser to the MidCircuitCDN login page.
 *   - Waits for the redirect (up to timeout_seconds).
 *   - Validates the state token.
 *   - Calls `on_success` with the parsed credentials.
 *
 * This function is non-blocking; the loopback runs on a background thread.
 */
void StartOAuthFlow(OAuthCallback on_success, int timeout_seconds = 300);

/*
 * Cancel any in-progress OAuth flow and shut down the loopback server.
 */
void CancelOAuthFlow();

/*
 * Returns true if the loopback server is currently listening.
 */
bool IsOAuthFlowActive();
