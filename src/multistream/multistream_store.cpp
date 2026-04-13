/*
 * MidCircuitCDN OBS Plugin — Multistream Config Store (Implementation)
 * ────────────────────────────────────────────────────────────────────────────
 * Uses OBS's global config (global.ini) for persistence, same pattern as
 * auth/plugin_store.cpp. All keys live under [MidCircuitCDN_Multistream].
 */

#include "multistream_store.hpp"
#include "../plugin-macros.h"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>

static const char *CONFIG_SECTION = "MidCircuitCDN_Multistream";

/* ── Suffix helpers for config keys ────────────────────────────────────── */

static std::string EnabledKey(const std::string &platform_id)
{
	return platform_id + "_enabled";
}

static std::string StreamKeyKey(const std::string &platform_id)
{
	return platform_id + "_stream_key";
}

static std::string RtmpUrlKey(const std::string &platform_id)
{
	return platform_id + "_rtmp_url";
}

/* ── Default Targets ───────────────────────────────────────────────────── */

std::vector<MultistreamTarget> GetDefaultTargets()
{
	return {
		{"twitch",  "Twitch",  RTMP_URL_TWITCH,  "", false, false},
		{"kick",    "Kick",    "",               "", false, true},
		{"youtube", "YouTube", RTMP_URL_YOUTUBE, "", false, false},
		{"x",       "X",       "",               "", false, true},
	};
}

/* ── Save ──────────────────────────────────────────────────────────────── */

void SaveMultistreamConfig(const std::vector<MultistreamTarget> &targets)
{
	config_t *cfg = obs_frontend_get_global_config();
	if (!cfg) {
		MCDN_LOG(LOG_ERROR,
			 "SaveMultistreamConfig: could not get global config");
		return;
	}

	for (const auto &t : targets) {
		std::string ek = EnabledKey(t.platform_id);
		std::string sk = StreamKeyKey(t.platform_id);

		config_set_bool(cfg, CONFIG_SECTION, ek.c_str(), t.enabled);
		config_set_string(cfg, CONFIG_SECTION, sk.c_str(),
				  t.stream_key.c_str());

		/* Persist user-entered RTMP URL for custom-URL platforms */
		if (t.custom_url) {
			std::string uk = RtmpUrlKey(t.platform_id);
			config_set_string(cfg, CONFIG_SECTION, uk.c_str(),
					  t.rtmp_url.c_str());
		}
	}

	config_save(cfg);
	MCDN_LOG(LOG_INFO, "Multistream config saved (%zu targets)",
		 targets.size());
}

/* ── Load ──────────────────────────────────────────────────────────────── */

std::vector<MultistreamTarget> LoadMultistreamConfig()
{
	config_t *cfg = obs_frontend_get_global_config();
	auto targets = GetDefaultTargets();

	if (!cfg)
		return targets;

	for (auto &t : targets) {
		std::string ek = EnabledKey(t.platform_id);
		std::string sk = StreamKeyKey(t.platform_id);

		t.enabled = config_get_bool(cfg, CONFIG_SECTION, ek.c_str());

		const char *key =
			config_get_string(cfg, CONFIG_SECTION, sk.c_str());
		if (key && *key)
			t.stream_key = key;

		/* Load user-entered RTMP URL for custom-URL platforms */
		if (t.custom_url) {
			std::string uk = RtmpUrlKey(t.platform_id);
			const char *url = config_get_string(
				cfg, CONFIG_SECTION, uk.c_str());
			if (url && *url)
				t.rtmp_url = url;
		}
	}

	return targets;
}

/* ── Filtered queries ──────────────────────────────────────────────────── */

std::vector<MultistreamTarget> GetEnabledTargets()
{
	auto all = LoadMultistreamConfig();
	std::vector<MultistreamTarget> result;

	for (const auto &t : all) {
		if (t.enabled && !t.stream_key.empty() &&
		    !t.rtmp_url.empty())
			result.push_back(t);
	}

	return result;
}

bool HasAnyMultistreamEnabled()
{
	auto all = LoadMultistreamConfig();
	for (const auto &t : all) {
		if (t.enabled && !t.stream_key.empty() &&
		    !t.rtmp_url.empty())
			return true;
	}
	return false;
}
