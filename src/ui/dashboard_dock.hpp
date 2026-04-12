/*
 * MidcircuitCDN OBS Plugin — Dashboard Dock
 * ────────────────────────────────────────────────────────────────────────────
 * Registers a custom browser dock inside OBS that loads the MidcircuitCDN
 * dashboard. Users can manage restreaming targets (Twitch, YouTube, Kick)
 * directly from within OBS without opening a separate browser.
 */

#pragma once

/*
 * Register the MidcircuitCDN Dashboard browser dock.
 * Uses OBS's built-in CEF browser panel support.
 * Should be called from obs_module_load().
 */
void RegisterDashboardDock();

/*
 * Clean up the dock registration.
 * Should be called from obs_module_unload().
 */
void UnregisterDashboardDock();
