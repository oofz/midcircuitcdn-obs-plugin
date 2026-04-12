/*
 * MidcircuitCDN OBS Plugin — Local Credential Store (Implementation)
 * ────────────────────────────────────────────────────────────────────────────
 * Uses OBS's config API (obs_data / global config) for persistence.
 */

#include "plugin_store.hpp"
#include "../plugin-macros.h"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>

static const char *CONFIG_SECTION = "MidcircuitCDN";
static const char *KEY_SLUG = "stream_slug";
static const char *KEY_STREAM_KEY = "stream_key";
static const char *KEY_SERVER_URL = "server_url";

void SaveCredentials(const PluginCredentials &creds)
{
	config_t *cfg = obs_frontend_get_global_config();
	if (!cfg) {
		MCDN_LOG(LOG_ERROR,
			 "SaveCredentials: could not get global config");
		return;
	}

	config_set_string(cfg, CONFIG_SECTION, KEY_SLUG,
			  creds.stream_slug.c_str());
	config_set_string(cfg, CONFIG_SECTION, KEY_STREAM_KEY,
			  creds.stream_key.c_str());
	config_set_string(cfg, CONFIG_SECTION, KEY_SERVER_URL,
			  creds.server_url.c_str());
	config_save(cfg);

	MCDN_LOG(LOG_INFO, "Credentials saved for user: %s",
		 creds.stream_slug.c_str());
}

bool LoadCredentials(PluginCredentials &out_creds)
{
	config_t *cfg = obs_frontend_get_global_config();
	if (!cfg)
		return false;

	const char *slug =
		config_get_string(cfg, CONFIG_SECTION, KEY_SLUG);
	const char *key =
		config_get_string(cfg, CONFIG_SECTION, KEY_STREAM_KEY);
	const char *url =
		config_get_string(cfg, CONFIG_SECTION, KEY_SERVER_URL);

	if (!key || !*key)
		return false;

	out_creds.stream_slug = slug ? slug : "";
	out_creds.stream_key = key;
	out_creds.server_url =
		(url && *url) ? url
			      : "rtmp://live.midcircuitcdn.com/live";

	return true;
}

void ClearCredentials()
{
	config_t *cfg = obs_frontend_get_global_config();
	if (!cfg)
		return;

	config_set_string(cfg, CONFIG_SECTION, KEY_SLUG, "");
	config_set_string(cfg, CONFIG_SECTION, KEY_STREAM_KEY, "");
	config_set_string(cfg, CONFIG_SECTION, KEY_SERVER_URL, "");
	config_save(cfg);

	MCDN_LOG(LOG_INFO, "Credentials cleared (disconnected)");
}

bool HasStoredCredentials()
{
	config_t *cfg = obs_frontend_get_global_config();
	if (!cfg)
		return false;

	const char *key =
		config_get_string(cfg, CONFIG_SECTION, KEY_STREAM_KEY);
	return key && *key;
}
