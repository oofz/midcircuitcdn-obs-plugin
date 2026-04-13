/*
 * MidCircuitCDN OBS Plugin — Update Checker
 * ────────────────────────────────────────────────────────────────────────────
 * Checks GitHub releases API for newer plugin versions.
 * Uses WinHTTP (Windows native TLS — no Qt SSL plugin needed).
 * Emits a Qt signal when a newer version is detected.
 */

#pragma once

#ifdef HAVE_QT

#include <QObject>
#include <QString>
#include <QByteArray>

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

private:
	bool m_hasUpdate = false;
	QString m_latestVersion;
	QString m_downloadUrl;

	static bool IsNewer(const QString &remote, const QString &local);

#ifdef _WIN32
	static QByteArray FetchReleaseJson();
#endif
};

#endif /* HAVE_QT */
