/*
 * MidcircuitCDN OBS Plugin — Update Checker (Implementation)
 * ────────────────────────────────────────────────────────────────────────────
 * Hits: GET https://api.github.com/repos/oofz/midcircuitcdn-obs-plugin/releases/latest
 * Parses: tag_name (e.g. "v0.2.1") and assets[0].browser_download_url
 * Compares against compiled-in PLUGIN_VERSION using semantic versioning.
 */

#ifdef HAVE_QT

#include "update_checker.hpp"
#include "plugin-macros.h"

#include <obs-module.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QNetworkRequest>

static const char *GITHUB_API_URL =
	"https://api.github.com/repos/oofz/midcircuitcdn-obs-plugin"
	"/releases/latest";

/* ── Constructor ──────────────────────────────────────────────────────── */

UpdateChecker::UpdateChecker(QObject *parent) : QObject(parent)
{
	m_manager = new QNetworkAccessManager(this);
	connect(m_manager, &QNetworkAccessManager::finished, this,
		&UpdateChecker::OnReplyFinished);
}

/* ── Fire the check ───────────────────────────────────────────────────── */

void UpdateChecker::CheckForUpdate()
{
	QUrl url(QString::fromLatin1(GITHUB_API_URL));
	QNetworkRequest req(url);
	req.setHeader(QNetworkRequest::UserAgentHeader,
		      QString::fromLatin1("MidcircuitCDN-OBS-Plugin/" PLUGIN_VERSION));
	req.setRawHeader("Accept", "application/vnd.github+json");
	m_manager->get(req);

	MCDN_LOG(LOG_INFO, "Update check started (current: v%s)",
		 PLUGIN_VERSION);
}

/* ── Handle the response ──────────────────────────────────────────────── */

void UpdateChecker::OnReplyFinished(QNetworkReply *reply)
{
	reply->deleteLater();

	if (reply->error() != QNetworkReply::NoError) {
		MCDN_LOG(LOG_WARNING, "Update check failed: %s",
			 reply->errorString().toUtf8().constData());
		return;
	}

	QByteArray data = reply->readAll();
	QJsonDocument doc = QJsonDocument::fromJson(data);
	if (!doc.isObject()) {
		MCDN_LOG(LOG_WARNING,
			 "Update check: invalid JSON response");
		return;
	}

	QJsonObject obj = doc.object();

	/* Extract version from tag_name (strip leading "v") */
	QString tagName = obj["tag_name"].toString();
	QString remoteVersion = tagName;
	if (remoteVersion.startsWith("v") || remoteVersion.startsWith("V"))
		remoteVersion = remoteVersion.mid(1);

	/* Extract download URL from first asset, or fallback to html_url */
	QString downloadUrl = obj["html_url"].toString();
	QJsonArray assets = obj["assets"].toArray();
	if (!assets.isEmpty()) {
		QJsonObject firstAsset = assets[0].toObject();
		QString assetUrl =
			firstAsset["browser_download_url"].toString();
		if (!assetUrl.isEmpty())
			downloadUrl = assetUrl;
	}

	/* Compare versions */
	QString localVersion = PLUGIN_VERSION;
	if (IsNewer(remoteVersion, localVersion)) {
		m_hasUpdate = true;
		m_latestVersion = remoteVersion;
		m_downloadUrl = downloadUrl;

		MCDN_LOG(LOG_INFO,
			 "Update available: v%s → v%s",
			 PLUGIN_VERSION,
			 remoteVersion.toUtf8().constData());

		emit UpdateAvailable(remoteVersion, downloadUrl);
	} else {
		MCDN_LOG(LOG_INFO, "Plugin is up to date (v%s)",
			 PLUGIN_VERSION);
	}
}

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
