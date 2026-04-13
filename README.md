# MidCircuitCDN OBS Plugin

An OBS Studio plugin that provides **1-click Connect** to MidCircuitCDN via Discord OAuth and built-in **multistreaming** to Twitch, Kick, YouTube, and X. Once authenticated, the plugin automatically configures your RTMP stream settings — no manual server URLs or stream keys required.

## Features

- **Discord OAuth Login** — Authenticate directly from within OBS using your Discord account.
- **Auto-Configuration** — RTMP ingest URL and stream key are set automatically after login.
- **Zero Configuration** — Just click Connect and start streaming.
- **Multistreaming** — Stream to MidCircuitCDN and up to 4 other platforms simultaneously:
  - **Twitch** — Enter your stream key and go.
  - **Kick** — Paste your server URL and stream key from the Kick Creator Dashboard.
  - **YouTube** — Enter your stream key from YouTube Studio.
  - **X** — Paste your server URL and stream key from X Media Studio.
- **Zero Extra CPU** — Multistream outputs share the main encoder, so there is no additional encoding overhead.
- **Auto Start/Stop** — Enabled multistream destinations start automatically when you begin streaming and stop when you end.
- **Persistent Keys** — Stream keys are saved and persist across sessions and MidCircuitCDN disconnects.

## Multistream Setup

1. Connect to MidCircuitCDN via the plugin dock panel.
2. Click the **↔ Multistream** button.
3. Enable the platforms you want to stream to.
4. Enter your stream key (and server URL for Kick/X).
5. Click **Save**.
6. Start streaming — all enabled destinations go live automatically.

> **Note:** Twitch and YouTube use standard global ingest URLs. Kick and X require a per-user server URL that you copy from your platform dashboard.

## Requirements

- **OBS Studio 30.0 or later** (built against the OBS 30.x API)
- Windows x64

## Installation

1. Download the latest `.zip` from the [Releases](https://github.com/oofz/MidCircuitCDN-obs-plugin/releases) page.
2. Extract the zip contents into your OBS Studio installation directory (e.g. `C:\Program Files\obs-studio\`).
   - This will place the plugin DLL into `obs-plugins/64bit/`.
3. Restart OBS Studio.
4. The MidCircuitCDN dock will appear — click **Connect** to authenticate via Discord.

## Building from Source

```bash
# Clone
git clone https://github.com/oofz/MidCircuitCDN-obs-plugin.git
cd MidCircuitCDN-obs-plugin

# Download OBS SDK into deps/
# (The CI workflow does this automatically)

# Configure & Build (MSVC + Qt6)
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

## License

This plugin is licensed under the **GNU General Public License v2.0** — see [LICENSE](LICENSE) for details.
