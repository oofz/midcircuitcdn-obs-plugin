/*
 * MidcircuitCDN OBS Plugin — Local Credential Store
 * ────────────────────────────────────────────────────────────────────────────
 * Persists stream credentials to the OBS global config so they survive
 * restarts. Stored under the [MidcircuitCDN] section in OBS's global.ini.
 */

#pragma once

#include "plugin_oauth.hpp"
#include <string>

/*
 * Save credentials to OBS global config.
 */
void SaveCredentials(const PluginCredentials &creds);

/*
 * Load credentials from OBS global config.
 * Returns true if valid credentials were found.
 */
bool LoadCredentials(PluginCredentials &out_creds);

/*
 * Clear stored credentials (disconnect).
 */
void ClearCredentials();

/*
 * Returns true if valid credentials are currently stored.
 */
bool HasStoredCredentials();
