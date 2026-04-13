/*
 * MidCircuitCDN OBS Plugin — Encoder Config
 * ────────────────────────────────────────────────────────────────────────────
 * Hooks into the video encoder to cap bitrate at the MidCircuitCDN limit.
 */

#pragma once

/* Default bitrate cap for MidCircuitCDN streams (kbps) */
constexpr int MCDN_TARGET_BITRATE_KBPS = 3000;

/*
 * Cap the active video encoder's bitrate.
 *
 * Uses the OBS C-API:
 *   obs_frontend_get_streaming_output()
 *   obs_output_get_video_encoder(output)
 *   obs_encoder_get_settings(encoder)
 *   obs_data_set_int(settings, "bitrate", max_kbps)
 *
 * @param max_bitrate_kbps  Maximum video bitrate in kbps (default: 3000)
 */
void CapVideoBitrate(int max_bitrate_kbps = MCDN_TARGET_BITRATE_KBPS);
