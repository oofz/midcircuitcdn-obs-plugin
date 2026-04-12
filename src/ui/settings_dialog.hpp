/*
 * MidcircuitCDN OBS Plugin — Settings Dialog
 * ────────────────────────────────────────────────────────────────────────────
 * Provides the "Connect MidcircuitCDN" UI accessible from the OBS Tools menu.
 * Displays connection status and triggers the OAuth flow.
 *
 * Note: This uses OBS's built-in properties API rather than raw Qt
 * to avoid Qt version compatibility issues. The properties API creates
 * a native OBS dialog that works across OBS versions.
 */

#pragma once

/*
 * Register the "MidcircuitCDN Settings" item in the OBS Tools menu.
 * Should be called from obs_module_load().
 */
void RegisterSettingsMenu();

/*
 * Clean up the menu registration.
 * Should be called from obs_module_unload().
 */
void UnregisterSettingsMenu();
