# MidCircuitCDN OBS Plugin

An OBS Studio plugin that provides **1-click Connect** to MidcircuitCDN via Discord OAuth. Once authenticated, the plugin automatically configures your RTMP stream settings — no manual server URLs or stream keys required.

## Features

- **Discord OAuth Login** — Authenticate directly from within OBS using your Discord account.
- **Auto-Configuration** — RTMP ingest URL and stream key are set automatically after login.
- **Zero Configuration** — Just click Connect and start streaming.

## Requirements

- **OBS Studio 30.0 or later** (built against the OBS 30.x API)
- Windows x64

## Installation

1. Download the latest `.zip` from the [Releases](https://github.com/oofz/midcircuitcdn-obs-plugin/releases) page.
2. Extract the zip contents into your OBS Studio installation directory (e.g. `C:\Program Files\obs-studio\`).
   - This will place the plugin DLL into `obs-plugins/64bit/`.
3. Restart OBS Studio.
4. The MidcircuitCDN dock will appear — click **Connect** to authenticate via Discord.

## Building from Source

```bash
# Clone
git clone https://github.com/oofz/midcircuitcdn-obs-plugin.git
cd midcircuitcdn-obs-plugin

# Download OBS SDK into deps/
# (The CI workflow does this automatically)

# Configure & Build (MSVC + Qt 6.7)
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

## License

This plugin is licensed under the **GNU General Public License v2.0** — see [LICENSE](LICENSE) for details.

