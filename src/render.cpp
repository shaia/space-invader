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
    float g = cfg::kGlowHalo;
    DrawRectangleRec({r.x - g, r.y - g, r.width + 2 * g, r.height + 2 * g}, WithAlpha(c, cfg::kGlowHaloA));
    DrawRectangleRec({r.x - 2, r.y - 2, r.width + 4, r.height + 4}, WithAlpha(c, cfg::kGlowMidA));
    DrawRectangleRec(r, c);
}

void GlowRectRot(Rectangle r, float rot, Color c) {
    Vector2 origin = {r.width / 2, r.height / 2};
    Rectangle at = {r.x + origin.x, r.y + origin.y, r.width, r.height};
    DrawRectanglePro({at.x, at.y, r.width + 6, r.height + 6}, {origin.x + 3, origin.y + 3},
                     rot, WithAlpha(c, cfg::kGlowHaloA + 0.03f));
    DrawRectanglePro(at, origin, rot, c);
}

void GlowCircle(Vector2 center, float radius, Color c) {
    DrawCircleV(center, radius * cfg::kGlowCircleOut, WithAlpha(c, cfg::kGlowHaloA));
    DrawCircleV(center, radius * cfg::kGlowCircleMid, WithAlpha(c, cfg::kGlowMidA));
    DrawCircleV(center, radius, c);
}

void GlowLine(Vector2 a, Vector2 b, float thick, Color c) {
    DrawLineEx(a, b, thick * 3.0f, WithAlpha(c, cfg::kGlowHaloA + 0.03f));
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

int ArchFor(int row) { return row == 0 ? 0 : (row <= 2 ? 1 : 2); }

// scale rgb by a factor (for cheap gradient shading), clamped
Color ShadeColor(Color c, float f) {
    auto s = [&](unsigned char v) { int x = (int)(v * f); return (unsigned char)(x > 255 ? 255 : x); };
    return {s(c.r), s(c.g), s(c.b), c.a};
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
    int arch = ArchFor(row);
    Color accent = cfg::kInvPal[arch].accent;
    Color eye = cfg::kInvPal[arch].eye;
    float sx = 1.0f + squash * 0.5f;
    float sy = 1.0f - squash * 0.35f;
    float cw = (w * sx) / 8.0f;
    float ch = (h * sy) / 6.0f;
    float x0 = c.x - (w * sx) / 2.0f;
    float y0 = c.y - (h * sy) / 2.0f;
    // one soft glow pass behind the whole body
    DrawRectangleRec({x0 - 4, y0 - 4, w * sx + 8, h * sy + 8}, WithAlpha(tint, 0.10f));
    for (int r = 0; r < 6; r++) {
        // vertical shading: bright top, deeper bottom (reads as a lit dome)
        Color rc = ShadeColor(tint, 1.20f - 0.45f * (r / 5.0f));
        for (int b = 0; b < 8; b++) {
            if (!(mask[frame][r] & (1 << (7 - b)))) continue;
            float jx = 0, jy = 0;
            if (wobbly) {
                jx = WobbleOff(seed + r * 8 + b, time, 6.0f, 1.6f);
                jy = WobbleOff(seed + r * 8 + b + 999, time, 5.0f, 1.6f);
            }
            DrawRectangleRec({x0 + b * cw + jx, y0 + r * ch + jy, cw + 0.5f, ch + 0.5f}, rc);
        }
    }
    // eyes: two dark sockets with a small animated accent glint
    float ew = cw * 0.95f, eh = ch * 1.05f;
    float eyY = y0 + ch * 1.9f;
    float glint = 0.3f + 0.35f * (0.5f + 0.5f * sinf(time * 3.0f + seed * 0.7f));
    float ex[2] = {c.x - cw * 1.7f, c.x + cw * 0.75f};
    for (int i = 0; i < 2; i++) {
        DrawRectangleRec({ex[i], eyY, ew, eh}, eye);
        DrawCircleV({ex[i] + ew * 0.6f, eyY + eh * 0.35f}, cw * 0.24f, WithAlpha(accent, glint));
    }
}

void DrawPlayerArt(Vector2 c, float w, float h, Color tint, float squash, float time) {
    float sx = 1.0f + squash * 0.4f;
    float sy = 1.0f - squash * 0.3f;
    float hw = w * sx / 2, hh = h * sy / 2;
    bool flip = h < 0;  // the Mirror Match ghost draws upside down

    // thruster flame (live cannon only) — flickering, cyan→violet
    if (time > 0.001f && !flip) {
        float f = 0.55f + 0.45f * sinf(time * 40.0f);
        Color fc = {(unsigned char)(110 + (int)(90 * f)), 200, 255, 255};
        float fy = c.y + hh * 0.9f;
        DrawTriangle({c.x - w * 0.15f, fy}, {c.x, fy + h * (0.55f + 0.5f * f)},
                     {c.x + w * 0.15f, fy}, WithAlpha(fc, 0.85f));
        DrawCircleV({c.x, fy}, w * 0.11f, WithAlpha(fc, 0.55f));
    }

    // outer halo
    Rectangle base = {c.x - hw, c.y - hh + h * 0.35f, w * sx, h * sy * 0.65f};
    DrawRectangleRec({base.x - 5, base.y - 5, base.width + 10, base.height + 10}, WithAlpha(tint, 0.12f));
    // hull with a lit top strip
    DrawRectangleRec(base, ShadeColor(tint, 0.85f));
    DrawRectangleRec({base.x, base.y, base.width, base.height * 0.4f}, ShadeColor(tint, 1.15f));
    // swept wing accents
    DrawTriangle({c.x - hw, c.y - hh + h * 0.35f}, {c.x - hw - w * 0.14f, c.y + hh * 0.9f},
                 {c.x - hw + w * 0.12f, c.y + hh * 0.9f}, WithAlpha(tint, 0.85f));
    DrawTriangle({c.x + hw, c.y - hh + h * 0.35f}, {c.x + hw - w * 0.12f, c.y + hh * 0.9f},
                 {c.x + hw + w * 0.14f, c.y + hh * 0.9f}, WithAlpha(tint, 0.85f));
    // turret + cockpit (kept dim so bloom doesn't wash out the green hull)
    DrawRectangleRec({c.x - hw * 0.55f, c.y - hh + h * 0.12f, w * sx * 0.55f, h * 0.3f}, ShadeColor(tint, 1.1f));
    Color cockpit = {150, 255, 210, 255};
    DrawCircleV({c.x, c.y - hh + h * 0.24f}, w * 0.055f, WithAlpha(cockpit, 0.6f));
    // barrel with a subtle charge-glow tip
    DrawRectangleRec({c.x - 3.0f, c.y - hh - h * 0.18f, 6.0f, h * 0.5f}, tint);  // barrel
    if (time > 0.001f && !flip)
        DrawCircleV({c.x, c.y - hh - h * 0.18f}, 3.0f,
                    WithAlpha(cockpit, 0.25f + 0.2f * sinf(time * 8.0f)));
}

void DrawUfoArt(Vector2 c, float w, float h, Color tint, float time) {
    float bob = sinf(time * 6.0f) * 2.0f;
    Vector2 p = {c.x, c.y + bob};
    // tractor-beam underglow
    DrawEllipse((int)p.x, (int)(p.y + h * 0.5f), w * 0.42f, h * 1.3f, WithAlpha(tint, 0.10f));
    // saucer halo + hull (shaded)
    DrawEllipse((int)p.x, (int)p.y, w * 0.78f, h * 0.95f, WithAlpha(tint, 0.14f));
    DrawEllipse((int)p.x, (int)p.y, w * 0.5f, h * 0.5f, ShadeColor(tint, 0.85f));
    DrawEllipse((int)p.x, (int)(p.y - h * 0.12f), w * 0.5f, h * 0.28f, ShadeColor(tint, 1.15f));
    // glowing dome
    DrawEllipse((int)p.x, (int)(p.y - h * 0.35f), w * 0.24f, h * 0.42f, WithAlpha(tint, 0.85f));
    DrawCircleV({p.x, p.y - h * 0.42f}, w * 0.08f, WithAlpha({230, 245, 255, 255}, 0.9f));
    // running lights around the rim
    for (int i = -2; i <= 2; i++) {
        float lx = p.x + i * w * 0.2f;
        bool on = fmodf(time * 4.0f + i, 3.0f) < 1.5f;
        DrawCircleV({lx, p.y + h * 0.28f}, 2.6f, on ? RAYWHITE : WithAlpha(tint, 0.4f));
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
        // .data() is null-terminated: labels are set only from kCompliments[] literals
        const char* t = !s.label.empty() ? s.label.data() : "nice!";
        int wpx = MeasureText(t, 16);
        GlowText(t, (int)s.pos.x - wpx / 2, (int)s.pos.y - 8, 16, cfg::kColCompliment);
        break;
    }
    case ShotKind::Clipboard:
        GlowRectRot({s.pos.x - 8, s.pos.y - 10, 16, 20}, s.spin, cfg::kColClipboard);
        DrawRectanglePro({s.pos.x, s.pos.y - 8, 10, 3}, {5, 1.5f}, s.spin, {120, 120, 130, 255});
        break;
    case ShotKind::Beamlet:
        GlowRect({s.pos.x - 3, s.pos.y - 9, 6, 18}, cfg::kColBeamlet);
        break;
    }
}

// CRT scanlines + vignette now live in the composite shader (postfx.cpp).
