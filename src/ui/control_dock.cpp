/*
 * MidcircuitCDN OBS Plugin — Control Panel Dock Widget (Implementation)
 * ────────────────────────────────────────────────────────────────────────────
 * A native Qt dock panel that appears in OBS like the Controls panel.
 * Shows a 1-click Connect button when disconnected, and status + gear
 * icon when connected. Includes multistream setup and destination banner.
 *
 * Uses obs_frontend_add_dock_by_id() (OBS 30+) to register as a
 * first-class draggable/dockable panel.
 */

#ifdef HAVE_QT

#include "control_dock.hpp"
#include "multistream_dialog.hpp"
#include "../plugin-macros.h"
#include "../auth/plugin_oauth.hpp"
#include "../auth/plugin_store.hpp"
#include "../obs_integration/stream_config.hpp"
#include "../obs_integration/encoder_config.hpp"
#include "../multistream/multistream_store.hpp"
#include "../update_checker.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <QStyle>
#include <QFont>
#include <QIcon>
#include <QApplication>
#include <QTimer>
#include <QGroupBox>
#include <QDockWidget>
#include <QDesktopServices>
#include <QUrl>
#include <QPixmap>

/* ── Dark-theme stylesheet matching OBS's look ───────────────────────── */
static const char *PANEL_STYLE = R"(
    QWidget#McdnControlPanel {
        background-color: #1e1e2e;
        border: 1px solid #313244;
        border-radius: 6px;
    }
    QPushButton#connectBtn {
        background-color: #7c3aed;
        color: #ffffff;
        border: none;
        border-radius: 4px;
        padding: 8px 16px;
        font-weight: bold;
        font-size: 13px;
    }
    QPushButton#connectBtn:hover {
        background-color: #8b5cf6;
    }
    QPushButton#connectBtn:pressed {
        background-color: #6d28d9;
    }
    QPushButton#logoutBtn {
        background-color: transparent;
        color: #6c7086;
        border: none;
        padding: 2px 6px;
        font-size: 10px;
    }
    QPushButton#logoutBtn:hover {
        color: #a6adc8;
        text-decoration: underline;
    }
    QPushButton#multistreamBtn {
        background-color: #313244;
        color: #cdd6f4;
        border: 1px solid #45475a;
        border-radius: 4px;
        padding: 6px 14px;
        font-size: 12px;
        font-weight: bold;
    }
    QPushButton#multistreamBtn:hover {
        background-color: #45475a;
        border-color: #7c3aed;
        color: #cba6f7;
    }
    QLabel#statusLabel {
        color: #a6e3a1;
        font-weight: bold;
        font-size: 12px;
    }
    QLabel#statusLabelDisconnected {
        color: #6c7086;
        font-size: 11px;
    }
    QLabel#slugLabel {
        color: #cdd6f4;
        font-size: 12px;
    }
    QLabel#titleLabel {
        color: #cba6f7;
        font-weight: bold;
        font-size: 13px;
    }
    QLabel#urlSectionLabel {
        color: #89b4fa;
        font-size: 11px;
        font-weight: bold;
        margin-top: 4px;
    }
    QLabel#urlLabel {
        color: #a6adc8;
        font-size: 10px;
        background: #181825;
        border: 1px solid #313244;
        border-radius: 4px;
        padding: 4px 6px;
    }
    QPushButton#copyBtn {
        background: #313244;
        color: #cdd6f4;
        border: 1px solid #45475a;
        border-radius: 4px;
        padding: 3px 8px;
        font-size: 10px;
        min-width: 40px;
    }
    QPushButton#copyBtn:hover {
        background: #45475a;
        border-color: #585b70;
    }
    QWidget#destinationBanner {
        background: #181825;
        border: 1px solid #313244;
        border-radius: 4px;
        padding: 6px;
    }
    QLabel#bannerTitle {
        color: #89b4fa;
        font-weight: bold;
        font-size: 11px;
    }
    QLabel#destActive {
        color: #a6e3a1;
        font-size: 11px;
    }
    QLabel#destInactive {
        color: #f38ba8;
        font-size: 11px;
    }
    QLabel#versionLabel {
        color: #585b70;
        font-size: 9px;
        padding: 2px 0px;
    }
    QWidget#updateBanner {
        background: #7c3aed;
        border-radius: 4px;
        padding: 6px 10px;
    }
    QLabel#updateText {
        color: #ffffff;
        font-size: 11px;
        font-weight: bold;
    }
)";

/* ── Constructor ──────────────────────────────────────────────────────── */
McdnControlPanel::McdnControlPanel(QWidget *parent) : QWidget(parent)
{
	setObjectName("McdnControlPanel");
	setStyleSheet(PANEL_STYLE);
	setMinimumWidth(200);

	m_mainLayout = new QVBoxLayout(this);
	m_mainLayout->setContentsMargins(10, 8, 10, 8);
	m_mainLayout->setSpacing(6);

	/* Title row */
	auto *titleLabel = new QLabel("MidcircuitCDN", this);
	titleLabel->setObjectName("titleLabel");
	titleLabel->setAlignment(Qt::AlignCenter);
	m_mainLayout->addWidget(titleLabel);

	/* Create output manager */
	m_outputMgr = new MultistreamOutputManager();

	/* Version label (bottom-right, added after UpdateState populates) */
	m_versionLabel = new QLabel(
		QString("v%1").arg(PLUGIN_VERSION), this);
	m_versionLabel->setObjectName("versionLabel");
	m_versionLabel->setAlignment(Qt::AlignRight);

	/* Start update checker */
	m_updateChecker = new UpdateChecker(this);
	connect(m_updateChecker, &UpdateChecker::UpdateAvailable, this,
		&McdnControlPanel::OnUpdateAvailable);
	m_updateChecker->CheckForUpdate();

	UpdateState();
}

/* ── Destructor ──────────────────────────────────────────────────────── */
McdnControlPanel::~McdnControlPanel()
{
	if (m_outputMgr) {
		m_outputMgr->StopAllOutputs();
		delete m_outputMgr;
		m_outputMgr = nullptr;
	}
}

/* ── Update UI based on connection state ──────────────────────────────── */
void McdnControlPanel::UpdateState()
{
	ClearLayout();

	PluginCredentials creds;
	bool connected = HasStoredCredentials() && LoadCredentials(creds) &&
			 !creds.stream_key.empty();

	if (connected) {
		BuildConnectedUI();
	} else {
		BuildDisconnectedUI();
	}
}

/* ── Build the "connected" UI ─────────────────────────────────────────── */
void McdnControlPanel::BuildConnectedUI()
{
	PluginCredentials creds;
	LoadCredentials(creds);

	/* Status indicator */
	m_statusLabel = new QLabel(QString::fromUtf8("\xe2\x97\x89 Connected"),
				   this);
	m_statusLabel->setObjectName("statusLabel");
	m_statusLabel->setAlignment(Qt::AlignCenter);
	m_mainLayout->addWidget(m_statusLabel);

	/* Stream slug + Log Out row */
	if (!creds.stream_slug.empty()) {
		auto *slugRow = new QHBoxLayout();
		slugRow->setSpacing(4);
		slugRow->addStretch(1);

		m_slugLabel = new QLabel(
			QString::fromStdString(creds.stream_slug), this);
		m_slugLabel->setObjectName("slugLabel");
		slugRow->addWidget(m_slugLabel);

		m_disconnectBtn = new QPushButton("Log Out", this);
		m_disconnectBtn->setObjectName("logoutBtn");
		connect(m_disconnectBtn, &QPushButton::clicked, this,
			&McdnControlPanel::OnDisconnectClicked);
		slugRow->addWidget(m_disconnectBtn);

		slugRow->addStretch(1);
		m_mainLayout->addLayout(slugRow);
	}

	/* ── Stream URL section ─────────────────────────────────────── */
	if (!creds.stream_slug.empty()) {
		QString slug = QString::fromStdString(creds.stream_slug);

		/* Low Latency RTSP */
		auto *rtspTitle = new QLabel("Low Latency RTSP", this);
		rtspTitle->setObjectName("urlSectionLabel");
		m_mainLayout->addWidget(rtspTitle);

		QString rtspUrl = QString("rtspt://stream.midcircuitcdn.com/live/%1").arg(slug);
		auto *rtspRow = new QHBoxLayout();
		rtspRow->setSpacing(4);
		auto *rtspLabel = new QLabel(rtspUrl, this);
		rtspLabel->setObjectName("urlLabel");
		rtspLabel->setWordWrap(true);
		rtspLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
		rtspRow->addWidget(rtspLabel, 1);

		auto *rtspCopy = new QPushButton("Copy", this);
		rtspCopy->setObjectName("copyBtn");
		connect(rtspCopy, &QPushButton::clicked, this, [rtspUrl]() {
			QApplication::clipboard()->setText(rtspUrl);
		});
		rtspRow->addWidget(rtspCopy);
		m_mainLayout->addLayout(rtspRow);

		/* Quest Compatible HLS */
		auto *hlsTitle = new QLabel("Quest Compatible", this);
		hlsTitle->setObjectName("urlSectionLabel");
		m_mainLayout->addWidget(hlsTitle);

		QString hlsUrl = QString("https://stream.midcircuitcdn.com/live/%1/index.m3u8").arg(slug);
		auto *hlsRow = new QHBoxLayout();
		hlsRow->setSpacing(4);
		auto *hlsLabel = new QLabel(hlsUrl, this);
		hlsLabel->setObjectName("urlLabel");
		hlsLabel->setWordWrap(true);
		hlsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
		hlsRow->addWidget(hlsLabel, 1);

		auto *hlsCopy = new QPushButton("Copy", this);
		hlsCopy->setObjectName("copyBtn");
		connect(hlsCopy, &QPushButton::clicked, this, [hlsUrl]() {
			QApplication::clipboard()->setText(hlsUrl);
		});
		hlsRow->addWidget(hlsCopy);
		m_mainLayout->addLayout(hlsRow);
	}

	/* ── Multistream button ────────────────────────────────────── */
	m_multistreamBtn = new QPushButton(
		QString::fromUtf8("\xe2\x86\x94 Multistream"), this);
	m_multistreamBtn->setObjectName("multistreamBtn");
	connect(m_multistreamBtn, &QPushButton::clicked, this,
		&McdnControlPanel::OnMultistreamClicked);

	/* Show indicator if multistream is active */
	if (HasAnyMultistreamEnabled()) {
		auto enabled = GetEnabledTargets();
		QString label = QString::fromUtf8("\xe2\x86\x94 Multistream (%1)")
					.arg(enabled.size());
		m_multistreamBtn->setText(label);
	}

	m_mainLayout->addWidget(m_multistreamBtn);

	/* ── Destination banner (shown while streaming) ────────────── */
	/* placeholder — will be populated by OnStreamingStarted() */

	/* Log Out button is now inline with the slug row above */

	/* ── Spacer + version label at bottom ──────────────────────── */
	m_mainLayout->addStretch(1);
	if (m_updateBanner)
		m_mainLayout->addWidget(m_updateBanner);
	m_mainLayout->addWidget(m_versionLabel);
}

/* ── Build the "disconnected" UI ──────────────────────────────────────── */
void McdnControlPanel::BuildDisconnectedUI()
{
	m_statusLabel = new QLabel("Not connected", this);
	m_statusLabel->setObjectName("statusLabelDisconnected");
	m_statusLabel->setAlignment(Qt::AlignCenter);
	m_mainLayout->addWidget(m_statusLabel);

	m_connectBtn =
		new QPushButton(QString::fromUtf8("\xe2\x96\xb6 Connect"),
				this);
	m_connectBtn->setObjectName("connectBtn");
	connect(m_connectBtn, &QPushButton::clicked, this,
		&McdnControlPanel::OnConnectClicked);
	m_mainLayout->addWidget(m_connectBtn);

	/* ── Spacer + version label at bottom ──────────────────────── */
	m_mainLayout->addStretch(1);
	if (m_updateBanner)
		m_mainLayout->addWidget(m_updateBanner);
	m_mainLayout->addWidget(m_versionLabel);
}

/* ── Update Available Handler ─────────────────────────────────────────── */
void McdnControlPanel::OnUpdateAvailable(const QString &version,
					 const QString &downloadUrl)
{
	if (m_updateBanner)
		return; /* already shown */

	/* Hide the plain version label — we're replacing it */
	if (m_versionLabel)
		m_versionLabel->setVisible(false);

	/* Build a full-width purple bar that replaces the version row */
	auto *bar = new QPushButton(this);
	bar->setObjectName("updateBar");
	bar->setCursor(Qt::PointingHandCursor);
	bar->setText(QString("v%1   |   v%2 available — Download")
			     .arg(PLUGIN_VERSION)
			     .arg(version));
	bar->setStyleSheet(
		"QPushButton#updateBar {"
		"  background: #7c3aed;"
		"  color: #ffffff;"
		"  border: none;"
		"  border-radius: 0px;"
		"  padding: 4px 8px;"
		"  font-size: 10px;"
		"  font-weight: bold;"
		"}"
		"QPushButton#updateBar:hover {"
		"  background: #6d28d9;"
		"}");

	QString url = downloadUrl;
	connect(bar, &QPushButton::clicked, this, [url]() {
		QDesktopServices::openUrl(QUrl(url));
	});

	m_updateBanner = bar;
	m_mainLayout->addWidget(m_updateBanner);

	/* Remove bottom margin so the bar goes edge-to-edge */
	m_mainLayout->setContentsMargins(
		m_mainLayout->contentsMargins().left(),
		m_mainLayout->contentsMargins().top(),
		m_mainLayout->contentsMargins().right(), 0);
}

/* ── Clear dynamic widgets (keep title) ───────────────────────────────── */
void McdnControlPanel::ClearLayout()
{
	/*
	 * Remove version label and update banner from layout
	 * before clearing — we reuse them across state changes.
	 */
	if (m_versionLabel)
		m_mainLayout->removeWidget(m_versionLabel);
	if (m_updateBanner)
		m_mainLayout->removeWidget(m_updateBanner);

	/* Remove everything except the title (item 0) */
	while (m_mainLayout->count() > 1) {
		auto *item = m_mainLayout->takeAt(1);
		if (QWidget *w = item->widget()) {
			/* Don't delete preserved widgets */
			if (w == m_versionLabel || w == m_updateBanner)
				continue;
			w->deleteLater();
		} else if (QLayout *childLayout = item->layout()) {
			/* Drain child widgets from the sub-layout */
			while (childLayout->count() > 0) {
				auto *sub = childLayout->takeAt(0);
				if (QWidget *sw = sub->widget())
					sw->deleteLater();
				delete sub;
			}
			/*
			 * DO NOT delete childLayout separately here!
			 * QLayout inherits QLayoutItem, so childLayout == item.
			 * The `delete item` below handles it.
			 */
		}
		delete item;
	}
	m_connectBtn = nullptr;
	m_disconnectBtn = nullptr;
	m_multistreamBtn = nullptr;
	m_statusLabel = nullptr;
	m_slugLabel = nullptr;
	m_destinationBanner = nullptr;
}

/* ── Build / Hide destination banner ──────────────────────────────────── */

void McdnControlPanel::BuildDestinationBanner()
{
	HideDestinationBanner();

	auto enabledTargets = GetEnabledTargets();

	/* Only show banner if multistream is active or always show MCDN */
	m_destinationBanner = new QWidget(this);
	m_destinationBanner->setObjectName("destinationBanner");
	auto *bannerLayout = new QVBoxLayout(m_destinationBanner);
	bannerLayout->setContentsMargins(8, 6, 8, 6);
	bannerLayout->setSpacing(3);

	auto *bannerTitle = new QLabel("Streaming to:", m_destinationBanner);
	bannerTitle->setObjectName("bannerTitle");
	bannerLayout->addWidget(bannerTitle);

	/* MidcircuitCDN row (always active when streaming) */
	auto *mcdnRow =
		new QLabel(QString::fromUtf8("\xe2\x97\x89 MidcircuitCDN"),
			   m_destinationBanner);
	mcdnRow->setObjectName("destActive");
	bannerLayout->addWidget(mcdnRow);

	/* Multistream target rows */
	for (const auto &t : enabledTargets) {
		auto *row = new QLabel(
			QString::fromUtf8("\xe2\x97\x89 ") +
				QString::fromStdString(t.display_name),
			m_destinationBanner);
		row->setObjectName("destActive");
		bannerLayout->addWidget(row);
	}

	/* Insert banner before the disconnect button
	 * (disconnect is the last widget) */
	int insertIdx = m_mainLayout->count() - 1;
	if (insertIdx < 1)
		insertIdx = m_mainLayout->count();
	m_mainLayout->insertWidget(insertIdx, m_destinationBanner);
}

void McdnControlPanel::HideDestinationBanner()
{
	if (m_destinationBanner) {
		m_mainLayout->removeWidget(m_destinationBanner);
		m_destinationBanner->deleteLater();
		m_destinationBanner = nullptr;
	}
}

/* ── Streaming event handlers ─────────────────────────────────────────── */

void McdnControlPanel::OnStreamingStarted()
{
	MCDN_LOG(LOG_INFO, "Control dock: streaming started — "
			   "starting multistream outputs");

	if (m_outputMgr && HasAnyMultistreamEnabled()) {
		m_outputMgr->StartAllOutputs();
	}

	BuildDestinationBanner();
}

void McdnControlPanel::OnStreamingStopped()
{
	MCDN_LOG(LOG_INFO, "Control dock: streaming stopped — "
			   "stopping multistream outputs");

	if (m_outputMgr) {
		m_outputMgr->StopAllOutputs();
	}

	HideDestinationBanner();
}

/* ── Slot: Connect clicked ────────────────────────────────────────────── */
void McdnControlPanel::OnConnectClicked()
{
	if (IsOAuthFlowActive()) {
		MCDN_LOG(LOG_INFO,
			 "OAuth flow already active — ignoring click");
		return;
	}

	MCDN_LOG(LOG_INFO,
		 "Control dock: starting MidcircuitCDN OAuth flow...");

	/* Start OAuth — on success, update the panel */
	StartOAuthFlow([this](const PluginCredentials &creds) {
		MCDN_LOG(LOG_INFO, "Control dock: credentials received for %s",
			 creds.stream_slug.c_str());

		/*
		 * IMPORTANT: OBS frontend API functions are NOT thread-safe.
		 * This callback runs on the OAuth background thread, so we
		 * must defer ALL OBS API calls to the main Qt thread.
		 * We capture creds by value to safely cross thread boundary.
		 */
		QMetaObject::invokeMethod(
			this,
			[this, creds]() {
				SaveCredentials(creds);

				/*
				 * Build the RTMP stream key in the format
				 * required by MidcircuitCDN's ingest:
				 *   {slug}?key={raw_key}
				 * e.g. heist?key=live_01e0181d7cd2ca20e98e09d2
				 */
				std::string full_key =
					creds.stream_slug + "?key=" +
					creds.stream_key;

				ApplyStreamSettings(creds.server_url,
						    full_key);
				CapVideoBitrate(MCDN_TARGET_BITRATE_KBPS);
				UpdateState();
			},
			Qt::QueuedConnection);
	});
}

/* ── Slot: Disconnect clicked ─────────────────────────────────────────── */
void McdnControlPanel::OnDisconnectClicked()
{
	MCDN_LOG(LOG_INFO, "Control dock: disconnecting...");
	ClearCredentials();

	/* NOTE: Multistream keys are intentionally NOT cleared here.
	 * They persist across MidcircuitCDN connect/disconnect cycles. */

	/*
	 * IMPORTANT: We cannot call UpdateState() directly here.
	 * UpdateState → ClearLayout deletes m_disconnectBtn, which is the
	 * widget whose clicked() signal we are currently handling.
	 * Deleting the sender during its own signal dispatch causes a
	 * use-after-free crash. Defer via QTimer::singleShot(0) so the
	 * signal handler fully unwinds first.
	 */
	QTimer::singleShot(0, this, [this]() { UpdateState(); });
}

/* ── Slot: Multistream button clicked ─────────────────────────────────── */
void McdnControlPanel::OnMultistreamClicked()
{
	MCDN_LOG(LOG_INFO, "Control dock: opening multistream dialog");

	MultistreamDialog dialog(this);
	if (dialog.exec() == QDialog::Accepted) {
		/* Update the button label to reflect new state */
		if (HasAnyMultistreamEnabled()) {
			auto enabled = GetEnabledTargets();
			QString label =
				QString::fromUtf8(
					"\xe2\x86\x94 Multistream (%1)")
					.arg(enabled.size());
			if (m_multistreamBtn)
				m_multistreamBtn->setText(label);
		} else {
			if (m_multistreamBtn)
				m_multistreamBtn->setText(
					QString::fromUtf8(
						"\xe2\x86\x94 Multistream"));
		}
	}
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* ── Registration with OBS ────────────────────────────────────────────── */
/* ═══════════════════════════════════════════════════════════════════════ */

static McdnControlPanel *g_panel = nullptr;

McdnControlPanel *GetControlPanel()
{
	return g_panel;
}

void RegisterControlDock()
{
	/* Create the panel widget */
	g_panel = new McdnControlPanel();

	/* Register as a dockable panel in OBS */
	bool ok = obs_frontend_add_dock_by_id("midcircuitcdn-control",
					      "MidcircuitCDN",
					      (void *)g_panel);

	if (ok) {
		MCDN_LOG(LOG_INFO,
			 "Control dock registered as native panel");

		/*
		 * Auto-show on first install.
		 *
		 * OBS restores its own dock visibility state AFTER all
		 * plugins initialize, so calling setVisible(true) now
		 * or even with QTimer::singleShot(0) gets overridden.
		 *
		 * The correct hook is OBS_FRONTEND_EVENT_FINISHED_LOADING
		 * which fires after OBS has fully restored its UI state.
		 * We register a one-shot callback here if it's the first
		 * install, then remove it immediately after showing.
		 */
		config_t *cfg = obs_frontend_get_global_config();
		if (cfg) {
			bool initialized = config_get_bool(
				cfg, "MidcircuitCDN", "dock_initialized");
			if (!initialized) {
				config_set_bool(cfg, "MidcircuitCDN",
						"dock_initialized", true);
				config_save(cfg);

				obs_frontend_add_event_callback(
					[](enum obs_frontend_event event,
					   void *) {
						if (event !=
						    OBS_FRONTEND_EVENT_FINISHED_LOADING)
							return;
						if (!g_panel)
							return;

						/* Find the QDockWidget wrapper
						 * that OBS created around our
						 * panel widget */
						QWidget *w = g_panel;
						while (w) {
							auto *dock = qobject_cast<QDockWidget *>(w);
							if (dock) {
								dock->setVisible(true);
								dock->raise();
								MCDN_LOG(LOG_INFO, "First install — dock auto-shown via QDockWidget");
								return;
							}
							w = w->parentWidget();
						}
						/* Fallback */
						g_panel->setVisible(true);
						MCDN_LOG(LOG_INFO, "First install — panel shown (no QDockWidget parent found)");
					},
					nullptr);
			}
		}
	} else {
		MCDN_LOG(LOG_WARNING,
			 "Failed to register control dock — "
			 "OBS version may not support add_dock_by_id");
		delete g_panel;
		g_panel = nullptr;
	}
}

void UnregisterControlDock()
{
	if (g_panel) {
		obs_frontend_remove_dock("midcircuitcdn-control");
		/* OBS owns the widget after add_dock_by_id, don't delete */
		g_panel = nullptr;
		MCDN_LOG(LOG_INFO, "Control dock unregistered");
	}
}

#endif /* HAVE_QT */
