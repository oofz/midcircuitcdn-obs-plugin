/*
 * MidCircuitCDN OBS Plugin — Settings Dialog (Implementation)
 * ────────────────────────────────────────────────────────────────────────────
 * Uses obs_frontend_add_tools_menu_item to hook into the OBS Tools menu.
 * When clicked, it either starts the OAuth flow or shows current status.
 */

#include "settings_dialog.hpp"
#include "../plugin-macros.h"
#include "../auth/plugin_oauth.hpp"
#include "../auth/plugin_store.hpp"
#include "../obs_integration/stream_config.hpp"
#include "../obs_integration/encoder_config.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>

/* ── Callback: handle credentials received from OAuth ─────────────────── */
static void OnCredentialsReceived(const PluginCredentials &creds)
{
	MCDN_LOG(LOG_INFO, "Credentials received for: %s",
		 creds.stream_slug.c_str());

	/* 1. Persist credentials locally */
	SaveCredentials(creds);

	/* 2. Apply stream settings (URL + Key) to OBS */
	ApplyStreamSettings(creds.server_url, creds.stream_key);

	/* 3. Cap the video bitrate */
	CapVideoBitrate(MCDN_TARGET_BITRATE_KBPS);
}

/* ── Tools Menu click handler ─────────────────────────────────────────── */
static void OnToolsMenuClicked(void *data)
{
	(void)data;

	if (IsOAuthFlowActive()) {
		MCDN_LOG(LOG_INFO,
			 "OAuth flow already active — ignoring click");
		return;
	}

	/* Check if already connected */
	PluginCredentials existing;
	if (HasStoredCredentials() && LoadCredentials(existing)) {
		MCDN_LOG(LOG_INFO, "Already connected as: %s",
			 existing.stream_slug.c_str());
		/*
		 * TODO: Show a dialog with options:
		 *   - "Connected as <slug>" status
		 *   - "Re-apply Settings" button
		 *   - "Disconnect" button
		 *
		 * For now, re-apply settings on every click and log status.
		 */
		ApplyStreamSettings(existing.server_url,
				    existing.stream_key);
		CapVideoBitrate(MCDN_TARGET_BITRATE_KBPS);
		return;
	}

	/* Not connected — start OAuth */
	MCDN_LOG(LOG_INFO, "Starting MidCircuitCDN OAuth flow...");
	StartOAuthFlow(OnCredentialsReceived);
}

/* ── Public API ───────────────────────────────────────────────────────── */

void RegisterSettingsMenu()
{
	obs_frontend_add_tools_menu_item("Connect MidCircuitCDN",
					 OnToolsMenuClicked, nullptr);
	MCDN_LOG(LOG_INFO, "Tools menu item registered");
}

void UnregisterSettingsMenu()
{
	/* OBS handles cleanup of tools menu items on unload */
	MCDN_LOG(LOG_INFO, "Tools menu item cleanup");
}
