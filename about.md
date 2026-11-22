# Sorrow Integration

by **endrosity** - roulette + death-stat bridge for your Geometry Dash Discord bot.

## What it does

- **Roulette in game.** When the bot gives you a roulette session (solo or group), the mod shows a
  small draggable HUD: level name, target percent, your best this level, deaths, group code.
- **Authoritative runs.** Deaths and completions are detected in game (`PlayLayer::destroyPlayer` /
  `levelComplete`, First priority like Death Tracker). Practice mode, test mode and cheated runs are
  never reported. Results go to your bot over the HTTP bridge - no screenshot OCR needed.
- **Group roulette over Globed.** If you are in a Globed room, the room owner's mod relays the group
  code to everyone in the session with Globed Server Events (`endrosity.sorrow-integration/start`),
  members auto-join (`/join`), and progressive level changes are broadcast (`/next`). Works even when
  the bot only knows Discord users, because every member reports with their own Discord id.
- **Death stats.** Deaths accumulate server-side in `data/gd_sorrow_stats.json`; `/gd stats` shows
  totals, per-level breakdowns and attempt streaks.
- **Plays nice with Death Tracker** (`elohmrow.death_tracker`, optional dep): both mods hook the same
  functions; Sorrow only adds its own reporting and HUD and never touches DT's save data.

## Setup

1. In the bot: `/gd config set sorrowToken <secret>` (bot admins), then start the bridge with
   `startSorrowBridge({ port: 8787 })` from `commands/gd.js` (exported by that file).
2. In the mod settings: Bridge URL (`http://yourbot-host:8787`), Bridge Token, and your Discord User ID.
3. Optional: Globed room roulette - join a room, start `/gd grouproulette` in Discord with the room
   code option, press Start. Room-mates with the mod join automatically.

## Building

Requires the [Geode SDK](https://docs.geode-sdk.org) (5.10.0+) and GD 2.2081 headers.

```bash
export GEODE_SDK=/path/to/geode-sdk
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Or push the repo to GitHub: `.github/workflows/build.yml` builds all four platforms (Win, macOS,
Android 32/64, iOS) and attaches the `.geode` artifacts to the run.

## Files

- `src/main.cpp` - PlayLayer hooks, session lifecycle, Globed listeners
- `src/Hud.hpp` - draggable HUD with fade in/out
- `src/Bridge.hpp` - HTTP bridge client (all endpoints from the bot contract)
- `src/GlobedLink.hpp` - soft-link API + Server Event encode/decode
- `src/Store.hpp` - session state and settings
