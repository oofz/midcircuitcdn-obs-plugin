/*
 * MidcircuitCDN OBS Plugin — Shared Macros
 */

#pragma once

#define PLUGIN_NAME "midcircuitcdn-obs-plugin"
#define PLUGIN_VERSION "0.3.0"

/* Logging convenience macro: blog(LOG_INFO, "[MidcircuitCDN] %s", msg) */
#ifndef MCDN_LOG
#define MCDN_LOG(level, format, ...) \
	blog(level, "[MidcircuitCDN] " format, ##__VA_ARGS__)
#endif
