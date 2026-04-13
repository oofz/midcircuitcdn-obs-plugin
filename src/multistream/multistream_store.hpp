/*
 * MidcircuitCDN OBS Plugin — Multistream Config Store
 * ────────────────────────────────────────────────────────────────────────────
 * Data model and persistence for multistream targets.
 * Stores platform enable/disable state and stream keys in OBS's global.ini
 * under the [MidcircuitCDN_Multistream] section.
 *
 * Keys persist across MidcircuitCDN connect/disconnect cycles.
 */

#pragma once

#include <string>
#include <vector>

/* ── Data Model ────────────────────────────────────────────────────────── */

struct MultistreamTarget {
	std::string platform_id;   /* "twitch", "kick", "youtube", "x" */
	std::string display_name;  /* "Twitch", "Kick", "YouTube", "X" */
	std::string rtmp_url;      /* RTMP ingest URL (hardcoded or user-entered) */
	std::string stream_key;    /* user-entered key (persisted) */
	bool enabled;              /* whether to stream to this target */
	bool custom_url;           /* true = user must enter the URL (Kick, X) */
};

/* ── Platform RTMP URLs (constants) ────────────────────────────────────── */

static const char *RTMP_URL_TWITCH  = "rtmp://live.twitch.tv/app";
static const char *RTMP_URL_YOUTUBE = "rtmp://a.rtmp.youtube.com/live2";
/* Kick and X do not have global ingest URLs — each user gets a unique
 * URL from their platform dashboard, so we leave these empty and let
 * the user enter them in the multistream dialog. */

/* ── API ───────────────────────────────────────────────────────────────── */

/*
 * Returns the default list of 4 targets with hardcoded URLs,
 * empty stream keys, and enabled=false.
 */
std::vector<MultistreamTarget> GetDefaultTargets();

/*
 * Save all multistream targets to OBS global config.
 * Writes: twitch_enabled, twitch_stream_key, kick_enabled, etc.
 */
void SaveMultistreamConfig(const std::vector<MultistreamTarget> &targets);

/*
 * Load multistream targets from OBS global config.
 * Returns defaults for any missing keys.
 */
std::vector<MultistreamTarget> LoadMultistreamConfig();

/*
 * Returns only targets that are enabled AND have a non-empty stream key.
 */
std::vector<MultistreamTarget> GetEnabledTargets();

/*
 * Returns true if at least one target is enabled with a non-empty key.
 */
bool HasAnyMultistreamEnabled();
