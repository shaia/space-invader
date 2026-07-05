# Every Sound Is a Function

*Post 3 of a series on the engineering behind* **SPACE INVADERS: WE HAVE DEMANDS**.
*Previously: [Drawing With No Pictures](02-drawing-with-no-pictures.md).*

---

The game makes noise. A laser, a wet pop when an invader dies, a warbling UFO, a
descending four-note bassline that speeds up as the ranks thin, and a sad falling tone
when your cannon gets it. None of that is a file. There is no `.wav`, no `.ogg`, no audio
middleware. Every sound in the game is a small function that fills an array with floats,
and all of them run exactly once, at startup, before the title screen appears.

The invaders would like it noted that they asked for residuals on the bassline. They have
crossed this screen since 1978. They have not seen a cent. The bassline, like everything
else here, was generated for free by `audio.cpp`, which is the subject of this post.

## The whole thing is baked before you press start

The audio bank is a flat struct — an array of sound effects plus one music loop — and it
is filled in a single loop at init:

```cpp
void InitAudioBank(AudioBank& a) {
    if (!IsAudioDeviceReady()) return;
    for (int i = 0; i < (int)Sfx::COUNT; i++) {
        a.sfx[i] = MakeSfx((Sfx)i);
        SetSoundVolume(a.sfx[i], cfg::kMasterSfx);
    }
    a.music = MakeMusic(a.musicLen);
    SetSoundVolume(a.music, cfg::kMasterMusic);
    a.ready = true;
}
```

`Sfx` is a plain `enum class` — `Shoot, Pop, PlayerDie, UfoWarble, …, March0..March3` —
and `MakeSfx` is a big `switch` that returns a finished `Sound` for each one. After this
loop returns, there is no more synthesis for the rest of the process's life. Playback is
just triggering a buffer that already exists. That's the entire design: **synthesize
once, trigger forever.**

Everything downstream falls out of that decision. No streaming means no audio callback.
No audio callback means no second thread. No second thread means the "no shared mutable
state, no function statics" rule the whole codebase lives by holds for audio too. We'll
come back to that at the end, because it's the load-bearing constraint — but first, how
you actually turn a `switch` case into a laser.

## Three primitives: an oscillator, a voice, a bake

The synth is tiny. There is an oscillator enum, a deterministic noise source, a float
buffer, a voice that writes into it, and a bake step that hands the result to raylib.

**The noise source is a hash, not an RNG.** This is a small thing that says a lot about
the codebase:

```cpp
// deterministic noise so synthesis needs no shared rng state
float NoiseAt(int i) {
    unsigned n = (unsigned)i * 2654435761u;
    n ^= n >> 13;  n *= 2246822519u;  n ^= n >> 16;
    return ((float)(n & 0xFFFF) / 32768.0f) - 1.0f;
}
```

White noise is normally "call `rand()`." Here it's a pure function of the sample index —
same `i`, same value, no seed, no shared state. It exists in exactly this form because
the project bans function statics, and a stateful RNG in the synth would be one. The
constraint from post 1 reaches all the way down to how we make a hi-hat.

**The buffer** is just a `std::vector<float>` sized in seconds at the configured sample
rate (44100). Mixing happens in float, which means intermediate sums can blow past ±1 —
that's fine, the bake step deals with it:

```cpp
struct Buf {
    std::vector<float> s;
    explicit Buf(float seconds) : s((size_t)(seconds * cfg::kSampleRate), 0.0f) {}
};
```

**`Voice` is the synth.** One call adds one tone to the buffer: pick an oscillator, glide
the frequency from `f0` to `f1` over the duration, shape the amplitude with an
attack/release envelope, optionally add vibrato. It's additive — you layer `Voice` calls
to build a sound:

```cpp
void Voice(Buf& b, float t0, float dur, Osc osc, float f0, float f1, float amp,
           float attack = 0.004f, float release = 0.05f,
           float vibHz = 0.0f, float vibAmt = 0.0f) {
    int start = (int)(t0 * cfg::kSampleRate);
    int n = (int)(dur * cfg::kSampleRate);
    float phase = 0.0f;
    for (int i = 0; i < n; i++) {
        int idx = start + i;
        if (idx < 0 || idx >= (int)b.s.size()) break;
        float t = (float)i / cfg::kSampleRate;
        float f = f0 + (f1 - f0) * (t / dur);              // linear pitch glide
        if (vibHz > 0) f += sinf(t * kTau * vibHz) * vibAmt;
        phase += f / cfg::kSampleRate;
        float v = 0.0f;
        switch (osc) {
        case Osc::Sine:   v = sinf(phase * kTau); break;
        case Osc::Square: v = (fmodf(phase, 1.0f) < 0.5f) ? 1.0f : -1.0f; break;
        case Osc::Saw:    v = 2.0f * fmodf(phase, 1.0f) - 1.0f; break;
        case Osc::Noise:  v = NoiseAt(idx); break;
        }
        float env = 1.0f;
        if (t < attack) env = t / attack;                  // attack ramp in
        float tail = dur - t;
        if (tail < release) env *= tail / release;         // release ramp out
        b.s[idx] += v * amp * env;                         // additive mix
    }
}
```

Note it accumulates phase incrementally (`phase += f / rate`) rather than `sin(2πft)`, so
a frequency glide stays phase-continuous and doesn't click. The envelope is a cheap
piecewise ramp — linear in, linear out — which is all an arcade blip needs.

**`Bake` finishes the sound.** Because voices are summed in float and can clip, the bake
step normalizes to the buffer's peak, converts to signed 16-bit, and wraps it in a raylib
`Wave`:

```cpp
Sound Bake(const Buf& b, float gain) {
    float peak = 0.0001f;
    for (float v : b.s) peak = fmaxf(peak, fabsf(v));
    float k = gain / peak;
    if (k > gain) k = gain;                    // don't amplify silence

    int n = (int)b.s.size();
    short* data = (short*)RL_MALLOC(n * sizeof(short));   // raylib owns/frees it
    for (int i = 0; i < n; i++) {
        float v = b.s[i] * k;
        v = fmaxf(-1.0f, fminf(1.0f, v));
        data[i] = (short)(v * 32000.0f);
    }
    Wave w{ .frameCount = (unsigned)n, .sampleRate = cfg::kSampleRate,
            .sampleSize = 16, .channels = 1, .data = data };
    Sound snd = LoadSoundFromWave(w);
    UnloadWave(w);
    return snd;
}
```

One deliberate detail: the sample buffer is allocated with `RL_MALLOC`, not `new` or a
`vector`, so that `UnloadWave`/raylib frees it with the matching allocator. We're feeding
raylib's C API, so we allocate the way raylib expects to deallocate. After `Bake`, the
float buffer is gone and all that survives is a `Sound` handle.

## A laser, a death, and a pop

With those three pieces, each effect is three or four lines. The laser is a square wave
diving from 900 Hz to 260 Hz over 120 ms:

```cpp
case Sfx::Shoot: {
    Buf b(0.14f);
    Voice(b, 0, 0.12f, Osc::Square, 900, 260, 0.7f, 0.002f, 0.04f);
    return Bake(b, 0.9f);
}
```

The invader pop layers a tonal thud under a noise burst — a sine gliding 420→70 Hz with a
short noise transient on top:

```cpp
case Sfx::Pop: {
    Buf b(0.18f);
    Voice(b, 0, 0.10f, Osc::Noise, 0,   0,  0.5f, 0.002f, 0.06f);
    Voice(b, 0, 0.15f, Osc::Sine,  420, 70, 0.9f, 0.002f, 0.05f);
    return Bake(b, 0.9f);
}
```

And the "sad descending" player death — the one the design doc calls out by feel — is a
sawtooth falling 320→50 Hz with heavy vibrato and a slow release, plus a noise layer for
grit. The vibrato (`9 Hz`, `±20 Hz`) is what gives it that queasy, deflating wobble:

```cpp
case Sfx::PlayerDie: {
    Buf b(0.9f);
    Voice(b, 0, 0.85f, Osc::Saw,   320, 50, 0.8f, 0.004f, 0.2f, 9.0f, 20.0f);
    Voice(b, 0, 0.40f, Osc::Noise, 0,   0,  0.25f, 0.002f, 0.3f);
    return Bake(b, 0.95f);
}
```

The pickup chime is three ascending square notes (C–E–G) offset in time; the UFO is a
single sine drenched in 11 Hz vibrato. You can read the sound design straight out of the
frequencies. It's the same philosophy as the graphics in post 2 — the asset *is* the code
that generates it, small enough to read and tweak in place.

## The bassline that keeps time with the invasion

The iconic four descending notes are just four one-shot buffers, generated from a small
table:

```cpp
case Sfx::March0: case Sfx::March1:
case Sfx::March2: case Sfx::March3: {
    const float notes[4] = {110.0f, 98.0f, 87.3f, 82.4f};   // the four descending notes
    float f = notes[(int)id - (int)Sfx::March0];
    Buf b(0.11f);
    Voice(b, 0, 0.09f, Osc::Square, f, f * 0.96f, 0.9f, 0.003f, 0.04f);
    return Bake(b, 0.85f);
}
```

The *tempo* is the trick, and it isn't in `audio.cpp` at all — it's an emergent property
of how the march works. In `invaders.cpp`, each march step advances the grid and plays
the next note in the cycle:

```cpp
PlaySfx(*g.audio, (Sfx)((int)Sfx::March0 + g.marchNoteIdx), pitch);
g.marchNoteIdx = (g.marchNoteIdx + 1) % 4;
```

The march step fires on a timer whose interval interpolates from slow (full grid) to fast
(last invader alive) — the classic 1978 acceleration curve from post 1. Because the note
is played *by* the march step, the bassline's tempo is welded to the march tempo for free:
kill invaders, the grid speeds up, and the four notes speed up with it, exactly in lock.
There is no music-sync code, no beat clock, no separate audio scheduler. The panic you
hear is the same timer as the panic you see.

## The 16-second loop is a step sequencer

The background music is one more baked buffer, built by a little sequencer — 120 BPM, 64
eighth-note steps, a 16-second loop. Each step optionally lays a bass note, an off-beat
noise hat, and a sparse sine arpeggio, all with the same `Voice` calls:

```cpp
const float bars[8] = {110, 110, 87.3f, 87.3f, 98, 98, 82.4f, 110};  // bass root per bar
for (int i = 0; i < steps; i++) {
    float t = i * step;
    float root = bars[(i / 8) % 8];
    if (i % 2 == 0) Voice(b, t, 0.20f, Osc::Square, root, root, 0.30f);   // bass, on-beat
    if (i % 2 == 1) Voice(b, t, 0.04f, Osc::Noise, 0, 0, 0.10f);          // hat, off-beat
    // ... sparse arpeggio on some steps ...
}
```

It reuses the same bass roots as the march notes, so the loop and the invaders are in the
same key. The whole soundtrack is one `Bake` call over a 16-second float buffer, computed
before the game starts.

## The rule: never touch the audio thread

Here is the constraint the entire file is built to honor, stated at the top of both
`audio.h` and `audio.cpp`:

> The game stays single-threaded — never use `SetAudioStreamCallback`.

raylib can hand you an audio stream and call *you* on the miniaudio mixer thread to fill
it. That's the normal way to do live synthesis. It is also exactly what this project
refuses to do, because that callback runs on a **different thread**, and the game's core
invariant — one `Game` struct, free functions, no function statics, no shared mutable
state — assumes a single thread of control. Feeding audio from the mixer thread would
mean either duplicating state or reaching across threads into game state, and both break
the model that keeps the rest of the code simple.

So instead of streaming, everything is pre-baked into finite `Sound` buffers, and even
the *looping* is done from the main thread. `TickMusic` is called once per frame from the
main loop and restarts the loop when it's about to run out:

```cpp
void TickMusic(AudioBank& a) {
    if (!a.ready) return;
    double now = GetTime();
    if (a.musicStartedAt < 0 || now - a.musicStartedAt >= (double)a.musicLen - 0.02) {
        PlaySound(a.music);
        a.musicStartedAt = now;
    }
}
```

No callback, no worker, no lock — a timestamp compared against `GetTime()` on the same
thread as everything else. The audio system's entire concurrency story is: there is no
concurrency. That's not a limitation the project is working around; it's the point. The
same decision that keeps the game legible keeps the audio trivially correct.

## Sound, from scratch, on one thread

Sixteen effects and a soundtrack, all generated by an oscillator, an envelope, and a
`switch`, baked to buffers before the first frame and triggered from the main thread ever
after. No files, no middleware, no second thread. The synthesizer is about a hundred
lines, and you can hear every one of them.

The invaders maintain that the bassline is their intellectual property. Legal has reviewed
the claim and determined that it is a `const float notes[4]`, which is not, under current
statute, eligible for royalties. The matter is considered closed. The march continues.

---

*Next: [One Struct, Many Functions](04-one-struct-many-functions.md) — the data-oriented
architecture that makes all of this hang together: one `Game`, a pile of free functions,
and a `switch` where a lesser codebase would have a class hierarchy.*
