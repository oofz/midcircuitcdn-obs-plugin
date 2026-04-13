/*
 * MidcircuitCDN OBS Plugin — Control Panel Dock Widget (Header)
 * ────────────────────────────────────────────────────────────────────────────
 * A native Qt dock panel that appears in OBS just like the Controls or
 * Scene Transitions panel. Provides 1-click Connect, multistream config,
 * and a destination banner showing where the user is streaming to.
 *
 * Requires Qt6 — gated behind HAVE_QT at compile time.
 */

#pragma once

#ifdef HAVE_QT

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QClipboard>
#include <QApplication>

#include "../multistream/multistream_output.hpp"

class UpdateChecker;

class McdnControlPanel : public QWidget {
	Q_OBJECT

public:
	explicit McdnControlPanel(QWidget *parent = nullptr);
	~McdnControlPanel() override;

	/* Update the panel to reflect current connection state */
	void UpdateState();

	/* Called by plugin-main when streaming starts/stops */
	void OnStreamingStarted();
	void OnStreamingStopped();

private slots:
	void OnConnectClicked();
	void OnDisconnectClicked();
	void OnMultistreamClicked();

private:
	void BuildConnectedUI();
	void BuildDisconnectedUI();
	void BuildDestinationBanner();
	void HideDestinationBanner();
	void ClearLayout();

	QVBoxLayout *m_mainLayout = nullptr;
	QPushButton *m_connectBtn = nullptr;
	QPushButton *m_disconnectBtn = nullptr;
	QPushButton *m_multistreamBtn = nullptr;
	QLabel *m_statusLabel = nullptr;
	QLabel *m_slugLabel = nullptr;
	QWidget *m_destinationBanner = nullptr;
	QLabel *m_versionLabel = nullptr;
	QWidget *m_updateBanner = nullptr;

	MultistreamOutputManager *m_outputMgr = nullptr;
	UpdateChecker *m_updateChecker = nullptr;

	void OnUpdateAvailable(const QString &version,
			       const QString &downloadUrl);
};

/* Register / Unregister the dock with OBS */
void RegisterControlDock();
void UnregisterControlDock();

/* Get the global control panel pointer (for plugin-main event hooks) */
McdnControlPanel *GetControlPanel();

#else

/* Stubs when Qt is not available */
static inline void RegisterControlDock() {}
static inline void UnregisterControlDock() {}

#endif /* HAVE_QT */
