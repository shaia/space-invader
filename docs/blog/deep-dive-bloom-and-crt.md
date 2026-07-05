# Deep Dive: The Bloom + CRT Pipeline, to the Metal

*A technical supplement to [Post 2 — Drawing With No Pictures](02-drawing-with-no-pictures.md).*
*Audience: graphics engineers. This one assumes you know what a fragment shader is and
goes all the way down.*

---

Post 2 gave the tour. This is the teardown. We're going to take the three-shader
post-processing chain in [`postfx.cpp`](../../src/postfx.cpp) — bright-pass, separable
Gaussian, CRT composite — and analyze it the way you'd analyze it in a rendering code
review: what it computes, where it's mathematically cutting corners, what those corners
cost, and exactly how you'd fix each one if this were a AAA HDR pipeline instead of a
neon arcade toy. Some of the corners are *correct* to cut for this game. Some are free
wins left on the table. I'll be specific about which is which.

## The data flow, with formats and resolutions

First, get the pixel bookkeeping exact, because half the analysis is about resolution and
format.

- The scene is drawn into a **supersampled canvas**: logical 800×950, rasterized at
  `kSupersample = 2`, so the actual render target is **1600×1900**, format
  `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` — a raylib `RenderTexture2D` is **8-bit UNORM,
  gamma-encoded, LDR**. Hold onto that fact; it drives two of the biggest findings below.
- Bloom works at `canvas / kBloomDownscale`, `kBloomDownscale = 2`, i.e. **800×950**. Three
  render targets live here: `bright` and a two-entry `pingpong[2]`, all RGBA8.
- The composite draws to the **default framebuffer** at whatever the window's letterboxed
  `dst` rectangle is — call it output res, typically ~1700 px tall.

So the pipeline is: `scene(1600×1900) → [bright-pass ↓2] → bright(800×950) → [blur ×8] →
bloom(800×950) → [composite ↑] → window`. Everything is LDR RGBA8 end to end. There is no
float target anywhere.

## Pass 1: the bright-pass, and two things it can't do

```glsl
vec3 c = texture(texture0, fragTexCoord).rgb;
float l = dot(c, vec3(0.2126, 0.7152, 0.0722));       // Rec. 709 relative luminance
float w = smoothstep(threshold - knee, threshold + knee, l);
finalColor = vec4(c * w, 1.0);
```

The luma coefficients are Rec. 709, which is correct *for linear-light RGB*. The soft knee
is a `smoothstep` window of half-width `knee` (0.28) centered on `threshold` (0.66), so
pixels ramp in over luma ∈ [0.38, 0.94] instead of a hard cutoff at 0.66. `smoothstep` is
the cubic `3t² − 2t³`, giving C¹ continuity at both ends — no derivative discontinuity to
show up as a visible ring in the bloom. Good, deliberate choice.

Now the two things this pass structurally cannot do, both traceable to the RGBA8 target:

**1. It's thresholding in gamma space.** The canvas stores sRGB-encoded values, and
`texture()` returns them verbatim (no sRGB texture view, no `GL_FRAMEBUFFER_SRGB`). So `l`
is the luminance of *gamma-encoded* values, not linear light. `dot(709, c_srgb)` is not the
luminance of the pixel; it's the luminance of the pixel's encoding. The threshold still
*works* — brighter things still cross it — but the knee is perceptually warped: because the
sRGB curve is steep near black and flat near white, a fixed 0.28 knee in encoded space
covers a very different range of actual light at the bottom of the range than at the top.
For a game whose bright objects are near-saturated neon this is barely visible, which is
why it's fine here. In a PBR pipeline it would be a bug.

**2. There is no headroom.** Bloom is supposed to be the visible spillover of light too
bright for the display — energy *above* 1.0. But an RGBA8 canvas clamps every pixel to 1.0
before this shader ever runs. So `c * w` can never exceed the source, and the source can
never exceed white. The bloom here isn't "extract the over-bright energy," it's "re-blur
the bright pixels you can already see." That's a legitimate *look* — it's the classic LDR
arcade glow — but it's categorically not HDR bloom, and no amount of tuning
`bloomIntensity` recovers the missing dynamic range, because the range was destroyed at
rasterization time.

Both are the *right call for this game* and *the first two things you'd change* if you ever
wanted physically-plausible bloom. The fix for both is the same one change: make the canvas
and bloom targets `R11G11B10F` (or `RGBA16F`), render the scene with values allowed to
exceed 1.0 on the neon cores, threshold and blur in linear light, and tonemap in the
composite. That's a real project — raylib's `LoadRenderTexture` gives you RGBA8, so you'd
be dropping to `rlgl`/GL to allocate float targets — but it's the canonical upgrade path.

## Pass 2: the separable Gaussian, weight by weight

```glsl
vec2 off = direction / resolution;                    // one texel along the blur axis
vec3 sum  = texture(tex, uv          ).rgb * 0.227027;
sum += texture(tex, uv + off*1.0).rgb * 0.1945946;
sum += texture(tex, uv - off*1.0).rgb * 0.1945946;
sum += texture(tex, uv + off*2.0).rgb * 0.1216216;
// ... ±3 @ 0.0540540, ±4 @ 0.0162162
```

**The weights.** This is the well-known 9-tap discrete Gaussian (σ ≈ 2 texels). Sum them:

```text
0.227027 + 2(0.1945946 + 0.1216216 + 0.0540540 + 0.0162162)
= 0.227027 + 2(0.3864864) = 0.9999998 ≈ 1
```

Normalized to within rounding, so the blur is energy-preserving — a flat white region
stays white, no brightening or darkening. Symmetric, so no directional bias. Textbook.

**Separability.** A 2D Gaussian G(x,y) = G(x)·G(y) factors into two 1D passes, turning an
N×N convolution into 2N taps instead of N². The code runs it as a ping-pong: horizontal
into `pingpong[0]`, vertical into `pingpong[1]`, repeat. Correct and standard.

**Iteration widens σ.** The loop runs `kBloomPasses * 2 = 8` blits — four full H+V
iterations. Convolving a Gaussian with itself yields another Gaussian with
σ_total = √(σ₁² + σ₂² + …). Four iterations of σ≈2 gives σ_eff ≈ √(4·4) = 4 texels at bloom
res, or ~8 texels at canvas res. That's a modest, tight glow — which matches the game's
crisp-neon look. If you wanted the soft, wide, cinematic bloom, iterating a fixed small
kernel is the *expensive* way to get there (see the pyramid note below).

**The free win it skips: bilinear tap folding.** The shader samples at integer offsets
1,2,3,4 — nine discrete `texture()` calls. But GPU bilinear filtering lets you sample
*between* two texels and get a hardware-weighted blend for the same cost as one tap. Pick
the sample position so the blend reproduces the sum of two adjacent Gaussian weights:

For the pair at ±1 and ±2:

```text
W₁₂ = w₁ + w₂ = 0.1945946 + 0.1216216 = 0.3162162
O₁₂ = (1·w₁ + 2·w₂) / W₁₂ = 0.4378378 / 0.3162162 = 1.3846154   texels
```

For the pair at ±3 and ±4:

```text
W₃₄ = w₃ + w₄ = 0.0540540 + 0.0162162 = 0.0702702
O₃₄ = (3·w₃ + 4·w₄) / W₃₄ = 0.2270268 / 0.0702702 = 3.2307692   texels
```

So the *same* Gaussian collapses to **5 taps**: center at weight 0.227027, ±1.3846154 at
0.3162162, ±3.2307692 at 0.0702702. (Those two magic offsets are the ones you've seen in
every "efficient Gaussian blur" article — now you know they're just `Σ(i·wᵢ)/Σwᵢ` for each
folded pair.) Five samples instead of nine is a **44% cut in the hottest pass** — the blur
is 8 full-screen bloom-res passes, the most sample-bound stage in the chain — for a
mathematically *identical* result, assuming a `GL_LINEAR` sampler (which these targets
already set via `SetTextureFilter(..., TEXTURE_FILTER_BILINEAR)`). This is the single
highest-ROI change in the whole file and it's pure shader edit, no format change, no risk.

**Downsample quality.** The bright-pass does the 2× downsample implicitly by rendering a
1600×1900 source into an 800×950 target with a bilinear sampler — each destination texel
lands exactly between four source texels, so it's a 2×2 box average. Fine for one level.
The shimmer risk (bright thin features aliasing as they move) is low here because the
supersampled source is already smooth and the content is chunky vector shapes, not
high-frequency detail.

## Pass 3: the CRT composite, term by term

This is the shader doing the most per fragment, and it runs at full output resolution, so
it's the bandwidth-dominant pass. Walk it top to bottom.

```glsl
vec2 uv = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y);   // (1) flip Y
uv = barrelDistort(uv, barrel);                          // (2) lens bulge
if (uv.x<0.0||uv.x>1.0||uv.y<0.0||uv.y>1.0) { finalColor = vec4(0,0,0,1); return; }
vec2 dir = uv - 0.5;
scene.r = texture(texture0, uv + dir*aberration).r;      // (3) chromatic aberration
scene.g = texture(texture0, uv).g;
scene.b = texture(texture0, uv - dir*aberration).b;
vec3 col = scene + texture(bloomTex, uv).rgb * bloomIntensity;   // (4) additive bloom
if (scanOn > 0.5) { float sl = 0.5 + 0.5*sin(uv.y*scanCount*3.14159); col *= 1.0 - scanDepth*sl; }  // (5)
float vig = smoothstep(0.9, 0.15, dot(dir,dir)*2.0);     // (6) vignette
col *= mix(1.0, vig, vignette);
finalColor = vec4(col, 1.0) * colDiffuse;
```

**(1) Y-flip.** GL render textures are bottom-up; sampling `1 - y` presents them upright.
The LDR fallback path can't do this in-shader, so it flips via a negative source height in
`DrawTexturePro` — same result, different layer. Worth noting the flip happens *before*
distortion so all subsequent math is in upright screen space.

**(2) Barrel distortion.**

```glsl
vec2 cc = uv - 0.5;  float d = dot(cc, cc);  return uv + cc * d * amt;
```

Displacement is `cc · |cc|² · amt` — radially outward, growing with the *cube* of radius
(one factor of `cc`, two from `d = r²`). So it's a pure third-order radial distortion,
`r' = r + amt·r³`, the cheapest convincing "convex tube" model. `amt = 0.020` is subtle.
Corners get pushed past the [0,1] uv box and are explicitly painted black rather than
smeared by clamp — the right choice, since sampling a clamped edge texel would streak the
border color outward. One subtlety: this warp is applied to the *sampling* uv, so it
distorts both scene and bloom consistently, which is what you want (the glow bends with the
glass).

**(3) Chromatic aberration.** R samples at `uv + dir·aberration`, B at `uv − dir·aberration`,
G unshifted, with `dir = uv − 0.5`. So the channel split is **radial and proportional to
field height** — zero at the optical center, maximal at the edges — which is exactly how
real lateral chromatic aberration behaves. `aberration = 0.0016` uv units ≈ 1.3 canvas px
at the extreme corner. Cheap and physically motivated. The only nit: three separate
`texture()` calls where two channels share the same tap could in principle be reduced, but
they sample different positions so there's no real fold here — it's genuinely 3 taps on the
scene + 1 on bloom = 4 samples/fragment.

**(5) Scanlines — the one with a real artifact.**

```glsl
float sl = 0.5 + 0.5*sin(uv.y * scanCount * PI);   // scanCount = 340
col *= 1.0 - scanDepth * sl;                         // scanDepth = 0.10
```

The sine period in uv.y is `2/scanCount = 2/340 ≈ 0.00588`, so there are `scanCount/2 = 170`
dark bands down the screen, darkening by up to 10%. Because the frequency is fixed in
**uv** but sampled at **output-pixel** density, the band count is resolution-independent
(good for consistency) but *not aligned to the output grid* (bad for aliasing). When 170
sinusoidal bands are rasterized onto, say, a 900-px-tall window, you get ~5.3 output pixels
per band and the beat between the sine and the pixel rows produces **moiré** — low-frequency
banding that swims as the window resizes. Two standard mitigations: (a) drive `scanCount`
from actual output height so there's an integer number of pixels per band, and/or (b)
replace the raw `sin` with a band profile that's explicitly filtered against the output
resolution (e.g. attenuate scanline depth as pixels-per-band drops below ~2, the Nyquist
limit, so the effect fades out gracefully instead of aliasing). This is the one place the
composite has a genuine correctness wrinkle rather than a deliberate stylistic LDR
shortcut.

**(6) Vignette.** `smoothstep(0.9, 0.15, dot(dir,dir)*2.0)` — note `edge0 > edge1`, so it's
a *descending* smoothstep: returns 1 at the center (where `dot(dir,dir)=0`) and 0 in the
corners (where `dot(dir,dir)=0.5`, ×2 = 1.0, past `edge0`). Then `mix(1.0, vig, vignette)`
with `vignette = 0.32` lerps corner brightness down to `mix(1,0,0.32) = 0.68`. So corners
sit at 68% brightness with a smooth cubic falloff. Using `dot(dir,dir)` (squared radius)
rather than `length(dir)` saves a `sqrt` and makes the falloff naturally quadratic-ish in
radius, which reads fine.

**(4) The blend.** `col = scene + bloom·intensity` is a straight **additive** composite,
`intensity = 1.15`. Not energy-conserving (an energy-preserving version would
`mix(scene, bloom, k)`), but additive is the correct arcade idiom — bloom is *added light*,
and the slight >1.0 push before the implicit clamp is what makes cores feel like they're
glowing. In LDR this immediately clamps, which is part of why the look is punchy rather than
soft.

## Ordering as a bloom mask (the clever bit)

The sharpest engineering decision isn't in the shaders — it's the draw order in `main.cpp`.
Bloom eats bright pixels, and the two brightest, most-readable things on screen are the HUD
and the white speech-bubble panels. Both are handled by keeping them *out of the bright-pass
input*:

- **HUD** uses `kColHud = {170,185,215}`, a deliberately dim, desaturated blue-grey. The
  config comment states the intent outright — "kept below bloom threshold" — the point
  being to place the UI color low enough on the luma curve that the bright-pass mostly
  ignores it and the score text doesn't blow out into an unreadable halo. (Run the
  encoded-space Rec.709 luma and it actually lands right around the knee rather than
  cleanly under it — a case where nudging the color a shade darker would fully exempt it —
  but the technique is the takeaway: pick UI luminance against the threshold on purpose.)
- **Speech bubbles** are drawn into the canvas *after* `RenderBloom` has already sampled it,
  by re-entering `BeginTextureMode(canvas)` between the bloom render and the composite. The
  bright-pass never sees them, so their white panels can't bloom — but they're still on the
  scene texture the composite samples, so they *do* receive barrel warp, aberration, and
  scanlines. Draw order is being used as a free, per-object bloom stencil, with zero extra
  passes or mask textures.

This is the kind of trick that's obvious in hindsight and saves an entire mask render
target. It's the single best idea in the file.

## A back-of-envelope performance model

Count texture samples per output frame (the currency that matters on a fill-bound 2D
pipeline). Let bloom res B = 800×950 = 760k px; output O ≈ 1700² ish, call it ~2.9M px.

| Stage        | Passes | Res | Samples/px | Total samples |
|--------------|-------:|----:|-----------:|--------------:|
| Bright-pass  | 1      | B   | 1          | 0.76M         |
| Blur (9-tap) | 8      | B   | 9          | 54.7M         |
| Composite    | 1      | O   | 4          | 11.6M         |

The **blur dominates** at ~55M samples — which is precisely why the bilinear-tap fold (9→5)
is the highest-value optimization: it drops that row to ~30M, cutting total pipeline samples
by roughly a third. The composite is second despite being one pass, because it runs at full
output res. The bright-pass is noise. If you were profiling this on an integrated GPU and
needed frames, you'd fold the blur taps first and consider a dual-filter pyramid second;
you would *not* touch the bright-pass.

## The fallback is a feature

Everything above assumes the shaders compiled. `InitPostFx` validates each with
`ShaderOk(s) = s.id != 0 && s.id != rlGetShaderIdDefault()` — because raylib returns its
*default* shader on a failed compile rather than a zero id, so "is this the default shader"
is the real compile check. Any of the three failing flips `enabled = false`, and the main
loop's composite call degrades to a plain `DrawTexturePro` with a negative source height
(the manual Y-flip). No bloom, no CRT, but a running game on a driver that can't do
`#version 330`. For a zero-config single-binary game shipped to unknown hardware, treating
the entire post-FX as optional is exactly right — the effect is garnish, and garnish must
never be load-bearing.

## Scorecard

What's a correct stylistic shortcut, what's a free win, what's an actual wrinkle:

- **Deliberate & correct for the game:** LDR RGBA8 throughout, additive blend, tight
  iterated-Gaussian σ, cheap cubic barrel, squared-radius vignette, ordering-as-mask,
  optional-effect fallback. These *are* the arcade look; don't "fix" them.
- **Free wins, no downside:** fold the 9-tap blur to 5 bilinear taps (identical output,
  ~⅓ fewer total samples). Do this one regardless.
- **Genuine wrinkles worth addressing:** scanline moiré (tie frequency to output res and/or
  Nyquist-fade it); gamma-space threshold/blur (only matters if you ever go HDR).
- **The big project, if you want physically-based glow:** float render targets end to end,
  threshold and blur in linear light, a downsample/upsample pyramid (dual-Kawase or
  Jimenez) instead of a fixed iterated kernel, and a tonemap in the composite. That's a
  different pipeline — and a different game's art direction.

The neon look this ships is a well-chosen point on the cost/quality curve: it commits to
LDR, spends its budget on a wide-enough iterated blur and a busy CRT composite, and hides
the seams with draw order. The one thing I'd change before shipping is the blur tap fold —
it's free — and the one thing I'd never change is that the whole chain is allowed to
politely not exist.

---

*Back to the [series index](README.md).*
