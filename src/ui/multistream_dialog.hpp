/*
 * MidCircuitCDN OBS Plugin — Multistream Settings Dialog
 * ────────────────────────────────────────────────────────────────────────────
 * Qt modal dialog for configuring multistream targets.
 * Shows 4 platform rows (Twitch, Kick, YouTube, X) each with:
 *   - Enable/disable checkbox
 *   - Stream key input (password mode with show/hide toggle)
 *
 * Requires Qt6 — gated behind HAVE_QT at compile time.
 */

#pragma once

#ifdef HAVE_QT

#include <QDialog>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <string>
#include <vector>

class MultistreamDialog : public QDialog {
	Q_OBJECT

public:
	explicit MultistreamDialog(QWidget *parent = nullptr);
	~MultistreamDialog() override = default;

private slots:
	void OnSaveClicked();
	void OnCancelClicked();

private:
	struct PlatformRow {
		std::string platform_id;
		bool custom_url;           /* true = Kick, X (user enters URL) */
		QCheckBox *checkbox;
		QLineEdit *urlInput;       /* only used when custom_url == true */
		QLineEdit *keyInput;
		QPushButton *showHideBtn;
	};

	std::vector<PlatformRow> m_rows;
	QPushButton *m_saveBtn = nullptr;
	QPushButton *m_cancelBtn = nullptr;

	void BuildUI();
	void LoadFromConfig();
	void SaveToConfig();
	void ToggleKeyVisibility(PlatformRow &row);
};

#endif /* HAVE_QT */
