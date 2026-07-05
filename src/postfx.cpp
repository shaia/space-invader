#include "postfx.h"
#include "config.h"
#include "rlgl.h"

namespace {

// ---- inline GLSL (desktop GL 3.3) ----

constexpr const char* kBrightFrag = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform float threshold;
uniform float knee;
void main() {
    vec3 c = texture(texture0, fragTexCoord).rgb;
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    float w = smoothstep(threshold - knee, threshold + knee, l);
    finalColor = vec4(c * w, 1.0);
}
)";

constexpr const char* kBlurFrag = R"(#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 direction;   // (1,0) horizontal or (0,1) vertical
uniform vec2 resolution;  // working texture size in px
void main() {
    vec2 off = direction / resolution;
    vec3 sum = texture(texture0, fragTexCoord).rgb * 0.227027;
    sum += texture(texture0, fragTexCoord + off * 1.0).rgb * 0.1945946;
    sum += texture(texture0, fragTexCoord - off * 1.0).rgb * 0.1945946;
    sum += texture(texture0, fragTexCoord + off * 2.0).rgb * 0.1216216;
    sum += texture(texture0, fragTexCoord - off * 2.0).rgb * 0.1216216;
    sum += texture(texture0, fragTexCoord + off * 3.0).rgb * 0.0540540;
    sum += texture(texture0, fragTexCoord - off * 3.0).rgb * 0.0540540;
    sum += texture(texture0, fragTexCoord + off * 4.0).rgb * 0.0162162;
    sum += texture(texture0, fragTexCoord - off * 4.0).rgb * 0.0162162;
    finalColor = vec4(sum, 1.0);
}
)";

constexpr const char* kCompositeFrag = R"(#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0;   // scene
uniform sampler2D bloomTex;   // blurred bright-pass
uniform vec4 colDiffuse;
uniform float bloomIntensity;
uniform float aberration;
uniform float barrel;
uniform float vignette;
uniform float scanDepth;
uniform float scanCount;
uniform float scanOn;

vec2 barrelDistort(vec2 uv, float amt) {
    vec2 cc = uv - 0.5;
    float d = dot(cc, cc);
    return uv + cc * d * amt;
}

void main() {
    // render textures are bottom-up; flip Y so both samplers align upright
    vec2 uv = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y);
    uv = barrelDistort(uv, barrel);
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        finalColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    vec2 dir = uv - 0.5;
    // chromatic aberration: split R/B radially
    vec3 scene;
    scene.r = texture(texture0, uv + dir * aberration).r;
    scene.g = texture(texture0, uv).g;
    scene.b = texture(texture0, uv - dir * aberration).b;
    vec3 bloom = texture(bloomTex, uv).rgb;
    vec3 col = scene + bloom * bloomIntensity;
    if (scanOn > 0.5) {
        float sl = 0.5 + 0.5 * sin(uv.y * scanCount * 3.14159265);
        col *= 1.0 - scanDepth * sl;
    }
    float vig = smoothstep(0.9, 0.15, dot(dir, dir) * 2.0);
    col *= mix(1.0, vig, vignette);
    finalColor = vec4(col, 1.0) * colDiffuse;
}
)";

bool ShaderOk(Shader s) { return s.id != 0 && s.id != rlGetShaderIdDefault(); }

// draw a source texture to fill the current render target, no vertical flip
void BlitFill(Texture2D src, int dstW, int dstH) {
    DrawTexturePro(src, {0, 0, (float)src.width, (float)src.height},
                   {0, 0, (float)dstW, (float)dstH}, {0, 0}, 0.0f, WHITE);
}

} // namespace

void InitPostFx(PostFx& fx, int canvasW, int canvasH) {
    fx.cw = canvasW;
    fx.ch = canvasH;
    fx.bw = canvasW / cfg::kBloomDownscale;
    fx.bh = canvasH / cfg::kBloomDownscale;

    fx.brightShader = LoadShaderFromMemory(0, kBrightFrag);
    fx.blurShader = LoadShaderFromMemory(0, kBlurFrag);
    fx.compositeShader = LoadShaderFromMemory(0, kCompositeFrag);

    if (!ShaderOk(fx.brightShader) || !ShaderOk(fx.blurShader) ||
        !ShaderOk(fx.compositeShader)) {
        fx.enabled = false;
        return;
    }

    fx.locBrightThreshold = GetShaderLocation(fx.brightShader, "threshold");
    fx.locBrightKnee = GetShaderLocation(fx.brightShader, "knee");
    fx.locBlurDir = GetShaderLocation(fx.blurShader, "direction");
    fx.locBlurRes = GetShaderLocation(fx.blurShader, "resolution");
    fx.locBloomTex = GetShaderLocation(fx.compositeShader, "bloomTex");
    fx.locBloomIntensity = GetShaderLocation(fx.compositeShader, "bloomIntensity");
    fx.locAberration = GetShaderLocation(fx.compositeShader, "aberration");
    fx.locBarrel = GetShaderLocation(fx.compositeShader, "barrel");
    fx.locVignette = GetShaderLocation(fx.compositeShader, "vignette");
    fx.locScanDepth = GetShaderLocation(fx.compositeShader, "scanDepth");
    fx.locScanCount = GetShaderLocation(fx.compositeShader, "scanCount");
    fx.locScanOn = GetShaderLocation(fx.compositeShader, "scanOn");

    fx.bright = LoadRenderTexture(fx.bw, fx.bh);
    fx.pingpong[0] = LoadRenderTexture(fx.bw, fx.bh);
    fx.pingpong[1] = LoadRenderTexture(fx.bw, fx.bh);
    SetTextureFilter(fx.bright.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(fx.pingpong[0].texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(fx.pingpong[1].texture, TEXTURE_FILTER_BILINEAR);

    fx.finalBloom = fx.pingpong[0].texture;
    fx.enabled = true;
}

void UnloadPostFx(PostFx& fx) {
    if (!fx.enabled) return;
    UnloadRenderTexture(fx.bright);
    UnloadRenderTexture(fx.pingpong[0]);
    UnloadRenderTexture(fx.pingpong[1]);
    UnloadShader(fx.brightShader);
    UnloadShader(fx.blurShader);
    UnloadShader(fx.compositeShader);
    fx.enabled = false;
}

void RenderBloom(PostFx& fx, Texture2D scene) {
    if (!fx.enabled) return;

    // bright-pass + downsample
    float threshold = cfg::kBloomThreshold, knee = cfg::kBloomSoftKnee;
    BeginTextureMode(fx.bright);
    ClearBackground(BLANK);
    BeginShaderMode(fx.brightShader);
    SetShaderValue(fx.brightShader, fx.locBrightThreshold, &threshold, SHADER_UNIFORM_FLOAT);
    SetShaderValue(fx.brightShader, fx.locBrightKnee, &knee, SHADER_UNIFORM_FLOAT);
    BlitFill(scene, fx.bw, fx.bh);
    EndShaderMode();
    EndTextureMode();

    // separable gaussian ping-pong
    Vector2 res = {(float)fx.bw, (float)fx.bh};
    Texture2D src = fx.bright.texture;
    int dstIdx = 0;
    for (int i = 0; i < cfg::kBloomPasses * 2; i++) {
        Vector2 dir = (i & 1) ? Vector2{0.0f, 1.0f} : Vector2{1.0f, 0.0f};
        BeginTextureMode(fx.pingpong[dstIdx]);
        ClearBackground(BLANK);
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
}

void DrawComposite(PostFx& fx, Texture2D scene, Rectangle dst, bool scanlinesOn) {
    float intensity = cfg::kBloomIntensity;
    float aberration = cfg::kCrtAberration;
    float barrel = cfg::kCrtBarrel;
    float vignette = cfg::kCrtVignette;
    float scanDepth = cfg::kCrtScanline;
    float scanCount = cfg::kCrtScanCount;
    float scanOn = scanlinesOn ? 1.0f : 0.0f;

    BeginShaderMode(fx.compositeShader);
    SetShaderValueTexture(fx.compositeShader, fx.locBloomTex, fx.finalBloom);
    SetShaderValue(fx.compositeShader, fx.locBloomIntensity, &intensity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(fx.compositeShader, fx.locAberration, &aberration, SHADER_UNIFORM_FLOAT);
    SetShaderValue(fx.compositeShader, fx.locBarrel, &barrel, SHADER_UNIFORM_FLOAT);
    SetShaderValue(fx.compositeShader, fx.locVignette, &vignette, SHADER_UNIFORM_FLOAT);
    SetShaderValue(fx.compositeShader, fx.locScanDepth, &scanDepth, SHADER_UNIFORM_FLOAT);
    SetShaderValue(fx.compositeShader, fx.locScanCount, &scanCount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(fx.compositeShader, fx.locScanOn, &scanOn, SHADER_UNIFORM_FLOAT);
    // scene is texture0 (auto-bound); positive source rect, shader flips Y
    DrawTexturePro(scene, {0, 0, (float)scene.width, (float)scene.height}, dst, {0, 0}, 0.0f, WHITE);
    EndShaderMode();
}
