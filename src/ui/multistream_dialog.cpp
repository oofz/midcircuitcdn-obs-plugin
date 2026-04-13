/*
 * MidcircuitCDN OBS Plugin — Multistream Settings Dialog (Implementation)
 * ────────────────────────────────────────────────────────────────────────────
 * Qt modal dialog for configuring multistream targets.
 *
 * Layout:
 * ┌───────────────────────────────────────────────┐
 * │          🔀  Multistream Setup                │
 * ├───────────────────────────────────────────────┤
 * │  ☑ Twitch                                     │
 * │    Stream Key: [••••••••••••••] [👁]           │
 * │                                               │
 * │  ☑ Kick                                       │
 * │    Stream Key: [••••••••••••••] [👁]           │
 * │                                               │
 * │  ☑ YouTube                                    │
 * │    Stream Key: [••••••••••••••] [👁]           │
 * │                                               │
 * │  ☑ X                                          │
 * │    Stream Key: [••••••••••••••] [👁]           │
 * ├───────────────────────────────────────────────┤
 * │                [ Cancel ]  [ Save ]           │
 * └───────────────────────────────────────────────┘
 *
 * Styled with the same Catppuccin dark theme as control_dock.cpp
 */

#ifdef HAVE_QT

#include "multistream_dialog.hpp"
#include "../multistream/multistream_store.hpp"
#include "../plugin-macros.h"

#include <obs-module.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QFont>

/* ── Dialog stylesheet matching the control dock theme ───────────────── */

static const char *DIALOG_STYLE = R"(
    QDialog {
        background-color: #1e1e2e;
        color: #cdd6f4;
    }
    QLabel#dialogTitle {
        color: #cba6f7;
        font-weight: bold;
        font-size: 15px;
        padding: 4px;
    }
    QLabel#platformLabel {
        color: #cdd6f4;
        font-weight: bold;
        font-size: 13px;
    }
    QLabel#keyLabel {
        color: #a6adc8;
        font-size: 11px;
    }
    QCheckBox {
        color: #cdd6f4;
        font-size: 13px;
        spacing: 6px;
    }
    QCheckBox::indicator {
        width: 16px;
        height: 16px;
    }
    QLineEdit {
        background: #181825;
        color: #cdd6f4;
        border: 1px solid #313244;
        border-radius: 4px;
        padding: 5px 8px;
        font-size: 12px;
    }
    QLineEdit:focus {
        border-color: #7c3aed;
    }
    QPushButton#saveBtn {
        background-color: #7c3aed;
        color: #ffffff;
        border: none;
        border-radius: 4px;
        padding: 8px 20px;
        font-weight: bold;
        font-size: 13px;
    }
    QPushButton#saveBtn:hover {
        background-color: #8b5cf6;
    }
    QPushButton#cancelBtn {
        background-color: transparent;
        color: #a6adc8;
        border: 1px solid #45475a;
        border-radius: 4px;
        padding: 8px 20px;
        font-size: 13px;
    }
    QPushButton#cancelBtn:hover {
        background-color: #313244;
        color: #cdd6f4;
    }
    QPushButton#showHideBtn {
        background: #313244;
        color: #cdd6f4;
        border: 1px solid #45475a;
        border-radius: 4px;
        padding: 4px 8px;
        font-size: 11px;
        min-width: 30px;
    }
    QPushButton#showHideBtn:hover {
        background: #45475a;
    }
    QGroupBox {
        background: #181825;
        border: 1px solid #313244;
        border-radius: 6px;
        margin-top: 4px;
        padding: 10px;
    }
)";

/* ── Constructor ──────────────────────────────────────────────────────── */

MultistreamDialog::MultistreamDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle("Multistream Setup");
	setMinimumWidth(400);
	setStyleSheet(DIALOG_STYLE);

	BuildUI();
	LoadFromConfig();
}

/* ── Build the UI ─────────────────────────────────────────────────────── */

void MultistreamDialog::BuildUI()
{
	auto *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(16, 12, 16, 12);
	mainLayout->setSpacing(10);

	/* Title */
	auto *title = new QLabel(
		QString::fromUtf8("\xe2\x86\x94 Multistream Setup"), this);
	title->setObjectName("dialogTitle");
	title->setAlignment(Qt::AlignCenter);
	mainLayout->addWidget(title);

	/* Platform rows */
	auto targets = GetDefaultTargets();
	for (const auto &t : targets) {
		auto *group = new QGroupBox(this);
		auto *groupLayout = new QVBoxLayout(group);
		groupLayout->setContentsMargins(10, 8, 10, 8);
		groupLayout->setSpacing(6);

		/* Checkbox row */
		auto *checkbox = new QCheckBox(
			QString::fromStdString(t.display_name), group);

		groupLayout->addWidget(checkbox);

		/* Server URL row (only for Kick, X — user must paste from dashboard) */
		QLineEdit *urlInput = nullptr;
		if (t.custom_url) {
			auto *urlRow = new QHBoxLayout();
			urlRow->setSpacing(4);

			auto *urlLabel = new QLabel("Server URL:", group);
			urlLabel->setObjectName("keyLabel");
			urlRow->addWidget(urlLabel);

			urlInput = new QLineEdit(group);
			urlInput->setPlaceholderText(
				"Paste RTMP URL from dashboard...");
			urlRow->addWidget(urlInput, 1);

			groupLayout->addLayout(urlRow);
		}

		/* Stream key row */
		auto *keyRow = new QHBoxLayout();
		keyRow->setSpacing(4);

		auto *keyLabel = new QLabel("Stream Key:", group);
		keyLabel->setObjectName("keyLabel");
		keyRow->addWidget(keyLabel);

		auto *keyInput = new QLineEdit(group);
		keyInput->setEchoMode(QLineEdit::Password);
		keyInput->setPlaceholderText("Enter stream key...");
		keyRow->addWidget(keyInput, 1);

		auto *showHideBtn =
			new QPushButton(QString::fromUtf8("\xf0\x9f\x91\x81"),
					group);
		showHideBtn->setObjectName("showHideBtn");
		showHideBtn->setFixedWidth(36);
		keyRow->addWidget(showHideBtn);

		groupLayout->addLayout(keyRow);

		mainLayout->addWidget(group);

		/* Track the row for save/load */
		PlatformRow row;
		row.platform_id = t.platform_id;
		row.custom_url = t.custom_url;
		row.checkbox = checkbox;
		row.urlInput = urlInput; /* nullptr for Twitch/YouTube */
		row.keyInput = keyInput;
		row.showHideBtn = showHideBtn;
		m_rows.push_back(row);

		/* Connect show/hide toggle */
		size_t idx = m_rows.size() - 1;
		connect(showHideBtn, &QPushButton::clicked, this,
			[this, idx]() {
				ToggleKeyVisibility(m_rows[idx]);
			});

		/* Enable/disable inputs based on checkbox */
		connect(checkbox, &QCheckBox::toggled, keyInput,
			&QLineEdit::setEnabled);
		if (urlInput) {
			connect(checkbox, &QCheckBox::toggled, urlInput,
				&QLineEdit::setEnabled);
		}
	}

	/* Spacer */
	mainLayout->addStretch(1);

	/* Button row */
	auto *btnRow = new QHBoxLayout();
	btnRow->setSpacing(8);
	btnRow->addStretch(1);

	m_cancelBtn = new QPushButton("Cancel", this);
	m_cancelBtn->setObjectName("cancelBtn");
	connect(m_cancelBtn, &QPushButton::clicked, this,
		&MultistreamDialog::OnCancelClicked);
	btnRow->addWidget(m_cancelBtn);

	m_saveBtn = new QPushButton("Save", this);
	m_saveBtn->setObjectName("saveBtn");
	connect(m_saveBtn, &QPushButton::clicked, this,
		&MultistreamDialog::OnSaveClicked);
	btnRow->addWidget(m_saveBtn);

	mainLayout->addLayout(btnRow);
}

/* ── Load config into UI ──────────────────────────────────────────────── */

void MultistreamDialog::LoadFromConfig()
{
	auto targets = LoadMultistreamConfig();

	for (const auto &t : targets) {
		for (auto &row : m_rows) {
			if (row.platform_id == t.platform_id) {
				row.checkbox->setChecked(t.enabled);
				row.keyInput->setText(
					QString::fromStdString(t.stream_key));
				row.keyInput->setEnabled(t.enabled);
				if (row.urlInput) {
					row.urlInput->setText(
						QString::fromStdString(
							t.rtmp_url));
					row.urlInput->setEnabled(
						t.enabled);
				}
				break;
			}
		}
	}
}

/* ── Save UI state to config ──────────────────────────────────────────── */

void MultistreamDialog::SaveToConfig()
{
	auto targets = GetDefaultTargets();

	for (auto &t : targets) {
		for (const auto &row : m_rows) {
			if (row.platform_id == t.platform_id) {
				t.enabled = row.checkbox->isChecked();
				t.stream_key =
					row.keyInput->text().toStdString();
				if (row.urlInput) {
					t.rtmp_url = row.urlInput->text()
							     .toStdString();
				}
				break;
			}
		}
	}

	SaveMultistreamConfig(targets);
}

/* ── Toggle key visibility ────────────────────────────────────────────── */

void MultistreamDialog::ToggleKeyVisibility(PlatformRow &row)
{
	if (row.keyInput->echoMode() == QLineEdit::Password) {
		row.keyInput->setEchoMode(QLineEdit::Normal);
		row.showHideBtn->setText(
			QString::fromUtf8("\xe2\x97\x89")); /* ◉ */
	} else {
		row.keyInput->setEchoMode(QLineEdit::Password);
		row.showHideBtn->setText(
			QString::fromUtf8("\xf0\x9f\x91\x81")); /* 👁 */
	}
}

/* ── Slots ─────────────────────────────────────────────────────────────── */

void MultistreamDialog::OnSaveClicked()
{
	SaveToConfig();
	MCDN_LOG(LOG_INFO, "Multistream settings saved via dialog");
	accept();
}

void MultistreamDialog::OnCancelClicked()
{
	reject();
}

#endif /* HAVE_QT */
