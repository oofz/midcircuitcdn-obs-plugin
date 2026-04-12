/*
 * MidcircuitCDN OBS Plugin — Control Panel Dock Widget (Implementation)
 * ────────────────────────────────────────────────────────────────────────────
 * A native Qt dock panel that appears in OBS like the Controls panel.
 * Shows a 1-click Connect button when disconnected, and status + gear
 * icon when connected.
 *
 * Uses obs_frontend_add_dock_by_id() (OBS 30+) to register as a
 * first-class draggable/dockable panel.
 */

#ifdef HAVE_QT

#include "control_dock.hpp"
#include "../plugin-macros.h"
#include "../auth/plugin_oauth.hpp"
#include "../auth/plugin_store.hpp"
#include "../obs_integration/stream_config.hpp"
#include "../obs_integration/encoder_config.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QStyle>
#include <QFont>
#include <QIcon>
#include <QApplication>
#include <QTimer>

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
    QPushButton#disconnectBtn {
        background-color: transparent;
        color: #f38ba8;
        border: 1px solid #f38ba8;
        border-radius: 4px;
        padding: 4px 10px;
        font-size: 11px;
    }
    QPushButton#disconnectBtn:hover {
        background-color: #f38ba8;
        color: #1e1e2e;
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

	UpdateState();
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

	/* Stream slug */
	if (!creds.stream_slug.empty()) {
		m_slugLabel = new QLabel(
			QString::fromStdString(creds.stream_slug), this);
		m_slugLabel->setObjectName("slugLabel");
		m_slugLabel->setAlignment(Qt::AlignCenter);
		m_mainLayout->addWidget(m_slugLabel);
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

	/* Disconnect button */
	m_disconnectBtn = new QPushButton("Disconnect", this);
	m_disconnectBtn->setObjectName("disconnectBtn");
	connect(m_disconnectBtn, &QPushButton::clicked, this,
		&McdnControlPanel::OnDisconnectClicked);
	m_mainLayout->addWidget(m_disconnectBtn);
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
}

/* ── Clear dynamic widgets (keep title) ───────────────────────────────── */
void McdnControlPanel::ClearLayout()
{
	/* Remove everything except the title (item 0) */
	while (m_mainLayout->count() > 1) {
		auto *item = m_mainLayout->takeAt(1);
		if (QWidget *w = item->widget()) {
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
	m_statusLabel = nullptr;
	m_slugLabel = nullptr;
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

/* ═══════════════════════════════════════════════════════════════════════ */
/* ── Registration with OBS ────────────────────────────────────────────── */
/* ═══════════════════════════════════════════════════════════════════════ */

static McdnControlPanel *g_panel = nullptr;

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
