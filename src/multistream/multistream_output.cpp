/*
 * MidCircuitCDN OBS Plugin — Multistream Output Manager (Implementation)
 * ────────────────────────────────────────────────────────────────────────────
 * Manages additional RTMP outputs that share the main stream's encoders.
 *
 * Reference implementation pattern from obs-multi-rtmp / Aitum Multistream:
 *   1. Get the main streaming output from OBS frontend
 *   2. Extract the video and audio encoders from it
 *   3. Create a new rtmp_custom service with the platform's URL + user key
 *   4. Create a new rtmp_output and attach the shared encoders + service
 *   5. Start the output
 *   6. On stop, release all refs
 */

#include "multistream_output.hpp"
#include "../plugin-macros.h"

#include <obs-frontend-api.h>

/* ── Constructor / Destructor ──────────────────────────────────────────── */

MultistreamOutputManager::MultistreamOutputManager()
{
}

MultistreamOutputManager::~MultistreamOutputManager()
{
	StopAllOutputs();
}

/* ── Start All Outputs ─────────────────────────────────────────────────── */

bool MultistreamOutputManager::StartAllOutputs()
{
	/* 1. Get enabled targets from config */
	auto targets = GetEnabledTargets();
	if (targets.empty()) {
		MCDN_LOG(LOG_INFO,
			 "Multistream: no enabled targets, skipping");
		return false;
	}

	/* 2. Get the main streaming output to share its encoders */
	obs_output_t *main_output = obs_frontend_get_streaming_output();
	if (!main_output) {
		MCDN_LOG(LOG_WARNING,
			 "Multistream: main streaming output not available");
		return false;
	}

	obs_encoder_t *video_encoder =
		obs_output_get_video_encoder(main_output);
	obs_encoder_t *audio_encoder =
		obs_output_get_audio_encoder(main_output, 0);

	if (!video_encoder || !audio_encoder) {
		MCDN_LOG(LOG_WARNING,
			 "Multistream: main encoders not ready yet");
		obs_output_release(main_output);
		return false;
	}

	/* 3. Stop any previously active outputs (safety) */
	StopAllOutputs();

	/* 4. Create and start each target independently */
	int started = 0;
	for (const auto &t : targets) {
		if (CreateAndStartOutput(t, video_encoder, audio_encoder)) {
			started++;
			MCDN_LOG(LOG_INFO,
				 "Multistream: started output for %s",
				 t.display_name.c_str());
		} else {
			MCDN_LOG(LOG_WARNING,
				 "Multistream: failed to start output for %s",
				 t.display_name.c_str());
		}
	}

	obs_output_release(main_output);

	MCDN_LOG(LOG_INFO, "Multistream: %d/%zu outputs started", started,
		 targets.size());
	return started > 0;
}

/* ── Stop All Outputs ──────────────────────────────────────────────────── */

void MultistreamOutputManager::StopAllOutputs()
{
	for (auto &ao : m_activeOutputs) {
		if (ao.output) {
			if (obs_output_active(ao.output)) {
				obs_output_stop(ao.output);
				MCDN_LOG(LOG_INFO,
					 "Multistream: stopped output for %s",
					 ao.display_name.c_str());
			}
			obs_output_release(ao.output);
			ao.output = nullptr;
		}
		if (ao.service) {
			obs_service_release(ao.service);
			ao.service = nullptr;
		}
	}

	m_activeOutputs.clear();
	MCDN_LOG(LOG_INFO, "Multistream: all outputs stopped and released");
}

/* ── Query State ───────────────────────────────────────────────────────── */

bool MultistreamOutputManager::IsOutputActive(
	const std::string &platform_id) const
{
	for (const auto &ao : m_activeOutputs) {
		if (ao.platform_id == platform_id && ao.output)
			return obs_output_active(ao.output);
	}
	return false;
}

std::vector<MultistreamOutputStatus>
MultistreamOutputManager::GetOutputStatuses() const
{
	std::vector<MultistreamOutputStatus> result;
	for (const auto &ao : m_activeOutputs) {
		result.push_back({ao.platform_id, ao.display_name,
				  ao.output ? obs_output_active(ao.output)
					    : false});
	}
	return result;
}

/* ── Create and Start Single Output ────────────────────────────────────── */
/*
 * This is the core pattern from obs-multi-rtmp and Aitum Multistream:
 *
 *   obs_service_create("rtmp_custom", ...)
 *   obs_output_create("rtmp_output", ...)
 *   obs_output_set_video_encoder(out, SHARED_venc)
 *   obs_output_set_audio_encoder(out, SHARED_aenc, 0)
 *   obs_output_set_service(out, svc)
 *   obs_output_start(out)
 */

bool MultistreamOutputManager::CreateAndStartOutput(
	const MultistreamTarget &target, obs_encoder_t *video_encoder,
	obs_encoder_t *audio_encoder)
{
	/*
	 * Sanitize URL and key to prevent common user-input issues:
	 *   - Trim leading/trailing whitespace (copy-paste artifacts)
	 *   - Strip trailing slashes from URL (prevents rtmp://server//key)
	 *   - Strip leading slashes from key (same reason)
	 */
	std::string url = target.rtmp_url;
	std::string key = target.stream_key;

	/* Trim whitespace */
	auto trim = [](std::string &s) {
		size_t start = s.find_first_not_of(" \t\r\n");
		size_t end = s.find_last_not_of(" \t\r\n");
		if (start == std::string::npos) {
			s.clear();
		} else {
			s = s.substr(start, end - start + 1);
		}
	};
	trim(url);
	trim(key);

	/* Strip trailing slashes from URL */
	while (!url.empty() && url.back() == '/')
		url.pop_back();

	/* Strip leading slashes from key */
	while (!key.empty() && key.front() == '/')
		key.erase(key.begin());

	MCDN_LOG(LOG_INFO, "Multistream: %s → server='%s'",
		 target.display_name.c_str(), url.c_str());

	/* Build service settings */
	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "server", url.c_str());
	obs_data_set_string(settings, "key", key.c_str());

	/* Create service name like "mcdn-multistream-twitch" */
	std::string svc_name = "mcdn-multistream-" + target.platform_id;
	std::string out_name = "mcdn-output-" + target.platform_id;

	/* Create the RTMP custom service */
	obs_service_t *service = obs_service_create(
		"rtmp_custom", svc_name.c_str(), settings, nullptr);
	obs_data_release(settings);

	if (!service) {
		MCDN_LOG(LOG_ERROR,
			 "Multistream: failed to create service for %s",
			 target.display_name.c_str());
		return false;
	}

	/* Create the RTMP output */
	obs_output_t *output = obs_output_create(
		"rtmp_output", out_name.c_str(), nullptr, nullptr);
	if (!output) {
		MCDN_LOG(LOG_ERROR,
			 "Multistream: failed to create output for %s",
			 target.display_name.c_str());
		obs_service_release(service);
		return false;
	}

	/* Attach shared encoders (zero extra CPU — this is the key trick) */
	obs_output_set_video_encoder(output, video_encoder);
	obs_output_set_audio_encoder(output, audio_encoder, 0);

	/* Attach the service */
	obs_output_set_service(output, service);

	/* Start the output */
	bool ok = obs_output_start(output);
	if (!ok) {
		const char *err = obs_output_get_last_error(output);
		MCDN_LOG(LOG_WARNING,
			 "Multistream: obs_output_start failed for %s: %s",
			 target.display_name.c_str(),
			 err ? err : "unknown error");
		obs_output_release(output);
		obs_service_release(service);
		return false;
	}

	/* Track the active output */
	m_activeOutputs.push_back({target.platform_id, target.display_name,
				   output, service});

	return true;
}
