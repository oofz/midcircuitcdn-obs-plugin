/*
 * MidcircuitCDN OBS Plugin — Stream Config (Implementation)
 * ────────────────────────────────────────────────────────────────────────────
 * Programmatically overwrites the OBS streaming service, URL, and key
 * using the OBS C-API.
 */

#include "stream_config.hpp"
#include "../plugin-macros.h"

#include <obs.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>

void ApplyStreamSettings(const std::string &server_url,
			 const std::string &stream_key)
{
	if (server_url.empty() || stream_key.empty()) {
		MCDN_LOG(LOG_WARNING,
			 "ApplyStreamSettings: empty URL or key — skipping");
		return;
	}

	/* 1. Build new settings with our RTMP URL + stream key */
	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "server", server_url.c_str());
	obs_data_set_string(settings, "key", stream_key.c_str());

	/* 2. Create a new custom RTMP service with these settings.
	 *    "rtmp_custom" is the OBS internal ID for "Custom..." service
	 *    which accepts arbitrary server + key.
	 *    Pass nullptr for hotkey_data — we don't need hotkeys. */
	obs_service_t *new_service = obs_service_create(
		"rtmp_custom", "MidcircuitCDN", settings, nullptr);

	if (new_service) {
		/* 3. Set as the active streaming service.
		 *    obs_frontend_set_streaming_service() adds its own ref,
		 *    so we release our creation ref afterward. */
		obs_frontend_set_streaming_service(new_service);
		obs_frontend_save_streaming_service();

		MCDN_LOG(LOG_INFO,
			 "Stream settings applied: server=%s, key=[%zu chars]",
			 server_url.c_str(), stream_key.size());

		obs_service_release(new_service);
	} else {
		MCDN_LOG(LOG_ERROR,
			 "Failed to create rtmp_custom service");
	}

	obs_data_release(settings);

	/*
	 * NOTE: Do NOT release the old service from
	 * obs_frontend_get_streaming_service(). Per the OBS docs, it returns
	 * a borrowed reference (no ref increment). Releasing it would
	 * free the service while OBS still owns it → use-after-free crash.
	 */
}
