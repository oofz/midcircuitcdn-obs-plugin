/*
 * MidCircuitCDN OBS Plugin — Shared Macros
 */

#pragma once

#define PLUGIN_NAME "MidCircuitCDN-obs-plugin"
#define PLUGIN_VERSION "0.3.0"

/* Logging convenience macro: blog(LOG_INFO, "[MidCircuitCDN] %s", msg) */
#ifndef MCDN_LOG
#define MCDN_LOG(level, format, ...) \
	blog(level, "[MidCircuitCDN] " format, ##__VA_ARGS__)
#endif
