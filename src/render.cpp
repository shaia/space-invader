#include "render.h"
#include "game.h"
#include <cmath>

Color WithAlpha(Color c, float a) {
    if (a < 0) a = 0;
    if (a > 1) a = 1;
    c.a = (unsigned char)(c.a * a);
    return c;
}

Color HueCycle(Color c, float time) {
    Vector3 hsv = ColorToHSV(c);
    hsv.x = fmodf(hsv.x + time * 90.0f, 360.0f);
    return ColorFromHSV(hsv.x, hsv.y > 0.2f ? hsv.y : 0.7f, hsv.z);
}

void GlowRect(Rectangle r, Color c) {
    float g = 5.0f;
    DrawRectangleRec({r.x - g, r.y - g, r.width + 2 * g, r.height + 2 * g}, WithAlpha(c, 0.10f));
    DrawRectangleRec({r.x - 2, r.y - 2, r.width + 4, r.height + 4}, WithAlpha(c, 0.25f));
    DrawRectangleRec(r, c);
}

void GlowRectRot(Rectangle r, float rot, Color c) {
    Vector2 origin = {r.width / 2, r.height / 2};
    Rectangle at = {r.x + origin.x, r.y + origin.y, r.width, r.height};
    DrawRectanglePro({at.x, at.y, r.width + 6, r.height + 6}, {origin.x + 3, origin.y + 3},
                     rot, WithAlpha(c, 0.15f));
    DrawRectanglePro(at, origin, rot, c);
}

void GlowCircle(Vector2 center, float radius, Color c) {
    DrawCircleV(center, radius * 1.8f, WithAlpha(c, 0.12f));
    DrawCircleV(center, radius * 1.2f, WithAlpha(c, 0.3f));
    DrawCircleV(center, radius, c);
}

void GlowLine(Vector2 a, Vector2 b, float thick, Color c) {
    DrawLineEx(a, b, thick * 3.0f, WithAlpha(c, 0.15f));
    DrawLineEx(a, b, thick, c);
}

void GlowText(const char* text, int x, int y, int size, Color c) {
    DrawText(text, x + 2, y + 2, size, WithAlpha(c, 0.25f));
    DrawText(text, x, y, size, c);
}

// Invader silhouettes as cell masks (8x6), two march frames each.
// Row archetypes: 0 = squid (executive), 1-2 = crab (manager), 3-4 = octopus (intern).
namespace {
constexpr uint8_t kSquid[2][6] = {
    {0b00011000, 0b00111100, 0b01111110, 0b11011011, 0b00100100, 0b01011010},
    {0b00011000, 0b00111100, 0b01111110, 0b11011011, 0b01011010, 0b10100101},
};
constexpr uint8_t kCrab[2][6] = {
    {0b00100100, 0b10111101, 0b11111111, 0b11111111, 0b01100110, 0b11000011},
    {0b00100100, 0b00111100, 0b11111111, 0b11111111, 0b01100110, 0b00100100},
};
constexpr uint8_t kOcto[2][6] = {
    {0b00111100, 0b01111110, 0b11111111, 0b11100111, 0b01000010, 0b10000001},
    {0b00111100, 0b01111110, 0b11111111, 0b11100111, 0b00100100, 0b01000010},
};

const uint8_t (*MaskFor(int row))[6] {
    if (row == 0) return kSquid;
    if (row <= 2) return kCrab;
    return kOcto;
}

// deterministic per-invader wobble hash
float WobbleOff(int seed, float time, float speed, float amp) {
    float phase = (float)((seed * 2654435761u) % 628) / 100.0f;
    return sinf(time * speed + phase) * amp;
}
} // namespace

void DrawInvaderArt(Vector2 c, float w, float h, int row, int frame, float squash,
                    Color tint, bool wobbly, float time, int seed) {
    const uint8_t(*mask)[6] = MaskFor(row);
    float sx = 1.0f + squash * 0.5f;
    float sy = 1.0f - squash * 0.35f;
    float cw = (w * sx) / 8.0f;
    float ch = (h * sy) / 6.0f;
    float x0 = c.x - (w * sx) / 2.0f;
    float y0 = c.y - (h * sy) / 2.0f;
    // one soft glow pass behind the whole body
    DrawRectangleRec({x0 - 4, y0 - 4, w * sx + 8, h * sy + 8}, WithAlpha(tint, 0.10f));
    for (int r = 0; r < 6; r++) {
        for (int b = 0; b < 8; b++) {
            if (!(mask[frame][r] & (1 << (7 - b)))) continue;
            float jx = 0, jy = 0;
            if (wobbly) {
                jx = WobbleOff(seed + r * 8 + b, time, 6.0f, 1.6f);
                jy = WobbleOff(seed + r * 8 + b + 999, time, 5.0f, 1.6f);
            }
            DrawRectangleRec({x0 + b * cw + jx, y0 + r * ch + jy, cw + 0.5f, ch + 0.5f}, tint);
        }
    }
}

void DrawPlayerArt(Vector2 c, float w, float h, Color tint, float squash) {
    float sx = 1.0f + squash * 0.4f;
    float sy = 1.0f - squash * 0.3f;
    float hw = w * sx / 2, hh = h * sy / 2;
    Rectangle base = {c.x - hw, c.y - hh + h * 0.35f, w * sx, h * sy * 0.65f};
    DrawRectangleRec({base.x - 5, base.y - 5, base.width + 10, base.height + 10}, WithAlpha(tint, 0.12f));
    DrawRectangleRec(base, tint);
    DrawRectangleRec({c.x - hw * 0.55f, c.y - hh + h * 0.12f, w * sx * 0.55f, h * 0.3f}, tint);
    DrawRectangleRec({c.x - 3.0f, c.y - hh - h * 0.18f, 6.0f, h * 0.5f}, tint);  // barrel
}

void DrawUfoArt(Vector2 c, float w, float h, Color tint, float time) {
    float bob = sinf(time * 6.0f) * 2.0f;
    Vector2 p = {c.x, c.y + bob};
    DrawEllipse((int)p.x, (int)p.y, w * 0.75f, h * 0.9f, WithAlpha(tint, 0.12f));
    DrawEllipse((int)p.x, (int)p.y, w * 0.5f, h * 0.5f, tint);
    DrawEllipse((int)p.x, (int)(p.y - h * 0.35f), w * 0.22f, h * 0.4f, WithAlpha(tint, 0.8f));
    for (int i = -1; i <= 1; i++) {
        float lx = p.x + i * w * 0.28f;
        bool on = fmodf(time * 4.0f + i, 3.0f) < 1.5f;
        DrawCircleV({lx, p.y + h * 0.28f}, 2.5f, on ? RAYWHITE : WithAlpha(tint, 0.4f));
    }
}

void DrawShotArt(const Game& g, const Shot& s) {
    const Modifier& m = CurrentMod(g);
    switch (s.kind) {
    case ShotKind::PlayerShot: {
        Color c = cfg::kColShot;
        if (s.pierce) c = cfg::kColAccent;
        GlowRect({s.pos.x - cfg::kShotW / 2, s.pos.y - cfg::kShotH / 2, cfg::kShotW, cfg::kShotH}, c);
        break;
    }
    case ShotKind::BigShot:
        GlowCircle(s.pos, 26.0f, cfg::kColPlayer);
        GlowText("$", (int)s.pos.x - 6, (int)s.pos.y - 12, 24, cfg::kColBg);
        break;
    case ShotKind::Bomb: {
        Color c = m.discoHue ? HueCycle(cfg::kColBomb, g.time) : cfg::kColBomb;
        float wig = sinf(g.time * 20.0f + s.pos.y * 0.1f) * 2.0f;
        GlowRect({s.pos.x - 2.5f + wig, s.pos.y - 8, 5, 16}, c);
        break;
    }
    case ShotKind::Compliment: {
        const char* t = s.label ? s.label : "nice!";
        int wpx = MeasureText(t, 16);
        GlowText(t, (int)s.pos.x - wpx / 2, (int)s.pos.y - 8, 16, {255, 170, 200, 255});
        break;
    }
    case ShotKind::Clipboard:
        GlowRectRot({s.pos.x - 8, s.pos.y - 10, 16, 20}, s.spin, {230, 220, 180, 255});
        DrawRectanglePro({s.pos.x, s.pos.y - 8, 10, 3}, {5, 1.5f}, s.spin, {120, 120, 130, 255});
        break;
    case ShotKind::Beamlet:
        GlowRect({s.pos.x - 3, s.pos.y - 9, 6, 18}, {255, 90, 90, 255});
        break;
    }
}

void DrawScanlines() {
    if (!cfg::kScanlines) return;
    for (int y = 0; y < cfg::kCanvasH; y += 3) {
        DrawRectangle(0, y, cfg::kCanvasW, 1, {0, 0, 0, 36});
    }
    // vignette: four soft edge gradients
    DrawRectangleGradientV(0, 0, cfg::kCanvasW, 70, {0, 0, 0, 90}, {0, 0, 0, 0});
    DrawRectangleGradientV(0, cfg::kCanvasH - 70, cfg::kCanvasW, 70, {0, 0, 0, 0}, {0, 0, 0, 90});
    DrawRectangleGradientH(0, 0, 60, cfg::kCanvasH, {0, 0, 0, 70}, {0, 0, 0, 0});
    DrawRectangleGradientH(cfg::kCanvasW - 60, 0, 60, cfg::kCanvasH, {0, 0, 0, 0}, {0, 0, 0, 70});
}
