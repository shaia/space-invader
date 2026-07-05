// GPU post-processing: threshold bloom + CRT composite.
// Shaders are compiled from inline GLSL strings (no external assets).
#pragma once
#include "raylib.h"

struct PostFx {
    bool enabled = false;               // false -> caller uses plain blit fallback
    int cw = 0, ch = 0;                 // scene (supersampled canvas) size
    int bw = 0, bh = 0;                 // bloom working resolution

    RenderTexture2D bright{};           // bright-pass extract (bloom res)
    RenderTexture2D pingpong[2]{};      // separable blur ping-pong (bloom res)
    Texture2D finalBloom{};             // result of RenderBloom(), fed to composite

    Shader brightShader{};
    Shader blurShader{};
    Shader compositeShader{};

    // cached uniform locations
    int locBrightThreshold = -1, locBrightKnee = -1;
    int locBlurDir = -1, locBlurRes = -1;
    int locBloomTex = -1, locBloomIntensity = -1;
    int locAberration = -1, locBarrel = -1, locVignette = -1;
    int locScanDepth = -1, locScanCount = -1, locScanOn = -1;
};

// Compiles shaders and allocates bloom render textures. On any shader compile
// failure, leaves fx.enabled = false so the caller can fall back to a plain blit.
void InitPostFx(PostFx& fx, int canvasW, int canvasH);
void UnloadPostFx(PostFx& fx);

// Runs bright-pass + blur passes over `scene`; stores the result in fx.finalBloom.
void RenderBloom(PostFx& fx, Texture2D scene);

// Presents `scene` + bloom to the current framebuffer inside `dst`, applying the
// CRT composite shader. Must be called between BeginDrawing()/EndDrawing().
void DrawComposite(PostFx& fx, Texture2D scene, Rectangle dst, bool scanlinesOn);
