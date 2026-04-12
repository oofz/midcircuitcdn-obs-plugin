/*
 * MidcircuitCDN OBS Plugin — Control Panel Dock Widget (Header)
 * ────────────────────────────────────────────────────────────────────────────
 * A native Qt dock panel that appears in OBS just like the Controls or
 * Scene Transitions panel. Provides 1-click Connect and a gear icon
 * for settings.
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

class McdnControlPanel : public QWidget {
	Q_OBJECT

public:
	explicit McdnControlPanel(QWidget *parent = nullptr);
	~McdnControlPanel() override = default;

	/* Update the panel to reflect current connection state */
	void UpdateState();

private slots:
	void OnConnectClicked();
	void OnDisconnectClicked();

private:
	void BuildConnectedUI();
	void BuildDisconnectedUI();
	void ClearLayout();

	QVBoxLayout *m_mainLayout = nullptr;
	QPushButton *m_connectBtn = nullptr;
	QPushButton *m_disconnectBtn = nullptr;
	QLabel *m_statusLabel = nullptr;
	QLabel *m_slugLabel = nullptr;
};

/* Register / Unregister the dock with OBS */
void RegisterControlDock();
void UnregisterControlDock();

#else

/* Stubs when Qt is not available */
static inline void RegisterControlDock() {}
static inline void UnregisterControlDock() {}

#endif /* HAVE_QT */
