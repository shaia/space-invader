# SPACE INVADERS: WE HAVE DEMANDS

Self-aware parody Space Invaders variant. C++20 + raylib 5.5, cross-platform
(Windows/Linux/macOS), zero external assets — all art is vector primitives, all audio
is synthesized at startup. Content source of truth: `docs/GAME_DESIGN.md`.

## Build & Run

```powershell
cmake -S . -B build                        # Windows: picks Visual Studio 17 2022
cmake --build build --config Debug
.\build\Debug\space_invader_plus.exe

cmake --build build --config Release       # check before each phase-complete commit
.\build\Release\space_invader_plus.exe
```

Linux/macOS: `cmake -S . -B build && cmake --build build && ./build/space_invader_plus`
(Linux needs X11/GL/ALSA dev packages: `libx11-dev libxrandr-dev libxinerama-dev
libxcursor-dev libxi-dev libgl1-mesa-dev libasound2-dev`.)

First configure clones raylib via FetchContent — needs network once; deleting `build/`
re-downloads it.

## Architecture

Single `Game` struct holds all playing state (`src/game.h`); subsystem `.cpp` files
(`invaders.cpp`, `player.cpp`, `boss.cpp`, …) implement free functions declared in
`game.h` operating on `Game&`. Screens are an enum switch in `main.cpp` — no state
pattern, no ECS, no inheritance hierarchies, no exceptions for game flow.

- Clamped variable timestep: `dt = min(GetFrameTime(), 1/30f)`; invader march is
  timer-stepped inside it. No fixed-timestep accumulator — don't add one.
- Entities are plain structs in arrays/vectors; removal via swap-and-pop/`erase_if`.
- All collision pairings live in `ResolveCollisions` (`game.cpp`).
- Wave modifiers are a data table of flags/params checked at hook sites — not classes.
- Rendering targets a fixed 800×950 logical canvas in a RenderTexture, scaled to the
  window. Glow = layered draws via `DrawGlow*` helpers in `render.cpp`.
- Audio is 100% pre-rendered to `Sound` buffers at init (`audio.cpp`). NEVER use
  `SetAudioStreamCallback` — it runs on the miniaudio thread; the game is
  single-threaded by design.
- High scores + achievements persist in the platform config dir (`platform_paths.cpp`).

## Hard rules

- **All tuning constants** go in `src/config.h`. **All joke text** goes in
  `src/content.h`. No literals scattered in logic files.
- No TU may include both `raylib.h` and `windows.h` (symbol clashes: `Rectangle`,
  `CloseWindow`, `DrawText`, `PlaySound`). `platform_paths.cpp` uses `std::getenv` —
  keep windows.h out of the project entirely.
- Never use function-static variables (the audio thread rule above is why this repo
  cares); pass state explicitly.
- Compiler warnings (`/W4 /permissive-`, `-Wall -Wextra`) apply to the game target
  only — never globally (raylib's C won't build clean).
- Debug helpers live behind the `DEBUG_KEYS` flag in `config.h`: F1 stats overlay,
  F2 skip wave (kills bosses too), F3 grant power-up, F4 cycle modifier,
  F5 kill-all-but-one, F6 sacrifice a life.

## Git

- New commit per change; never `--amend`. No `Co-Authored-By` lines.
- Messages: concise, present tense, explain why ("clamp dt to stop bunker tunneling").

## Tone (for any new content)

Deadpan, dry, self-aware — the invaders are underpaid and mildly annoyed, never evil;
the game speaks in a corporate/legal voice. Jokes never block input or hide mechanics.
See the Tone Guide in `docs/GAME_DESIGN.md` §1.
