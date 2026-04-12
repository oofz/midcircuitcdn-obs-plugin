/*
 * MidcircuitCDN OBS Plugin — Stream Config
 * ────────────────────────────────────────────────────────────────────────────
 * Programmatically overwrites the OBS streaming service, URL, and key.
 */

#pragma once

#include <string>

/*
 * Apply MidcircuitCDN stream settings to the active OBS profile.
 *
 * Uses the OBS C-API:
 *   obs_frontend_get_streaming_service()
 *   obs_data_set_string(settings, "server", ...)
 *   obs_data_set_string(settings, "key", ...)
 *   obs_service_update(service, settings)
 *
 * @param server_url  RTMP ingest URL (e.g., "rtmp://live.midcircuitcdn.com/live")
 * @param stream_key  User's stream key (e.g., "live_abc123...")
 */
void ApplyStreamSettings(const std::string &server_url,
			 const std::string &stream_key);
