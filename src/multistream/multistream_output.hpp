/*
 * MidcircuitCDN OBS Plugin — Multistream Output Manager
 * ────────────────────────────────────────────────────────────────────────────
 * Creates and manages additional OBS RTMP outputs that share the main
 * stream's video/audio encoders. Zero extra encoding overhead.
 *
 * Based on the approach used by:
 *   - sorayuki/obs-multi-rtmp (4.8k★)
 *   - Aitum/obs-aitum-multistream (207★)
 *
 * Key pattern from reference code:
 *   obs_service_t *svc = obs_service_create("rtmp_custom", name, settings, nullptr);
 *   obs_output_t  *out = obs_output_create("rtmp_output", name, nullptr, nullptr);
 *   obs_output_set_video_encoder(out, shared_venc);
 *   obs_output_set_audio_encoder(out, shared_aenc, 0);
 *   obs_output_set_service(out, svc);
 *   obs_output_start(out);
 */

#pragma once

#include "multistream_store.hpp"
#include <obs.h>
#include <string>
#include <vector>

/* ── Output Status (for UI display) ────────────────────────────────────── */

struct MultistreamOutputStatus {
	std::string platform_id;
	std::string display_name;
	bool active;
};

/* ── Output Manager ────────────────────────────────────────────────────── */

class MultistreamOutputManager {
public:
	MultistreamOutputManager();
	~MultistreamOutputManager();

	/*
	 * Start all enabled multistream outputs.
	 * Shares the main stream's video/audio encoders.
	 *
	 * Returns: true if at least one output started successfully.
	 *
	 * Dependencies:
	 *   - GetEnabledTargets() for target list
	 *   - obs_frontend_get_streaming_output() for main output
	 *   - obs_output_get_video_encoder() / obs_output_get_audio_encoder()
	 *   - obs_service_create("rtmp_custom", ...)
	 *   - obs_output_create("rtmp_output", ...)
	 *   - obs_output_start()
	 */
	bool StartAllOutputs();

	/*
	 * Stop and release all active multistream outputs.
	 */
	void StopAllOutputs();

	/*
	 * Check if a specific platform output is active.
	 */
	bool IsOutputActive(const std::string &platform_id) const;

	/*
	 * Get status of all currently tracked outputs.
	 */
	std::vector<MultistreamOutputStatus> GetOutputStatuses() const;

private:
	struct ActiveOutput {
		std::string platform_id;
		std::string display_name;
		obs_output_t *output;
		obs_service_t *service;
	};

	std::vector<ActiveOutput> m_activeOutputs;

	/*
	 * Create and start a single RTMP output for the given target.
	 */
	bool CreateAndStartOutput(const MultistreamTarget &target,
				  obs_encoder_t *video_encoder,
				  obs_encoder_t *audio_encoder);
};
