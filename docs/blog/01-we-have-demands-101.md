# We Have Demands, and So Does the Compiler

*Post 1 of a series on the engineering behind* **SPACE INVADERS: WE HAVE DEMANDS**.

---

The aliens have read their contract. They have been descending, one row at a time,
since 1978, and they would like to point out that the cannon respawns and they do not.
They are not evil. They are underpaid, self-aware, and mildly annoyed, and they would
like to speak to whoever is in charge.

That is the game. This is the series about how it is built — and, since you are an
engineer and not a middle-manager crab, we are going to skip the part where we pretend
the jokes wrote themselves. They did not. They are `constexpr` arrays. Everything here
is code, and this first post is the org chart for all of it.

## What the game actually is

*We Have Demands* is a fixed-shooter arcade game — a "Classic+" take on the 1978
original. The 1978 core is treated as sacred: one player shot on screen at a time, a
5×11 grid that marches sideways and **speeds up as you thin it out** (the classic panic
curve), four destructible bunkers, a mystery UFO, and instant game-over if the grid
reaches the cannon row.

The "+" is a comedy layer that is load-bearing but never obstructive. The invaders are
an org chart — Squid Executives up top who "complain the most and do the least," Octopus
Interns at the bottom who are "not even paid." Power-ups arrive as corporate memos.
Wave modifiers are labor actions. The governing rule, stated in the design doc and
enforced everywhere: **every joke that changes the screen also changes the game.**
"Opposite Day" doesn't just flavor a wave — it inverts your controls, because your
keyboard has unionized. When you lose, the game does not say GAME OVER. It says:

> The invaders thank you for your service and your quarter.

The voice is deadpan corporate throughout — HR memos, arbitration, "legally required."
That register is a design constraint, and, as we will see, it is also an architectural
one: because the jokes are *data*, the whole comedy layer lives in a single header and
never leaks into the logic.

## The technical thesis

Two through-lines explain nearly every decision in this codebase. Keep them in mind;
the rest of the series is just five ways of proving them.

1. **One self-contained binary.** Exactly one dependency (raylib, pinned), fetched and
   built from source. **Zero asset files** — every pixel is a vector primitive drawn at
   runtime, every sound is synthesized at startup. The shipped artifact is one
   executable with nothing to locate, load, or lose. There is no `assets/` directory
   because there is nothing to put in it.

2. **Aggressively single-threaded simplicity.** One `Game` struct holds all playing
   state. Subsystems are free functions over `Game&`. Screens are an `enum` and a
   `switch`. No ECS, no inheritance hierarchy, no state pattern, no exceptions for game
   flow, no function-static variables, and — deliberately — no fixed-timestep
   accumulator. The audio thread is never touched, because there is no audio thread.

Neither of these is an accident, and neither is dogma for its own sake. They are the
two invariants that keep a solo-authored game legible. Let's look at how they show up.

## Stack at a glance

C++20 and raylib 5.5. That's the whole stack. raylib is pulled in with FetchContent and
pinned to an exact tag — no submodule, no vendored tree, no system package to hunt for:

```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(raylib)
```

`GIT_SHALLOW TRUE` skips the history; `BUILD_EXAMPLES`/`BUILD_GAMES OFF` builds only the
library. The first configure needs the network once; after that, deleting `build/`
re-downloads it. That is the entire "getting started."

Crucially, raylib is treated as a **thin platform layer, not an engine**. It gives us a
window, input, an audio device, immediate-mode 2D drawing, render textures, and a shader
pipeline. There is no framework sitting on top of it — no `Entity` base class, no scene
graph. The game *is* the free functions and the one struct. raylib is the part that
talks to the OS so we don't have to.

One tooling note that pays for itself immediately: the strict warnings are scoped to the
game target *only*, never globally, because raylib's C won't compile clean under them:

```cmake
if(MSVC)
  target_compile_options(space_invader_plus PRIVATE /W4 /permissive-)
  target_compile_definitions(space_invader_plus PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(space_invader_plus PRIVATE -Wall -Wextra)
endif()
```

We hold our own code to `/W4 /permissive-` (MSVC's strict-conformance mode) and
`-Wall -Wextra`, and let the dependency build the way its author intended. That is what
`PRIVATE` is for.

## A tour of the pillars

### One `Game` struct, and functions that operate on it

Here is the load-bearing data structure. All mutable playing state is one flat
aggregate:

```cpp
struct Game {
    Player player{};
    std::array<Invader, cfg::kGridCount> invaders{};  // index = row * kGridCols + col
    int aliveCount = 0;
    float marchTimer = 0.0f;
    int marchDir = 1;
    // ... march animation, bomb timing ...

    std::vector<Shot> shots;     // player + enemy, flag disambiguates
    std::array<Bunker, cfg::kBunkerCount> bunkers{};
    std::vector<PowerUp> pickups;
    ActiveEffects fx{};
    Ufo ufo{};
    Boss boss{};
    WaveState wave{};
    std::vector<Particle> particles;
    std::vector<Star> stars;     // parallax background, seeded in ResetRun
    UiFx uifx{};

    int score = 0;
    int lives = cfg::kStartLives;
    bool gameOver = false;
    // ... shake, run time, achievement watchers ...
    Rng rng{};

    AudioBank* audio = nullptr;  // owned by main
};
```

There are no methods on `Game`. Behavior lives in free functions, and the header reads
like a table of contents grouped by owning `.cpp` file:

```cpp
// ---- game.cpp ----
void ResetRun(Game& g);
void StartWave(Game& g, int number);
void UpdatePlaying(Game& g, float dt);
void DrawPlaying(const Game& g);
void ResolveCollisions(Game& g);
void AddScore(Game& g, int points);

// ---- invaders.cpp ----
void SpawnGrid(Game& g);
void UpdateInvaders(Game& g, float dt);
void KillInvader(Game& g, int idx);
```

Fixed arrays for bounded sets (55 invaders, 4 bunkers), `std::vector` for the unbounded
ones (shots, particles), a hand-rolled xorshift `Rng` seeded once at startup — no
`<random>`, no globals. State and behavior are cleanly separable, which means "start a
new run" is almost literally `g = Game{}` (restoring the few borrowed handles after).
Post 4 is entirely about why this shape scales further than it has any right to.

### The loop, and a variable timestep with a leash

Everything hangs off one loop. Note the second line:

```cpp
while (!WindowShouldClose() && screen != Screen::Quit) {
    float dt = std::min(GetFrameTime(), cfg::kDtClamp);   // 1/30 s ceiling
    screenTimer += dt;
    TickMusic(audio);

    Screen next = screen;

    BeginTextureMode(canvas);
    BeginMode2D(ssCam);
    switch (screen) {
    case Screen::Title:
        next = UpdateDrawTitle(g, hs, screenTimer);
        break;
    case Screen::Playing:
        UpdatePlaying(g, dt);
        DrawPlaying(g);
        // ...
```

This is a **variable timestep** — `pos += vel * dt`, no fixed-step accumulator. The
project rule is explicit: don't add one. But an unbounded `dt` is a spiral-of-death
waiting to happen: one alt-tab stall and a shot teleports straight through a bunker.
So `dt` is clamped to `cfg::kDtClamp` (1/30 s). Simulation slows below 30 FPS rather
than tunneling — the correct trade for a game where nothing needs sub-frame collision
determinism.

The invader march is the one thing *not* integrated against `dt`. It's a timer stepped
at an interval that interpolates from slow (full grid) to fast (last invader alive)
based on `aliveCount`. That's how you get the 1978 acceleration curve without coupling
game feel to frame rate — and how the four-note bassline stays locked to the march,
because the same timer advances the note index.

### Drawing with no pictures

There are no sprites. The neon look is faked in software: `DrawGlow*` helpers stack two
or three translucent halos under each solid shape, and the invaders are drawn from
`constexpr uint8_t[2][6]` bitmask tables (two march frames per archetype) with per-cell
shading and animated eye glints.

All of it is drawn into a fixed **800×950 logical canvas**, itself rasterized at **2×
supersample** into a `RenderTexture2D` under a `zoom = 2` camera, so the game stays crisp
at any window size:

```cpp
RenderTexture2D canvas = LoadRenderTexture(cfg::kCanvasW * cfg::kSupersample,
                                           cfg::kCanvasH * cfg::kSupersample);
Camera2D ssCam{};
ssCam.zoom = (float)cfg::kSupersample;
```

That canvas then goes through a GPU post-processing stage — bloom plus a CRT composite
(barrel distortion, chromatic aberration, scanlines, vignette) — compiled from inline
GLSL strings, with a graceful fallback to a plain blit if the shaders won't compile on a
given machine. All of that is **post 2**.

### Sound with no files

Same story, different medium. There are no `.wav` or `.ogg` files. At startup, every
sound effect is synthesized — a tiny additive synth (`Osc` oscillators, an ADSR-ish
`Voice`) renders into float buffers, normalizes, converts to `int16`, and hands raylib a
`Wave` to `LoadSoundFromWave`. Sixteen-odd effects and a 16-second music loop, all baked
into `Sound` buffers before the title screen appears.

And there is a hard rule attached: **never** use `SetAudioStreamCallback`. That callback
runs on miniaudio's audio thread, and this game is single-threaded by design — feeding
audio from a background thread would break the "no shared mutable state" invariant the
whole codebase is built on. So instead of streaming, everything is pre-baked and simply
triggered. **Post 3** takes the synth apart voice by voice.

### Behavior as data

The last pillar is the one that keeps the other four honest: content and tuning are data,
not code. Two headers enforce it.

`config.h` holds every tuning constant, and opens with the house rule:

> All tuning constants live here. If you're typing a number into a logic file, stop.

`content.h` holds every string. Jokes are `constexpr` arrays, categorized by trigger —
power-up toasts, ambient speech-bubble lines, boss dialogue, the system voice:

```cpp
inline constexpr const char* kToastFreeze = "UNION-MANDATED BREAK - legally required.";
inline constexpr const char* kToastRapid  = "ESPRESSO OVERRIDE - HR has been notified.";
```

And wave modifiers — the labor actions — are a plain POD table. A `Modifier` is a struct
of flags and multipliers (`invertInput`, `invisibleInvaders`, `complimentBombs`,
`mirrorCannon`, `scale`, `scoreMult`, …), one row per modifier, queried at hook sites
throughout the code. No `Modifier` subclasses, no virtual dispatch — a `switch`-free
data table and a shuffle-bag that picks the next one without repeats. This is what lets
the comedy be edited, extended, and tone-audited in one file without touching the engine.
**Posts 4 and 5** dig into the modifier system and the content/logic split.

## What we deliberately didn't build

The negative space is half the design. There is:

- **No ECS.** Plain structs in arrays and vectors; removal via swap-and-pop / `erase_if`.
- **No state pattern.** Screens are an `enum` and a `switch`.
- **No inheritance hierarchy** and **no exceptions for game flow.**
- **No function-static variables** anywhere — state is always passed explicitly.
- **No fixed-timestep accumulator** — the clamped variable step is the whole story.
- **No settings screen, no difficulty menu.** The game *is* the difficulty setting.

Each omission is load-bearing. The absence of an audio callback is why there's no
threading. The absence of function statics is why `ResetRun` is a near-trivial
`g = Game{}`. The constraints reinforce each other.

## Coming up

The rest of the series takes each pillar apart:

- **2 — Drawing with no pictures.** The `DrawGlow*` layered-halo look, bitmask invaders,
  and the inline-GLSL bloom + CRT post-processing chain (with its compile-failure
  fallback).
- **3 — Every sound is a function.** The startup synth: oscillators, voices, baking to
  `Wave`, the tempo-tracking bassline, and why `SetAudioStreamCallback` is forbidden.
- **4 — One struct, many functions.** The data-oriented architecture in full: the `Game`
  struct, free functions, the screen `switch`, and `ResolveCollisions`.
- **5 — Behavior as data.** `config.h` / `content.h` and the wave-modifier POD table —
  comedy and tuning expressed as data, checked at hook sites.
- **6 — One binary, three platforms.** CMake and FetchContent, scoped warnings, per-OS
  packaging, the one-line `.clangd`, and the `DEBUG_KEYS` inner loop.

---

The invaders would like it noted that they did not consent to being described as "plain
structs." Their grievance has been logged, categorized, and stored in a `constexpr`
array, where — like every complaint in this game — it will be read exactly when the code
decides to read it, and not one frame sooner.

*Next: how to draw a convincing arcade with no artist and no images.*
