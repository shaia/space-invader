# Dev Blog — SPACE INVADERS: WE HAVE DEMANDS

A series on the engineering behind the game, from a tools-and-technology angle.
Audience: C++ / graphics engineers. Written in the game's own deadpan register.

Two through-lines run under everything: **one self-contained binary** (a single pinned
dependency, zero asset files) and **aggressively single-threaded simplicity** (one `Game`
struct, free functions, no audio thread). Each post takes one pillar apart.

## Posts

1. [We Have Demands, and So Does the Compiler](01-we-have-demands-101.md) — the 101: what
   the game is, the technical thesis, and a guided tour of the pillars.
2. [Drawing With No Pictures](02-drawing-with-no-pictures.md) — the software neon look
   (`GlowRect`, bitmask invaders) and the inline-GLSL bloom + CRT post-processing chain,
   with its compile-failure fallback.
3. [Every Sound Is a Function](03-every-sound-is-a-function.md) — startup audio synthesis,
   the `Osc`/`Voice` synth, baking to `Wave`, the tempo-tracking march bassline, and why
   `SetAudioStreamCallback` is banned.
4. [One Struct, Many Functions](04-one-struct-many-functions.md) — the data-oriented
   architecture: the flat `Game` struct, free functions over `Game&`, the screen `enum`
   switch, `ResetRun`, `UpdatePlaying`, and the single `ResolveCollisions`.
5. [Behavior as Data](05-behavior-as-data.md) — `config.h` / `content.h` and the
   wave-modifier POD table: comedy and tuning expressed as data, checked at hook sites,
   not classes.
6. [One Binary, Three Platforms](06-one-binary-three-platforms.md) — the toolchain finale:
   CMake + FetchContent, warnings scoped to the game target, the `windows.h` quarantine,
   per-OS packaging, the one-line `.clangd`, and the `DEBUG_KEYS` inner loop.

*Series complete.*

## Deep dives

Supplementary teardowns that go past the series' altitude into the low-level detail.

- [The Bloom + CRT Pipeline, to the Metal](deep-dive-bloom-and-crt.md) — a graphics-engineer
  teardown of the three post-processing shaders: LDR-vs-HDR bloom, gamma-space blur
  correctness, the Gaussian weight math and the bilinear-tap optimization it leaves unused,
  the CRT distortion/scanline/vignette math, a per-pass sample-count performance model, and
  a scorecard of deliberate shortcuts vs. free wins vs. genuine artifacts.
- [The Synthesizer, to the Sample](deep-dive-audio-synthesis.md) — a DSP teardown of the
  startup synth: phase-accurate oscillators, the square/saw aliasing problem and the PolyBLEP
  fix, envelope behavior (and the release default that swallows short sounds), peak-vs-loudness
  normalization, 16-bit quantization/dither, playback-time resampling and retrigger, the
  click-safe loop seam, and the same shortcuts-vs-free-wins scorecard.
