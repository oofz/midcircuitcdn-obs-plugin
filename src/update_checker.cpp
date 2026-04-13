/*
 * MidCircuitCDN OBS Plugin — Update Checker (Implementation)
 * ────────────────────────────────────────────────────────────────────────────
 * Hits: GET https://api.github.com/repos/oofz/MidCircuitCDN-obs-plugin/releases/latest
 * Parses: tag_name (e.g. "v0.2.1") and assets[0].browser_download_url
 * Compares against compiled-in PLUGIN_VERSION using semantic versioning.
 *
 * Uses WinHTTP on a background thread (no Qt SSL/TLS dependency needed).
 * Falls back gracefully if the network is unavailable.
 */

#ifdef HAVE_QT

#include "update_checker.hpp"
#include "plugin-macros.h"

#include <obs-module.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

static const wchar_t *GITHUB_HOST = L"api.github.com";
static const wchar_t *GITHUB_PATH =
	L"/repos/oofz/MidCircuitCDN-obs-plugin/releases/latest";

/* ── Constructor ──────────────────────────────────────────────────────── */

UpdateChecker::UpdateChecker(QObject *parent) : QObject(parent)
{
}

/* ── Fire the check (runs network on background thread) ───────────────── */

void UpdateChecker::CheckForUpdate()
{
	MCDN_LOG(LOG_INFO, "Update check started (current: v%s)",
		 PLUGIN_VERSION);

#ifdef _WIN32
	/* Run on a background thread to avoid blocking the UI */
	QThread *thread = QThread::create([this]() {
		QByteArray data = FetchReleaseJson();
		if (data.isEmpty())
			return;

		/* Parse JSON on the background thread */
		QJsonDocument doc = QJsonDocument::fromJson(data);
		if (!doc.isObject()) {
			MCDN_LOG(LOG_WARNING,
				 "Update check: invalid JSON response");
			return;
		}

		QJsonObject obj = doc.object();

		QString tagName = obj["tag_name"].toString();
		QString remoteVersion = tagName;
		if (remoteVersion.startsWith("v") ||
		    remoteVersion.startsWith("V"))
			remoteVersion = remoteVersion.mid(1);

		QString downloadUrl = obj["html_url"].toString();
		QJsonArray assets = obj["assets"].toArray();
		if (!assets.isEmpty()) {
			QJsonObject firstAsset = assets[0].toObject();
			QString assetUrl =
				firstAsset["browser_download_url"].toString();
			if (!assetUrl.isEmpty())
				downloadUrl = assetUrl;
		}

		QString localVersion = PLUGIN_VERSION;
		if (IsNewer(remoteVersion, localVersion)) {
			m_hasUpdate = true;
			m_latestVersion = remoteVersion;
			m_downloadUrl = downloadUrl;

			MCDN_LOG(LOG_INFO, "Update available: v%s -> v%s",
				 PLUGIN_VERSION,
				 remoteVersion.toUtf8().constData());

			/* Emit on the main thread via queued connection */
			QMetaObject::invokeMethod(
				this,
				[this, remoteVersion, downloadUrl]() {
					emit UpdateAvailable(remoteVersion,
							     downloadUrl);
				},
				Qt::QueuedConnection);
		} else {
			MCDN_LOG(LOG_INFO, "Plugin is up to date (v%s)",
				 PLUGIN_VERSION);
		}
	});

	connect(thread, &QThread::finished, thread, &QThread::deleteLater);
	thread->start();
#else
	MCDN_LOG(LOG_INFO, "Update check not supported on this platform");
#endif
}

/* ── WinHTTP-based fetch (no Qt SSL needed) ───────────────────────────── */

#ifdef _WIN32
QByteArray UpdateChecker::FetchReleaseJson()
{
	QByteArray result;

	HINTERNET hSession = WinHttpOpen(
		L"MidCircuitCDN-OBS-Plugin", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession) {
		MCDN_LOG(LOG_WARNING, "Update check: WinHttpOpen failed");
		return result;
	}

	HINTERNET hConnect = WinHttpConnect(hSession, GITHUB_HOST,
					    INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (!hConnect) {
		MCDN_LOG(LOG_WARNING, "Update check: WinHttpConnect failed");
		WinHttpCloseHandle(hSession);
		return result;
	}

	HINTERNET hRequest = WinHttpOpenRequest(
		hConnect, L"GET", GITHUB_PATH, NULL, WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
	if (!hRequest) {
		MCDN_LOG(LOG_WARNING,
			 "Update check: WinHttpOpenRequest failed");
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return result;
	}

	/* Add headers */
	WinHttpAddRequestHeaders(
		hRequest,
		L"Accept: application/vnd.github+json\r\n"
		L"User-Agent: MidCircuitCDN-OBS-Plugin\r\n",
		(DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);

	BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS,
				       0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
	if (!sent) {
		MCDN_LOG(LOG_WARNING,
			 "Update check: WinHttpSendRequest failed");
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return result;
	}

	BOOL received = WinHttpReceiveResponse(hRequest, NULL);
	if (!received) {
		MCDN_LOG(LOG_WARNING,
			 "Update check: WinHttpReceiveResponse failed");
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return result;
	}

	/* Read response body */
	DWORD bytesAvailable = 0;
	do {
		WinHttpQueryDataAvailable(hRequest, &bytesAvailable);
		if (bytesAvailable == 0)
			break;

		QByteArray chunk(bytesAvailable, 0);
		DWORD bytesRead = 0;
		WinHttpReadData(hRequest, chunk.data(), bytesAvailable,
				&bytesRead);
		chunk.resize(bytesRead);
		result.append(chunk);
	} while (bytesAvailable > 0);

	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hConnect);
	WinHttpCloseHandle(hSession);

	return result;
}
#endif

/* ── Semantic version comparison ──────────────────────────────────────── */

bool UpdateChecker::IsNewer(const QString &remote, const QString &local)
{
	QStringList remoteParts = remote.split(".");
	QStringList localParts = local.split(".");

	int maxParts = qMax(remoteParts.size(), localParts.size());
	for (int i = 0; i < maxParts; i++) {
		int r = (i < remoteParts.size()) ? remoteParts[i].toInt() : 0;
		int l = (i < localParts.size()) ? localParts[i].toInt() : 0;
		if (r > l)
			return true;
		if (r < l)
			return false;
	}
	return false; /* equal */
}

#endif /* HAVE_QT */
