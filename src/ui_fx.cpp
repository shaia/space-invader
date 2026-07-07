#include "content.h"
#include "game.h"
#include "render.h"
#include <cmath>

namespace {
constexpr float kToastDur = 3.2f;
constexpr int kBubbleFont = 16;
constexpr int kToastFont = 16;

Vector2 GridCenter(const Game& g) {
    Vector2 sum{0, 0};
    int n = 0;
    for (const auto& v : g.invaders) {
        if (!v.alive) continue;
        sum.x += v.pos.x;
        sum.y += v.pos.y;
        n++;
    }
    if (n == 0) return {cfg::kCanvasW / 2.0f, 300.0f};
    return {sum.x / n, sum.y / n};
}

// resolve where a bubble's tail should point right now; false = anchor is gone
bool AnchorPos(const Game& g, const Bubble& bb, Vector2& out) {
    if (bb.anchor >= 0) {
        const Invader& v = g.invaders[bb.anchor];
        if (!v.alive) return false;
        out = v.pos;
        return true;
    }
    if (bb.anchor == kBubbleAnchorBoss) {
        if (!g.boss.active) return false;
        out = g.boss.pos;
        return true;
    }
    if (bb.anchor == kBubbleAnchorUfo) {
        if (!g.ufo.active) return false;
        out = g.ufo.pos;
        return true;
    }
    if (bb.anchor <= kBubbleAnchorFallerBase) {
        uint32_t id = (uint32_t)(kBubbleAnchorFallerBase - bb.anchor);
        for (const auto& f : g.fallers)
            if (f.id == id) { out = f.pos; return true; }
        return false;
    }
    out = bb.pos;
    return true;
}
} // namespace

void PushBubble(Game& g, int anchor, std::string_view text, float dur) {
    if (g.uifx.bubbles.size() >= 2) g.uifx.bubbles.erase(g.uifx.bubbles.begin());
    Bubble b;
    b.text.assign(text);
    b.anchor = anchor;
    b.dur = dur;
    if (anchor == kBubbleAnchorFixed) b.pos = GridCenter(g);
    g.uifx.bubbles.push_back(b);
}

void PushToast(Game& g, std::string_view text) {
    if ((int)g.uifx.toasts.size() >= 6) g.uifx.toasts.erase(g.uifx.toasts.begin());
    Toast t;
    t.text.assign(text);
    g.uifx.toasts.push_back(t);
}

void Announce(Game& g, std::string_view big, std::string_view small, float dur) {
    g.uifx.card.big.assign(big);
    g.uifx.card.small.assign(small);
    g.uifx.card.t = 0;
    g.uifx.card.dur = dur;
}

void TryAward(Game& g, Ach id) {
    uint32_t bit = 1u << (int)id;
    if (g.uifx.achAwarded & bit) return;
    g.uifx.achAwarded |= bit;
    const content::AchDef& d = content::kAch[(int)id];
    PushToast(g, TextFormat("ACHIEVEMENT: %s", d.name.data()));
    PushToast(g, d.desc);
    PlaySfx(*g.audio, Sfx::Ding);
}

void UpdateUiFx(Game& g, float dt) {
    for (auto& b : g.uifx.bubbles) {
        b.t += dt;
        Vector2 p;
        if (!AnchorPos(g, b, p) && b.anchor != kBubbleAnchorFixed) {
            // a faller exited via the floor, mid-quip; let the line die with dignity
            if (b.anchor <= kBubbleAnchorFallerBase) {
                b.t = b.dur;
                continue;
            }
            // the speaker was shot mid-sentence; re-anchor at the grid center
            b.pos = GridCenter(g);
            b.anchor = kBubbleAnchorFixed;
            if (b.text != content::kAnchorDied) {
                b.text = content::kAnchorDied;
                b.t = 0;
                b.dur = 1.4f;
            }
        }
    }
    std::erase_if(g.uifx.bubbles, [](const Bubble& b) { return b.t >= b.dur; });

    for (auto& t : g.uifx.toasts) t.t += dt;
    std::erase_if(g.uifx.toasts, [](const Toast& t) { return t.t >= kToastDur; });

    if (g.uifx.card.dur > 0) g.uifx.card.t += dt;

    // ambient invader commentary
    if (g.aliveCount > 3 && !g.wave.bossWave && g.uifx.bubbles.empty()) {
        g.uifx.ambientTimer -= dt;
        if (g.uifx.ambientTimer <= 0) {
            g.uifx.ambientTimer = g.rng.range(cfg::kBubbleAmbientMin, cfg::kBubbleAmbientMax);
            for (int tries = 0; tries < 30; tries++) {
                int idx = g.rng.irange(0, cfg::kGridCount - 1);
                if (g.invaders[idx].alive) {
                    PushBubble(g, idx,
                               content::kAmbient[g.rng.irange(0, content::kAmbientCount - 1)], 4.5f);
                    break;
                }
            }
        }
    }
}

// Speech bubbles use a bright white panel, so they are drawn in a separate crisp
// overlay pass (main.cpp) *after* the bloom bright-pass — otherwise the bloom smears
// them into a glowing smudge.
void DrawSpeechBubbles(const Game& g) {
    for (const auto& b : g.uifx.bubbles) {
        Vector2 at;
        if (!AnchorPos(g, b, at)) continue;
        float fade = 1.0f;
        if (b.t < 0.2f) fade = b.t / 0.2f;
        if (b.dur - b.t < 0.3f) fade = (b.dur - b.t) / 0.3f;

        Vector2 sz = MeasureTextEx(GetFontDefault(), b.text.c_str(), kBubbleFont, 1.0f);
        float pad = 8.0f;
        float bw = sz.x + pad * 2, bh = sz.y + pad * 2;
        float bx = at.x - bw / 2, by = at.y - cfg::kInvaderH - bh - 10;
        if (bx < 4) bx = 4;
        if (bx + bw > cfg::kCanvasW - 4) bx = cfg::kCanvasW - 4 - bw;
        if (by < cfg::kHudTopH + 4) by = at.y + cfg::kInvaderH + 12;  // flip below

        Color bg = WithAlpha({235, 238, 245, 255}, 0.92f * fade);
        Color fg = WithAlpha({30, 32, 44, 255}, fade);
        DrawRectangleRounded({bx, by, bw, bh}, 0.4f, 8, bg);
        // tail
        Vector2 tip = {at.x, at.y - cfg::kInvaderH / 2 - 2};
        if (by > at.y) tip.y = at.y + cfg::kInvaderH / 2 + 2;
        DrawTriangle({bx + bw / 2 - 6, by + (by > at.y ? 0 : bh)},
                     {bx + bw / 2 + 6, by + (by > at.y ? 0 : bh)}, tip, bg);
        DrawTextEx(GetFontDefault(), b.text.c_str(), {bx + pad, by + pad}, kBubbleFont, 1.0f, fg);
    }
}

void DrawUiFx(const Game& g) {
    // ---- toasts ----
    int shown = 0;
    for (int i = (int)g.uifx.toasts.size() - 1; i >= 0 && shown < cfg::kMaxToasts; i--, shown++) {
        const Toast& t = g.uifx.toasts[i];
        float slide = 1.0f;
        if (t.t < 0.25f) slide = t.t / 0.25f;
        if (kToastDur - t.t < 0.4f) slide = (kToastDur - t.t) / 0.4f;
        int w = MeasureText(t.text.c_str(), kToastFont);
        float x = cfg::kCanvasW - (w + 26.0f) * slide;
        float y = cfg::kCanvasH - 124.0f - shown * 34.0f;
        DrawRectangleRec({x, y, (float)w + 20, 28}, WithAlpha({30, 32, 50, 255}, 0.85f));
        DrawRectangleRec({x, y, 3, 28}, cfg::kColAccent);
        DrawText(t.text.c_str(), (int)x + 10, (int)y + 6, kToastFont, WithAlpha(RAYWHITE, slide));
    }

    // ---- announcement card ----
    const Announcement& c = g.uifx.card;
    if (c.dur > 0 && c.t < c.dur) {
        float fade = 1.0f;
        if (c.t < 0.3f) fade = c.t / 0.3f;
        if (c.dur - c.t < 0.5f) fade = (c.dur - c.t) / 0.5f;
        float cy = cfg::kCanvasH * 0.38f;
        DrawRectangleRec({0, cy - 46, (float)cfg::kCanvasW, 108}, WithAlpha({10, 10, 22, 255}, 0.75f * fade));
        DrawRectangleRec({0, cy - 46, (float)cfg::kCanvasW, 2}, WithAlpha(cfg::kColAccent, fade));
        DrawRectangleRec({0, cy + 60, (float)cfg::kCanvasW, 2}, WithAlpha(cfg::kColAccent, fade));
        int bw = MeasureText(c.big.c_str(), 36);
        GlowText(c.big.c_str(), cfg::kCanvasW / 2 - bw / 2, (int)cy - 30, 36,
                 WithAlpha(cfg::kColAccent, fade));
        Vector2 sw = MeasureTextEx(GetFontDefault(), c.small.c_str(), 16, 1.0f);
        DrawTextEx(GetFontDefault(), c.small.c_str(),
                   {cfg::kCanvasW / 2.0f - sw.x / 2, cy + 16}, 16, 1.0f,
                   WithAlpha(cfg::kColHud, fade));
    }
}
