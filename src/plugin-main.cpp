/*
 * MidcircuitCDN OBS Plugin
 * ────────────────────────────────────────────────────────────────────────────
 * Provides a seamless "Connect Account" experience for MidcircuitCDN users.
 * Automatically configures stream URL, key, and caps video bitrate.
 *
 * Copyright (C) 2026 MidcircuitCDN
 * Licensed under GPLv2 (required by OBS plugin distribution).
 */

#include <obs-module.h>
#include <obs-frontend-api.h>

#include "plugin-macros.h"
#include "auth/plugin_oauth.hpp"
#include "auth/plugin_store.hpp"
#include "ui/settings_dialog.hpp"
#include "ui/dashboard_dock.hpp"
#include "ui/control_dock.hpp"
#include "obs_integration/stream_config.hpp"
#include "obs_integration/encoder_config.hpp"

/* ── OBS Module Boilerplate ─────────────────────────────────────────────── */

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("midcircuitcdn-obs-plugin", "en-US")
OBS_MODULE_AUTHOR("MidcircuitCDN")

const char *obs_module_name(void)
{
	return "MidcircuitCDN";
}

const char *obs_module_description(void)
{
	return "Connect your MidcircuitCDN account to automatically configure "
	       "stream settings, URL, key, and bitrate cap.";
}

/* ── Frontend event callback ────────────────────────────────────────────── */
static void OnFrontendEvent(enum obs_frontend_event event, void *data)
{
	(void)data;

	if (event == OBS_FRONTEND_EVENT_STREAMING_STARTING) {
		/*
		 * NOTE: We MUST NOT call ApplyStreamSettings() here.
		 * ApplyStreamSettings creates a new obs_service_t and swaps
		 * out the streaming service. OBS is currently in the middle of
		 * StartStreaming() and holds a reference to the existing
		 * service object. Replacing it underneath causes a
		 * use-after-free / double-release crash in obs_service_release.
		 *
		 * Settings are already applied at OAuth connect time via the
		 * control dock's OnConnectClicked handler.
		 */
		PluginCredentials creds;
		if (LoadCredentials(creds)) {
			MCDN_LOG(LOG_INFO,
				 "Stream starting — MidcircuitCDN settings "
				 "already configured for: %s",
				 creds.stream_slug.c_str());
		}
	}
}

/* ── Module Lifecycle ───────────────────────────────────────────────────── */

bool obs_module_load(void)
{
	blog(LOG_INFO, "[MidcircuitCDN] Plugin loaded (version %s)",
	     PLUGIN_VERSION);

	/* Register "Connect MidcircuitCDN" in the Tools menu */
	RegisterSettingsMenu();

	/* Register "MidcircuitCDN Dashboard" in the Tools menu */
	RegisterDashboardDock();

	/* Register native control dock panel */
	RegisterControlDock();

	/* Auto-apply settings when user starts streaming */
	obs_frontend_add_event_callback(OnFrontendEvent, nullptr);

	/* If credentials exist from a previous session, log it */
	PluginCredentials creds;
	if (LoadCredentials(creds)) {
		MCDN_LOG(LOG_INFO, "Restored saved credentials for: %s",
			 creds.stream_slug.c_str());
	}

	return true;
}

void obs_module_unload(void)
{
	blog(LOG_INFO, "[MidcircuitCDN] Plugin unloading...");

	/* Cancel any in-progress OAuth flow */
	CancelOAuthFlow();

	/* Cleanup UI */
	UnregisterSettingsMenu();
	UnregisterDashboardDock();
	UnregisterControlDock();

	blog(LOG_INFO, "[MidcircuitCDN] Plugin unloaded");
}
