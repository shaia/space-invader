# One Binary, Three Platforms

*Post 6 — the finale — of a series on the engineering behind* **SPACE INVADERS: WE HAVE
DEMANDS**. *Previously: [Behavior as Data](05-behavior-as-data.md).*

---

Five posts in, the game exists: it draws itself from primitives, sings its own soundtrack,
runs on one flat struct, and stores its personality in tables. This last post is about the
part you never see and always feel — the toolchain that turns all of it into a *single
executable* that builds and runs on Windows, Linux, and macOS, with exactly one dependency
and no assets to ship alongside it.

The whole build is 72 lines of CMake, one `.clangd` file, one OS-aware source file, and a
compile-time debug flag. That's the entire "infrastructure." The invaders were promised a
CI pipeline, a release manager, and at least one all-hands about cross-platform strategy.
They got a `CMakeLists.txt` shorter than this blog post. Here's what's in it.

## One dependency, fetched and pinned

We saw this in post 1, but it's the foundation, so briefly: raylib is pulled in with
FetchContent, pinned to an exact tag, shallow-cloned, and stripped down to just the
library:

```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(raylib)
```

There is no submodule to forget to init, no system package to hunt for, no "works on my
machine because I have raylib installed." The version is in the file. The first configure
needs the network once; after that it's local. This is the single-self-contained-binary
thesis starting at the dependency graph: one dependency, and its exact identity is source
code.

The executable is then just the fifteen game translation units linking `raylib` privately.
Nothing else.

## The warnings only apply to *our* code

Here's the first real cross-platform subtlety, and it's a good one. The project holds its
own code to strict warnings — `/W4 /permissive-` on MSVC, `-Wall -Wextra` on GCC/Clang.
But raylib is a large C library that does not build clean under those flags. So the flags
are attached to the game target with `PRIVATE`, never globally:

```cmake
target_link_libraries(space_invader_plus PRIVATE raylib)

if(MSVC)
  target_compile_options(space_invader_plus PRIVATE /W4 /permissive-)
  # getenv is used deliberately (platform_paths.cpp) to keep windows.h out of the project
  target_compile_definitions(space_invader_plus PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(space_invader_plus PRIVATE -Wall -Wextra)
endif()
```

`PRIVATE` is the whole trick: our target compiles under `/permissive-` (MSVC's strict
conformance mode) and full warnings, while the FetchContent'd raylib builds however its
author intended. You get a strict conformance bar on the code you actually maintain
without trying to fix warnings in a dependency you don't. That `_CRT_SECURE_NO_WARNINGS`
define has a specific reason attached in the comment — and it points at the most
interesting file in the build.

## The `windows.h` quarantine

raylib and `windows.h` are mortal enemies. They both define `Rectangle`, `CloseWindow`,
`DrawText`, and `PlaySound`, so a translation unit that includes both doesn't compile.
raylib wins by fiat across this codebase: **`windows.h` is kept out of the entire
project.** But somebody still has to find the per-user config directory to save high
scores, and on Windows the "proper" way to do that is `SHGetKnownFolderPath` — from
`windows.h`.

The resolution is to not do it the proper way. One file — `platform_paths.cpp`, "the only
file that knows what OS this is" — resolves the config directory with plain `std::getenv`
and `std::filesystem`, no OS headers at all:

```cpp
// The only file that knows what OS this is. Uses getenv, never windows.h
// (raylib.h and windows.h fight over Rectangle/CloseWindow/DrawText/PlaySound).
fs::path ConfigDir() {
    fs::path dir;
#if defined(_WIN32)
    if (const char* appdata = std::getenv("APPDATA"))
        dir = fs::path(appdata) / "SpaceInvaderPlus";
    else dir = fs::path(".") / "SpaceInvaderPlus";
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME"))
        dir = fs::path(home) / "Library" / "Application Support" / "SpaceInvaderPlus";
    else dir = fs::path(".") / "SpaceInvaderPlus";
#else
    if (const char* xdg = std::getenv("XDG_DATA_HOME"))
        dir = fs::path(xdg) / "space-invader-plus";
    else if (const char* home = std::getenv("HOME"))
        dir = fs::path(home) / ".local" / "share" / "space-invader-plus";
    else dir = fs::path(".") / "space-invader-plus";
#endif
    std::error_code ec;
    fs::create_directories(dir, ec);   // best effort; Save() copes with failure
    return dir;
}
```

Three things worth noting. First, this is the *only* `#if defined(_WIN32)` in the whole
project — all platform variance is quarantined to one function, so the other forty-odd
files are blissfully OS-agnostic. Second, `getenv("APPDATA")` is exactly why
`_CRT_SECURE_NO_WARNINGS` exists in the CMake: MSVC deprecates `getenv`, we use it on
purpose, so we silence that one warning for the target. Third, every branch has a `"."`
fallback and the directory creation uses the non-throwing `std::error_code` overload — a
missing env var or an unwritable disk degrades to "save next to the exe" or "don't save,"
never a crash. The high-score file is a nicety, not a dependency.

That's cross-platform file I/O in thirty lines, no `#ifdef` maze, no platform layer, no
third-party path library.

## The binary is self-contained; the *packaging* is per-OS

The runtime artifact is one executable with zero asset files — but a game people install
should still get an icon and a menu entry. That's the one place the build forks by OS, and
it forks cleanly:

- **Windows** enables the `RC` language and compiles `res/app.rc` to embed the executable
  icon:
  ```cmake
  if(WIN32)
    enable_language(RC)
    target_sources(space_invader_plus PRIVATE res/app.rc)  # embeds the executable icon
  endif()
  ```
- **macOS** links the required frameworks and builds a real `.app` bundle, copying
  `app.icns` into `Resources/` so Finder and the Dock show the icon:
  ```cmake
  if(APPLE)
    target_link_libraries(space_invader_plus PRIVATE
      "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
    set_target_properties(space_invader_plus PROPERTIES
      MACOSX_BUNDLE TRUE MACOSX_BUNDLE_ICON_FILE app.icns)
  endif()
  ```
- **Linux** ships an XDG-compliant `install` rule — binary to `bin/`, a `.desktop` entry,
  and hicolor PNG icons at every standard size:
  ```cmake
  if(UNIX AND NOT APPLE)
    install(TARGETS space_invader_plus RUNTIME DESTINATION bin)
    install(FILES res/space_invader_plus.desktop DESTINATION share/applications)
    foreach(sz 16 32 48 64 128 256 512)
      install(FILES res/linux/hicolor/${sz}x${sz}/apps/space_invader_plus.png
        DESTINATION share/icons/hicolor/${sz}x${sz}/apps)
    endforeach()
  endif()
  ```

Note the important distinction from post 2 and 3: the files in `res/` are *packaging*
artifacts — icons and a desktop entry the OS reads — never assets the *game* loads at
runtime. The running binary still touches no file it didn't generate itself. The icon is
for the desktop, not the render loop.

## The IDE story is one file

Because raylib is fetched into `build/_deps/` rather than a system include path, an editor
language server has no idea where `raylib.h` is. The entire fix is a four-line `.clangd`:

```yaml
CompileFlags:
  Add:
    - -std=c++20
    - -Ibuild/_deps/raylib-src/src
```

Point clangd at the fetched raylib source and force C++20 so it doesn't diagnose the
project against the wrong standard. That's the whole IDE configuration — no `.vscode/`, no
generated `compile_commands.json` to keep in sync, no `.editorconfig`. One file, and
autocomplete and diagnostics light up for the one dependency the project has.

## Testability is a compile-time flag, not a framework

There is no test suite here, and that's a deliberate choice worth being honest about — for
a solo arcade game where "correct" means "feels right," an automated harness would cost
more than it returns. Instead, "testability" means *reaching any game state in seconds by
hand*, and that's what the `DEBUG_KEYS` flag buys. It's one define in `config.h`:

```cpp
// Compile-time debug helpers: F1 overlay, F2 skip wave, F3 powerup, F4 modifier, F5 kill-all-but-one
#define DEBUG_KEYS 1
```

Flip it to `0` and every debug block `#if`-compiles out of the shipping binary. With it on,
one function gives you the whole developer inner loop:

```cpp
#if DEBUG_KEYS
void DebugKeys(Game& g) {
    if (IsKeyPressed(KEY_F2)) { /* skip wave — kills the boss too */ }
    if (IsKeyPressed(KEY_F3)) ActivatePickup(g, (PowerKind)g.rng.irange(0, (int)PowerKind::COUNT - 1));
    if (IsKeyPressed(KEY_F4)) { /* force-cycle the wave modifier + announce */ }
    if (IsKeyPressed(KEY_F5)) { /* kill all invaders but one → fast end-of-wave march */ }
    if (IsKeyPressed(KEY_F6)) { g.player.invuln = 0; HitPlayer(g, "debug"); }  // test death flow
}
#endif
```

Want to test the boss? F2 to the wave. A specific modifier? F4 until it comes up — which,
per post 5, is just incrementing a `ModifierId`. The panicky fast-march at the end of a
wave? F5. The death-to-game-over path? F6. And F1 holds up a live telemetry overlay — FPS,
frame `dt`, alive/shots/particles counts, the active modifier, boss HP, power-up timers —
drawn straight in `DrawHud`. No rebuild, no external tool, no fixture. The flat-struct
architecture from post 4 is what makes this safe: because there's no hidden state, forcing
a game state by poking `Game` can't desync anything.

## The two threads that run through everything

Step back and the whole series is two decisions, restated six ways.

**One self-contained binary.** One pinned dependency (this post), zero asset files —
because the art is primitives (post 2), the audio is synthesized (post 3), and the content
and tuning are compiled-in tables (post 5). The shipped thing is a single executable with
nothing to locate, load, or lose, and a `"."`-fallback for the one file it optionally
writes.

**Aggressively single-threaded simplicity.** One `Game` struct and free functions (post 4)
— which is only *possible* because there's no audio callback thread (post 3), no function
statics anywhere (every post), and a toolchain that keeps the strict-conformance bar on
our code and `windows.h` out of the building (this post). The constraints don't fight each
other; each one makes the next one easier to hold.

That's the payoff of picking your constraints early and letting them compound. Vector art
made the neon look, which fed the bloom. Synthesized audio removed the audio thread, which
protected the no-statics rule, which made `ResetRun` a one-liner. Data tables kept comedy
out of logic, which kept the debug keys trivial. A 72-line build ships all of it,
identically, on three operating systems. None of it is clever in isolation. It's coherent,
which is better.

The invaders have reviewed the final build. They confirm it compiles, runs, and pays them
nothing, exactly as specified in the original 1978 design document. They consider the
matter closed and have returned to the top of the screen to begin again.

Ship it.

---

*That's the series. Six posts, one game, no assets, one binary. Thanks for reading —
[start from the top](README.md) if you came in at the end.*
