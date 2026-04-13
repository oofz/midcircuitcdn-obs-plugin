/*
 * MidcircuitCDN OBS Plugin — Update Checker
 * ────────────────────────────────────────────────────────────────────────────
 * Checks GitHub releases API for newer plugin versions.
 * Emits a Qt signal when a newer version is detected.
 *
 * Uses QNetworkAccessManager (non-blocking, runs on Qt event loop).
 * Requires Qt6 — gated behind HAVE_QT.
 */

#pragma once

#ifdef HAVE_QT

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class UpdateChecker : public QObject {
	Q_OBJECT

public:
	explicit UpdateChecker(QObject *parent = nullptr);
	~UpdateChecker() override = default;

	/* Fire a check now — results arrive asynchronously via signal */
	void CheckForUpdate();

	/* Accessors */
	bool HasUpdate() const { return m_hasUpdate; }
	QString LatestVersion() const { return m_latestVersion; }
	QString DownloadUrl() const { return m_downloadUrl; }

signals:
	void UpdateAvailable(const QString &version,
			     const QString &downloadUrl);

private slots:
	void OnReplyFinished(QNetworkReply *reply);

private:
	QNetworkAccessManager *m_manager = nullptr;
	bool m_hasUpdate = false;
	QString m_latestVersion;
	QString m_downloadUrl;

	static bool IsNewer(const QString &remote, const QString &local);
};

#endif /* HAVE_QT */
