/*
 * MidcircuitCDN OBS Plugin — Encoder Config (Implementation)
 * ────────────────────────────────────────────────────────────────────────────
 * Hooks into the video encoder and caps bitrate at the MidcircuitCDN limit.
 */

#include "encoder_config.hpp"
#include "../plugin-macros.h"

#include <obs.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>

void CapVideoBitrate(int max_bitrate_kbps)
{
	/* 1. Get the streaming output */
	obs_output_t *output = obs_frontend_get_streaming_output();
	if (!output) {
		MCDN_LOG(LOG_WARNING,
			 "CapVideoBitrate: no streaming output — "
			 "will apply on next stream start");
		return;
	}

	/* 2. Get the video encoder attached to the output */
	obs_encoder_t *vencoder = obs_output_get_video_encoder(output);
	if (!vencoder) {
		MCDN_LOG(LOG_WARNING,
			 "CapVideoBitrate: no video encoder attached");
		obs_output_release(output);
		return;
	}

	/* 3. Read current encoder settings */
	obs_data_t *settings = obs_encoder_get_settings(vencoder);
	if (!settings) {
		MCDN_LOG(LOG_WARNING,
			 "CapVideoBitrate: could not read encoder settings");
		obs_output_release(output);
		return;
	}

	int current = (int)obs_data_get_int(settings, "bitrate");

	/* 4. Only cap if currently above the limit */
	if (current > max_bitrate_kbps) {
		obs_data_set_int(settings, "bitrate",
				 (long long)max_bitrate_kbps);
		obs_encoder_update(vencoder, settings);

		MCDN_LOG(LOG_INFO,
			 "Video bitrate capped: %d -> %d kbps",
			 current, max_bitrate_kbps);
	} else if (current > 0) {
		MCDN_LOG(LOG_INFO,
			 "Video bitrate already within cap: %d kbps (max %d)",
			 current, max_bitrate_kbps);
	} else {
		/* Bitrate is 0 or unset — force it to the cap */
		obs_data_set_int(settings, "bitrate",
				 (long long)max_bitrate_kbps);
		obs_encoder_update(vencoder, settings);

		MCDN_LOG(LOG_INFO,
			 "Video bitrate was unset — set to %d kbps",
			 max_bitrate_kbps);
	}

	/* 5. Also try to update the OBS Output settings (profile-level).
	 *    This ensures the setting persists in the output config. */
	config_t *cfg = obs_frontend_get_profile_config();
	if (cfg) {
		int profile_bitrate = (int)config_get_uint(
			cfg, "SimpleOutput", "VBitrate");
		if (profile_bitrate > max_bitrate_kbps) {
			config_set_uint(cfg, "SimpleOutput", "VBitrate",
					max_bitrate_kbps);
			config_save(cfg);
			MCDN_LOG(LOG_INFO,
				 "Profile VBitrate capped: %d -> %d kbps",
				 profile_bitrate, max_bitrate_kbps);
		}

		/* Also handle Advanced output mode */
		int adv_bitrate = (int)config_get_uint(
			cfg, "AdvOut", "FFVBitrate");
		if (adv_bitrate > max_bitrate_kbps) {
			config_set_uint(cfg, "AdvOut", "FFVBitrate",
					max_bitrate_kbps);
			config_save(cfg);
			MCDN_LOG(LOG_INFO,
				 "Advanced FFVBitrate capped: %d -> %d kbps",
				 adv_bitrate, max_bitrate_kbps);
		}
	}

	obs_data_release(settings);
	obs_output_release(output);
}
