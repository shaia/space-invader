# Deep Dive: The Synthesizer, to the Sample

*A technical supplement to [Post 3 — Every Sound Is a Function](03-every-sound-is-a-function.md),
and a parallel to the [bloom + CRT teardown](deep-dive-bloom-and-crt.md).*
*Audience: anyone comfortable with sample rates, the Nyquist limit, and why a square wave
is secretly a lot of sine waves. This goes down to the sample.*

---

Post 3 gave the tour of [`audio.cpp`](../../src/audio.cpp): an oscillator, an envelope, a
bake step, everything synthesized once at startup. This is the DSP teardown of that same
hundred lines — where it's mathematically clean, where it's cutting corners, what those
corners sound like, and exactly how a "correct" synth would do it differently. As with the
bloom dive, some corners are the right call for a lo-fi arcade game and some are free wins.
I'll separate them at the end.

## The signal chain, with units

- **Sample rate** `kSampleRate = 44100` Hz. Nyquist is **22050 Hz** — the hard ceiling
  above which any generated frequency folds back as alias. This single number governs the
  whole aliasing discussion below.
- **Working buffer** is `std::vector<float>`, sized `seconds · 44100`, mixed in **float**
  (so intermediate sums can and do exceed ±1.0 — the bake step handles it).
- **Output** is a raylib `Sound`: 16-bit signed PCM, mono, 44100 Hz, produced by baking the
  float buffer through `LoadSoundFromWave`.
- **When:** all of it at `InitAudioBank`, before the first frame. Runtime cost is zero — you
  only ever `PlaySound` a finished buffer.

The chain per effect is `Voice(...) → [additive mix into Buf] → Bake → Sound`. Let's take
each stage at the sample level.

## The oscillator core: phase accumulation is right

```cpp
float phase = 0.0f;
for (int i = 0; i < n; i++) {
    float t = (float)i / cfg::kSampleRate;
    float f = f0 + (f1 - f0) * (t / dur);          // linear pitch glide
    if (vibHz > 0) f += sinf(t * kTau * vibHz) * vibAmt;
    phase += f / cfg::kSampleRate;                  // accumulate, don't recompute
    switch (osc) {
    case Osc::Sine:   v = sinf(phase * kTau); break;
    case Osc::Square: v = (fmodf(phase, 1.0f) < 0.5f) ? 1.0f : -1.0f; break;
    case Osc::Saw:    v = 2.0f * fmodf(phase, 1.0f) - 1.0f; break;
    case Osc::Noise:  v = NoiseAt(idx); break;
    }
}
```

The important structural correctness first: **phase is integrated incrementally**
(`phase += f/fs`) rather than computed as `sin(2π f t)`. This matters the instant frequency
varies. With a glide or vibrato, `f` changes every sample; `sin(2π f t)` would produce phase
jumps (because the *instantaneous* `f·t` is not the integral of frequency over time), which
click. Accumulating the increment makes phase a true running integral of frequency, so
glides and vibrato stay phase-continuous and click-free. This is the textbook-correct way
and the file gets it right.

**Vibrato** is genuine frequency modulation — `f += sin(2π·vibHz·t)·vibAmt` before the
increment — so the UFO's `vibHz=11, vibAmt=90` is a real ±90 Hz swing at 11 Hz around its
620 Hz carrier. Correct FM, not a fake amplitude wobble.

**The glide is linear in Hz**, `f = f0 + (f1−f0)·u`. Worth flagging because pitch perception
is logarithmic: a linear-Hz sweep from 320→50 (the death sound) covers most of its *musical*
descent in the first fraction of the sweep and crawls through the low end. That's a
sound-design choice, not a bug — but if you wanted a constant-rate *musical* glissando you'd
use the exponential form `f = f0·(f1/f0)^u`. The linear version has a characteristic
"dive-then-settle" shape that happens to suit a dying-cannon effect.

## The big finding: the square and saw are not band-limited

Here's the audio analog of "the bloom is LDR" — the one structural DSP fact that shapes the
whole character of the output.

`Osc::Square` and `Osc::Saw` are generated the naive way: a comparison and a `fmod`. A
mathematically ideal saw contains **every** harmonic, `n·f0`, with amplitude `1/n`; an ideal
square contains the odd harmonics with amplitude `1/n`. Generating them by direct evaluation
means you're sampling a signal with **infinite bandwidth** at 44.1 kHz — and every harmonic
above Nyquist doesn't just vanish, it **folds back** to `|f − k·fs|` as an inharmonic alias.

Quantify it. A saw at fundamental `f0` has `floor(22050 / f0)` harmonics that fit below
Nyquist; the rest alias. So:

```text
March low note  f0 = 82.4 Hz  → ~267 harmonics below Nyquist, remainder aliases (weak, ~1/n)
Shoot (start)   f0 = 900 Hz   → only ~24 harmonics below Nyquist; folding is proportionally
                                 louder relative to the fundamental → more audible buzz
```

Counterintuitively, the **higher-pitched** square/saw alias *worse* in perceptual terms:
fewer legitimate harmonics carry the tone, and the folded partials sit at inharmonic
frequencies that beat against the fundamental. The 900 Hz start of the laser is the worst
offender in the bank; the deep march notes are the least affected. (`Osc::Sine` is exact —
one `sinf`, no harmonics, no aliasing. `Osc::Noise` is white by definition, so "aliasing"
is moot — it's meant to be broadband.)

Does it matter? For this game, mostly no — a bit of grit on a chiptune square reads as
*authentically lo-fi*, and it's swimming under gameplay SFX. But it is unambiguously
aliasing, and if you wanted it clean, the standard fix is **PolyBLEP** (polynomial
band-limited step): correct the waveform in a one-sample-increment neighborhood of each
discontinuity. For a saw with phase `p` and increment `dt = f/fs`:

```cpp
float polyBlep(float t, float dt) {          // residual to subtract near a discontinuity
    if (t < dt)      { t /= dt;      return t + t - t*t - 1.0f; }   // just after the jump
    if (t > 1 - dt)  { t = (t-1)/dt; return t*t + t + t + 1.0f; }   // just before the jump
    return 0.0f;
}
// saw:    v = (2*p - 1) - polyBlep(p, dt);
// square: v = ±1 with polyBlep applied at BOTH edges (p≈0 and p≈0.5)
```

That's ~10 lines and it removes essentially all of the aliasing for a couple of multiplies
per sample — at *bake* time, so it costs nothing at runtime. It's the audio equivalent of
the bilinear-tap fold from the graphics dive: a well-known, cheap, correctness-restoring
change with no downside except that the sound gets *cleaner*, which for this aesthetic you
might not even want. Deliberate lo-fi is a legitimate reason to skip it — but it should be a
choice, not an accident.

## The envelope: linear ramps, and a release that swallows short sounds

```cpp
float env = 1.0f;
if (t < attack)   env = t / attack;            // linear ramp up from 0
float tail = dur - t;
if (tail < release) env *= tail / release;     // linear ramp down to 0
b.s[idx] += v * amp * env;
```

Two ramps, linear, defaulting to `attack = 0.004 s` (~176 samples) and `release = 0.05 s`
(~2205 samples). The onset ramps from exactly 0 and the tail ramps to exactly 0, so there's
no click at either end — good. Two things worth dissecting:

**Linear, not exponential.** Real instruments and analog envelopes decay exponentially;
linear ramps are C⁰-continuous but not C¹ — there's a slope discontinuity at the
attack→sustain and sustain→release corners. A slope discontinuity injects a little
high-frequency spectral splatter. At these ramp lengths it's inaudible, and linear is
cheaper and perfectly idiomatic for arcade blips, so this is fine — just know the corners
are there.

**The release default is longer than several of the sounds.** This is the subtle one. Look
at `Blip`: `Voice(b, 0, 0.05, Square, 660, 660, 0.5)` — duration `0.05 s`, and it takes the
**default `release = 0.05 s`**. Since the release ramp engages whenever `tail < release`, and
here `release == dur`, the condition is true for the *entire* note except `t=0`. So the blip
has no sustain phase at all — it's a 4 ms ramp up immediately into a 50 ms linear ramp down,
i.e. a little triangle that peaks near `t≈attack` at ~0.92 and decays to zero. That's a fine
percussive envelope, but it's worth realizing that for any sound whose `dur ≲ release`, the
"amplitude" argument never fully applies — the note lives entirely inside its own release.
Several short SFX are shaped this way by default rather than by explicit design. If
`dur < attack + release`, attack and release even *overlap* and multiply, so the peak env
drops below 1.0 — again not a click, just a quieter-than-nominal note. Nothing broken; just a
place where the default parameter, not the caller's `amp`, is setting the level.

## The noise source: white, deterministic, and pitch-blind

```cpp
float NoiseAt(int i) {                    // deterministic value noise, no shared state
    unsigned n = (unsigned)i * 2654435761u;
    n ^= n >> 13;  n *= 2246822519u;  n ^= n >> 16;
    return ((float)(n & 0xFFFF) / 32768.0f) - 1.0f;
}
```

Two notes. First, the philosophical one from Post 3: it's a **pure function of the sample
index**, so noise needs no RNG state and honors the codebase's no-function-statics rule even
down in the synth. Second, the DSP one: it's **full-band white noise** and it **ignores the
`f0`/`f1` arguments entirely** — the `Osc::Noise` case doesn't touch `phase`. So every noise
layer (the Pop transient, the Crunch, the explosion grit) is the same broadband hiss shaped
only by its envelope and amplitude. That's totally adequate for short transients, but it's
the one oscillator with no timbral control — you can't make "low rumble noise" vs "bright
hiss" without adding a filter. A one-pole low-pass or a simple state-variable filter over the
noise would unlock colored noise (wind, rumble, surf) for near-zero cost. Not a flaw, just
the ceiling of what this synth can currently say.

One aside on distribution: the hash returns `(n & 0xFFFF)/32768 − 1`, i.e. values in
`[−1, +0.99997]` — very slightly asymmetric and thus a hair of DC, utterly negligible after
the envelope multiply and peak-normalize.

## Bake: peak-normalize-with-ceiling, and 16-bit reality

```cpp
float peak = 0.0001f;
for (float v : b.s) peak = fmaxf(peak, fabsf(v));
float k = gain / peak;
if (k > gain) k = gain;                    // don't amplify silence
// ... per sample:
float v = clamp(b.s[i] * k, -1, 1);
data[i] = (short)(v * 32000.0f);
```

**The normalization is a peak-limiter with a unity-ish ceiling.** Work the two cases:

- If the mixed buffer **peaks above 1.0** (common — voices sum in float), then `peak ≥ 1`, so
  `k = gain/peak ≤ gain`, and the post-scale peak is `peak·k = gain`. The sound is normalized
  so its loudest sample sits at exactly `gain`.
- If the buffer **peaks below 1.0** (a quiet sound), `k = gain/peak > gain` would *boost* it,
  so the clamp `if (k > gain) k = gain` kicks in and it's merely scaled by `gain`, landing
  below full scale.

So: loud sounds are normalized to `gain`, quiet sounds are attenuated by `gain` but never
boosted. It's a sane, click-free gain stage. The one thing it *isn't* is **loudness-matched**:
this is peak normalization, and peak ignores crest factor. A noisy transient and a sustained
sine normalized to the same peak will have very different perceived loudness (the sine is far
louder to the ear). The per-category masters (`kMasterSfx = 0.65`, `kMasterMusic = 0.35`)
trim it globally, but individual SFX loudness is hand-tuned via each `Bake(b, gain)` call
rather than measured. RMS/LUFS normalization would make the bank consistent automatically —
overkill here, but that's the "correct" version.

**The 16-bit conversion has two textbook micro-issues, both inaudible and both worth
knowing:**

1. **Scale is `32000`, not `32767`.** That leaves a hair of headroom — `32000/32767 ≈ −0.2
   dB` — so a sample that rounds up can't wrap around to full-scale negative. Cheap safety
   margin, sensible.
2. **`(short)(v * 32000.0f)` truncates toward zero** rather than rounding to nearest, and
   there's **no dither**. Truncation adds up to a full LSB of signal-correlated quantization
   error (vs half an LSB for rounding), and skipping dither means the quantization noise is
   correlated with the signal rather than spectrally flat. At 16 bits and these levels this is
   inconceivably far below audible — but the "correct" path is round-to-nearest plus TPDF
   dither, and it's one line each.

## Playback realities the synth doesn't see

The bake is only half the story; how these buffers get *triggered* has its own DSP
consequences.

**`SetSoundPitch` resamples.** `PlaySfx` sets a per-play pitch before `PlaySound`
(e.g. Pop is played at `rng.range(0.9, 1.1)` for variation, and shrunk-wave kills at `1.7`).
raylib/miniaudio implements pitch by changing the resample ratio, which shifts pitch **and**
duration together and re-samples the baked buffer. Pitching *up* (1.7×) reads the buffer
faster — it shifts all that baked harmonic content (including the aliasing above) up in
frequency; it won't create *new* aliasing beyond what's in the buffer, but it does move the
existing artifacts around. Fine for variation; just note that pitch is a playback-time
resample, not a re-synthesis.

**Retrigger, not polyphony.** Calling `PlaySound` on a `Sound` that's already playing
restarts that one buffer from the top — it doesn't layer a second voice. So rapid identical
SFX (a flurry of `Pop`s when you clear a row fast, or repeated `Crunch`) cut themselves off
rather than overlapping. The **march sidesteps this on purpose**: the four notes are four
*separate* `Sound` objects (`March0..3`) played cyclically, so consecutive steps never
retrigger the same buffer and never clip each other — a real reason the bassline stays clean
under the accelerating march from Post 3. If you wanted true overlap on the one-shots, raylib
`LoadSoundAlias` gives you cheap extra playback voices over the same PCM.

**The music loop restart is (just barely) click-safe by construction.** `TickMusic`
retriggers the 16 s loop when `now − started ≥ musicLen − 0.02`, i.e. ~20 ms early, from the
main thread. Restarting mid-waveform would click — *unless* the waveform is near silence at
the splice. It is: the sequencer's last bass note lands at step 63 (`t = 15.75`, `dur 0.20`,
done by `15.95`), and the buffer is allocated with `+0.3 s` of slack that never plays, so at
the ~15.98 s restart point the signal has already decayed to near-zero. The loop is designed
so its seam sits in silence — the 20 ms early cut and the retrigger both land in the quiet
tail. Deliberate, and the kind of thing that only works because someone thought about where
the loop point falls.

## Cost model

The whole synth is a startup tax and nothing else. Rough totals:

| Item              | Duration | Samples (×44100) | int16 bytes |
|-------------------|---------:|-----------------:|------------:|
| ~16 SFX           | ~0.05–1.1 s each | a few ×10⁵ total | ~a few hundred KB |
| Music loop        | ~16.3 s  | ~719k            | ~1.4 MB     |

Synthesis is `O(total samples)` of float work — a few million multiply-adds, single-digit
milliseconds at load. After that the runtime audio cost is `PlaySound`: a buffer reference
and a mix, no synthesis, no per-frame DSP, no allocation. That is the entire point of baking
— and, per Post 3, the reason there's no audio-callback thread to violate the game's
single-threaded model. The PolyBLEP and dither improvements above would all fold into this
same one-time startup cost; none of them would touch the frame budget.

## Scorecard

Sorting the findings the way the graphics dive did:

- **Deliberate & correct for the game:** incremental phase accumulation, true FM vibrato,
  linear envelopes, peak-normalize-with-ceiling, `32000` headroom, four-separate-buffers
  march, silence-at-the-seam music loop. These are right — several are subtly *clever*.
- **Free wins, no runtime cost (all at bake time):** PolyBLEP band-limiting on square/saw to
  kill aliasing; round-to-nearest + TPDF dither on the int16 conversion. Do these only if you
  actually want the sounds cleaner — which for this aesthetic is a real question.
- **Ceilings, not bugs:** white-only noise with no filter (blocks colored-noise timbres);
  peak- rather than loudness-normalization (per-sound level is hand-tuned); linear-Hz glide
  (fine, but not a constant-rate musical sweep).
- **The "correct synth" version, if you wanted it:** band-limited oscillators (PolyBLEP or
  additive-below-Nyquist), a one-pole filter on the noise path, exponential envelope options,
  and LUFS-matched bake gains. That's a more capable instrument — and a less charming one.

The synthesizer makes a deliberate trade that mirrors the renderer's LDR-neon trade exactly:
it commits to a lo-fi, chiptune-adjacent sound, spends its complexity budget on the things
that carry game feel (the click-free glides, the tempo-locked march, the seamless loop), and
leaves the textbook-clean DSP — band-limiting, dither, loudness matching — on the table
because the grit is part of the look. The one change I'd genuinely consider is PolyBLEP, and
only after A/B-ing it, because there's a real chance the aliasing is load-bearing character.

Everything here runs once, before the title screen, on one thread, and never allocates
again. The invaders' bassline remains, as established, ineligible for royalties.

---

*Back to the [series index](README.md). See also the [bloom + CRT teardown](deep-dive-bloom-and-crt.md).*
