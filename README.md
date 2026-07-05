# SPACE INVADERS: WE HAVE DEMANDS

A self-aware parody of *Space Invaders*, written in C++20 + [raylib](https://www.raylib.com/)
5.5. The invaders know they're stuck in a 1978 video game, they have opinions about it,
and they would like to speak to whoever is in charge.

- **Genre:** fixed-shooter arcade (Classic+ variant)
- **Platforms:** Windows, Linux, macOS
- **Assets:** none — all art is vector primitives, all audio is synthesized at startup
- **A run:** endless escalating waves, ~5–15 minutes, high-score chase

The faithful arcade core is kept sacred (single shot on screen, marching 5×11 grid that
speeds up as it thins, destructible bunkers, mystery UFO). The "+" adds power-up drops,
per-wave comedic **modifiers**, **boss waves** every 5th wave, joke **achievements**, and
a persistent high-score table. Jokes are always ambient — nothing blocks input or hides a
mechanic.

## Build & Run

Requires CMake and a C++20 compiler. The first configure clones raylib via
`FetchContent`, so it needs network access once.

### Windows

```powershell
cmake -S . -B build                    # picks Visual Studio 17 2022
cmake --build build --config Release
.\build\Release\space_invader_plus.exe
```

### Linux / macOS

```bash
cmake -S . -B build && cmake --build build
./build/space_invader_plus
```

Linux also needs X11/GL/ALSA dev packages:

```bash
sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev \
                 libxi-dev libgl1-mesa-dev libasound2-dev
```

## Controls

| Input | Action |
|---|---|
| ← / → or A / D | Move cannon |
| Space | Fire |
| Enter | Confirm / start |
| Esc / P | Pause (Esc on title quits) |

## Architecture

A single `Game` struct in [src/game.h](src/game.h) holds all playing state; subsystem
`.cpp` files (`invaders.cpp`, `player.cpp`, `boss.cpp`, …) implement free functions
operating on `Game&`. Screens are an enum switch in [src/main.cpp](src/main.cpp) — no
state pattern, no ECS, no inheritance, no exceptions for game flow.

- **Timestep:** clamped variable timestep (`dt = min(GetFrameTime(), 1/30)`); the invader
  march is timer-stepped inside it.
- **Entities:** plain structs in arrays/vectors; removal via swap-and-pop / `erase_if`.
- **Collisions:** all pairings resolved in `ResolveCollisions` ([src/game.cpp](src/game.cpp)).
- **Wave modifiers:** a data table of flags/params checked at hook sites — not classes.
- **Rendering:** a fixed 800×950 logical canvas drawn to a `RenderTexture`, scaled to the
  window, with layered bloom + CRT post-processing ([src/postfx.cpp](src/postfx.cpp)).
- **Audio:** 100% pre-rendered to `Sound` buffers at init ([src/audio.cpp](src/audio.cpp)) —
  the game is single-threaded by design.
- **Persistence:** high scores and achievements are stored in the platform config dir
  ([src/platform_paths.cpp](src/platform_paths.cpp)).

Tuning constants live in [src/config.h](src/config.h); all joke text lives in
[src/content.h](src/content.h). The content source of truth is
[docs/GAME_DESIGN.md](docs/GAME_DESIGN.md), and [docs/blog/](docs/blog/) documents the
build.

## License

See the repository for license details.
