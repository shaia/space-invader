# Drawing With No Pictures

*Post 2 of a series on the engineering behind* **SPACE INVADERS: WE HAVE DEMANDS**.
*Previously: [We Have Demands, and So Does the Compiler](01-we-have-demands-101.md).*

---

There is no artist on this project. There is also no `assets/` folder, no sprite sheet,
no `.png` anywhere the game will ever load. And yet the thing has a look — neon shapes
smeared with bloom, a curved CRT with scanlines and a slight color fringe at the edges,
invaders that squash when they die. The invaders find this deeply unfair. They were
promised a rendering department. The rendering department was, and I quote the game's
own modifier text, "laid off."

This post is about how you build an arcade with no images. Two layers do all the work:
a **software glow** pass drawn from raylib primitives, and a **GPU post-processing**
chain — bloom plus a CRT composite — compiled from GLSL strings baked into the binary.
No file the game reads is larger than the source that draws it.

## The look is a lie you draw three times

Start with the cheapest trick in the file. There are no glowing shapes in raylib. So we
fake a glow the way a stage lighting rig does — draw the same shape more than once, big
and faint underneath, small and solid on top:

```cpp
void GlowRect(Rectangle r, Color c) {
    float g = cfg::kGlowHalo;                                   // 6 px
    DrawRectangleRec({r.x - g, r.y - g, r.width + 2*g, r.height + 2*g},
                     WithAlpha(c, cfg::kGlowHaloA));            // 0.12 — big, faint
    DrawRectangleRec({r.x - 2, r.y - 2, r.width + 4, r.height + 4},
                     WithAlpha(c, cfg::kGlowMidA));             // 0.28 — mid
    DrawRectangleRec(r, c);                                     // solid core
}
```

That's the whole idea: an outer halo at 12% alpha, a mid ring at 28%, then the crisp
core. `GlowCircle`, `GlowLine`, and `GlowText` are the same pattern in different
primitives — `GlowText` is literally the string drawn twice, once offset and dimmed as a
drop-shadow, once sharp on top. Every neon edge in the game is two or three overlapping
translucent draws, and the numbers all live in `config.h` (`kGlowHalo`, `kGlowHaloA`,
`kGlowMidA`, …) because typing a literal into a draw call is, per house rule, grounds
for a staff meeting.

This is doing a real job. The glow pass is what *feeds* the bloom shader later — it lays
down soft, bright margins around every shape so the GPU pass has something to bleed. The
two layers are designed together: cheap CPU halos to shape the light, then a GPU blur to
make it convincing.

## Invaders are 48 bits and a mood

There are no invader sprites either. Each archetype is a pair of 8×6 bitmasks — one per
march frame — and the silhouette is drawn one lit cell at a time:

```cpp
// Row archetypes: 0 = squid (executive), 1-2 = crab (manager), 3-4 = octopus (intern).
constexpr uint8_t kSquid[2][6] = {
    {0b00011000, 0b00111100, 0b01111110, 0b11011011, 0b00100100, 0b01011010},
    {0b00011000, 0b00111100, 0b01111110, 0b11011011, 0b01011010, 0b10100101},
};
```

Six bytes, two frames, three archetypes — that's the entire cast's art, and you can *read*
the aliens right there in the binary literals. The draw loop walks the bits and does a
little extra work per cell to sell depth on a flat shape:

```cpp
for (int r = 0; r < 6; r++) {
    // vertical shading: bright top, deeper bottom (reads as a lit dome)
    Color rc = ShadeColor(tint, 1.20f - 0.45f * (r / 5.0f));
    for (int b = 0; b < 8; b++) {
        if (!(mask[frame][r] & (1 << (7 - b)))) continue;
        float jx = 0, jy = 0;
        if (wobbly) {                                  // "The Understudies" modifier
            jx = WobbleOff(seed + r*8 + b, time, 6.0f, 1.6f);
            jy = WobbleOff(seed + r*8 + b + 999, time, 5.0f, 1.6f);
        }
        DrawRectangleRec({x0 + b*cw + jx, y0 + r*ch + jy, cw + 0.5f, ch + 0.5f}, rc);
    }
}
```

Three details worth calling out, because each is a design rule from post 1 showing up in
pixels:

- **Per-row shading.** `ShadeColor(tint, 1.20 → 0.75)` brightens the top rows and darkens
  the bottom, so a flat cell grid reads as a lit dome. No normals, no lighting — just a
  scalar ramped down the mask.
- **The wobble is a modifier, not a decoration.** When "The Understudies" is active,
  `wobbly` is true and every cell gets a per-cell hash-driven jitter (`WobbleOff` is a
  deterministic `sin` of a hashed seed — no RNG state, consistent with the no-statics
  rule). The joke is that the understudies can't hold still; the *mechanical* consequence
  is that the drawn silhouette drifts, so the hitbox you're aiming at isn't quite where
  the body is. Every gag changes play.
- **Squash and stretch** come in as `sx`/`sy` scale factors, so a dying invader can
  flatten without a second art asset.

The player ship, the UFO, and every projectile are the same story — procedural vector
shapes assembled from triangles, ellipses, and glowing rects (`DrawPlayerArt`,
`DrawUfoArt`, `DrawShotArt` in `render.cpp`). The player even has a flickering thruster
flame computed from `sin(time * 40)`. It's all math. None of it is loaded.

## The bloom is real, and it runs on the GPU

The software glow gives us bright margins. The convincing part — the light that bleeds
past its edges — is a proper multi-pass bloom on the GPU, and it lives entirely in
`postfx.cpp`. There are no `.fs`/`.vs` files; the three shaders are `constexpr const
char*` GLSL 3.3 strings compiled with `LoadShaderFromMemory`, so the whole effect ships
inside the executable.

**Pass 1 — bright-pass extract.** Sample the scene, compute luminance, and keep only the
pixels above a threshold, with a soft knee so the cutoff isn't a hard edge:

```glsl
vec3 c = texture(texture0, fragTexCoord).rgb;
float l = dot(c, vec3(0.2126, 0.7152, 0.0722));       // Rec. 709 luma
float w = smoothstep(threshold - knee, threshold + knee, l);
finalColor = vec4(c * w, 1.0);
```

This renders into a **downscaled** target (`canvas / kBloomDownscale`, i.e. half res) —
cheaper to blur, and the blur hides the downsampling. Threshold `0.66` and knee `0.28`
are tuned so the HUD *doesn't* bloom; more on that in a second.

**Pass 2 — separable Gaussian blur.** A 9-tap Gaussian, run as a horizontal pass then a
vertical pass, ping-ponged between two textures for several iterations:

```cpp
Texture2D src = fx.bright.texture;
int dstIdx = 0;
for (int i = 0; i < cfg::kBloomPasses * 2; i++) {         // 4 passes = 8 blits (H+V each)
    Vector2 dir = (i & 1) ? Vector2{0,1} : Vector2{1,0};
    BeginTextureMode(fx.pingpong[dstIdx]);
    BeginShaderMode(fx.blurShader);
    SetShaderValue(fx.blurShader, fx.locBlurDir, &dir, SHADER_UNIFORM_VEC2);
    SetShaderValue(fx.blurShader, fx.locBlurRes, &res, SHADER_UNIFORM_VEC2);
    BlitFill(src, fx.bw, fx.bh);
    EndShaderMode();
    EndTextureMode();
    src = fx.pingpong[dstIdx].texture;
    dstIdx ^= 1;
}
fx.finalBloom = src;
```

Separable is the classic win: a 2D Gaussian factored into two 1D passes is O(n) taps
instead of O(n²). Four H+V iterations over a half-res buffer widen the glow a lot for
very little bandwidth. `finalBloom` is just whichever ping-pong texture we landed on.

**Pass 3 — the CRT composite.** One fragment shader combines the scene with the blurred
bloom and does everything that makes it feel like a tube:

```glsl
vec2 uv = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y);  // render targets are bottom-up
uv = barrelDistort(uv, barrel);                        // bulge the glass
if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
    finalColor = vec4(0,0,0,1); return;                // black the warped-off corners
}
vec2 dir = uv - 0.5;
vec3 scene;
scene.r = texture(texture0, uv + dir * aberration).r;  // chromatic aberration:
scene.g = texture(texture0, uv).g;                     //   split R/B radially
scene.b = texture(texture0, uv - dir * aberration).b;
vec3 col = scene + texture(bloomTex, uv).rgb * bloomIntensity;   // additive bloom
if (scanOn > 0.5) {
    float sl = 0.5 + 0.5 * sin(uv.y * scanCount * 3.14159265);
    col *= 1.0 - scanDepth * sl;                        // scanline banding
}
float vig = smoothstep(0.9, 0.15, dot(dir, dir) * 2.0);
col *= mix(1.0, vig, vignette);                         // darken the edges
finalColor = vec4(col, 1.0) * colDiffuse;
```

Barrel distortion bulges the image so the "glass" is convex (and the corners that get
pushed off-screen are painted black, not smeared). Chromatic aberration offsets the red
and blue channels radially — zero at the center, worst at the edges, exactly like a cheap
lens. Scanlines are a `sin` band whose count (`340`) and depth (`0.10`) are, of course,
config constants. A vignette drops the corners. Every parameter is uniform-driven from
`config.h`, so the entire CRT personality is tunable without recompiling a shader.

## The bloom is tuned around the *content*, not the other way around

Here's the detail I like most, because it's where art direction becomes an engineering
constraint. Bloom eats bright pixels. That is great for lasers and terrible for
**anything you need to read** — a HUD, a score, a speech bubble. So the game arranges for
readable things to sit *below* the bloom threshold.

The HUD color is chosen deliberately dim, with the reason written into `config.h`:

```cpp
inline constexpr Color kColHud = {170, 185, 215, 255};  // kept below bloom threshold
```

And speech bubbles — white panels, the single most bloom-prone thing on screen — are
drawn **after** the bright-pass has already sampled the scene, by re-entering the canvas
between the bloom render and the final composite:

```cpp
if (postfx.enabled) RenderBloom(postfx, canvas.texture);

// speech bubbles draw AFTER the bright-pass: their white panels must not bloom
if (screen == Screen::Playing || screen == Screen::Paused || screen == Screen::GameOver) {
    BeginTextureMode(canvas);
    BeginMode2D(ssCam);
    DrawSpeechBubbles(g);
    EndMode2D();
    EndTextureMode();
}
```

The bright-pass never sees the bubbles, so they can't glow into an unreadable smear — but
they still land on the scene texture the composite samples, so they *do* get the barrel
warp and scanlines like everything else. Draw order is being used as a cheap per-object
bloom mask. This is the post-1 rule ("jokes never hide mechanics") enforced at the
framebuffer level: the invaders' complaints are legible precisely because we refused to
let them bloom.

## The whole thing is allowed to fail

None of this is load-bearing for the game running. Shaders are the one place where a
target machine can genuinely say no — an old driver, a headless context, a GL profile
that won't compile `#version 330`. So the post-FX is written to fail soft. `InitPostFx`
validates every compile and, on any failure, flips a single flag:

```cpp
bool ShaderOk(Shader s) { return s.id != 0 && s.id != rlGetShaderIdDefault(); }
// ...
if (!ShaderOk(fx.brightShader) || !ShaderOk(fx.blurShader) ||
    !ShaderOk(fx.compositeShader)) {
    fx.enabled = false;
    return;
}
```

`ShaderOk` is the important bit: raylib doesn't throw on a failed compile, it hands back
its *default* shader, so "did this compile" means "is the id neither zero nor the default
id." If any of the three fails, `enabled` stays false and the main loop takes the other
branch — a plain textured blit, no bloom, no CRT:

```cpp
if (postfx.enabled) {
    DrawComposite(postfx, canvas.texture, dst, cfg::kScanlines);
} else {
    DrawTexturePro(canvas.texture,
                   {0, 0, (float)canvas.texture.width, -(float)canvas.texture.height},
                   dst, {0, 0}, 0.0f, WHITE);   // negative height flips Y in the blit
}
```

The game looks flatter, but it *runs*, everywhere, with zero configuration. (Note the
negative source height — the fallback has to flip the bottom-up render texture itself,
the job the composite shader's `1.0 - uv.y` does on the fast path.) One flag, two draw
paths, no crash surface. That is the whole graphics-driver support matrix.

## What we drew, and what we didn't

Everything on screen is one of two things: a few translucent primitives stacked into a
glow, or a bitmask walked cell by cell — then the lot is pushed through a bloom-and-CRT
shader chain that lives in the `.text` section of the executable. No textures, no
materials, no atlas, no asset pipeline, and a hard fallback for the machines that can't
run the fancy part.

The rendering department remains laid off. Management notes that output has not
measurably changed.

---

*Next: [Every Sound Is a Function](03-every-sound-is-a-function.md) — building an entire
arcade's audio at startup with an oscillator, an envelope, and a strict prohibition on
touching the audio thread.*
