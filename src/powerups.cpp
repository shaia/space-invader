#include "content.h"
#include "game.h"
#include "render.h"
#include <cmath>

namespace {
struct PickupDef {
    PowerKind kind;
    std::string_view glyph;
    Color color;
    std::string_view toast;
};

const PickupDef kDefs[(int)PowerKind::COUNT] = {
    {PowerKind::Spread,    "W", {90, 200, 255, 255},  content::kToastSpread},
    {PowerKind::Pierce,    "!", {255, 90, 200, 255},  content::kToastPierce},
    {PowerKind::Rapid,     "E", {255, 220, 90, 255},  content::kToastRapid},
    {PowerKind::Freeze,    "U", {140, 255, 255, 255}, content::kToastFreeze},
    {PowerKind::Shield,    "B", {200, 120, 255, 255}, content::kToastShield},
    {PowerKind::ExtraLife, "+", {80, 255, 140, 255},  content::kToastLife},
    {PowerKind::BigShot,   "$", {255, 170, 90, 255},  content::kToastBigShot},
    {PowerKind::DejaVu,    "D", {255, 255, 255, 255}, content::kToastDejaVu},
};

const PickupDef& Def(PowerKind k) { return kDefs[(int)k]; }
} // namespace

void MaybeDropPickup(Game& g, Vector2 pos) {
    if ((int)g.pickups.size() >= cfg::kMaxFalling) return;
    if (!g.rng.chance(cfg::kDropChance * CollectMemoFx(g).dropMult)) return;  // PERFORMANCE BONUS halves drops
    PowerUp p;
    p.pos = pos;
    int roll = g.rng.irange(0, (int)PowerKind::COUNT - 1);
    // ExtraLife is rarer: reroll it half the time
    if ((PowerKind)roll == PowerKind::ExtraLife && g.rng.chance(0.5f))
        roll = g.rng.irange(0, (int)PowerKind::COUNT - 1);
    p.kind = (PowerKind)roll;
    g.pickups.push_back(p);
}

void UpdatePickups(Game& g, float dt) {
    for (auto& p : g.pickups) {
        p.pos.y += cfg::kPickupFall * dt;
        p.wobble += dt;
    }
    // catch with the cannon
    Rectangle pr = PlayerRect(g);
    for (auto& p : g.pickups) {
        Rectangle box = {p.pos.x - cfg::kPickupSize / 2, p.pos.y - cfg::kPickupSize / 2,
                         cfg::kPickupSize, cfg::kPickupSize};
        if (g.player.alive && CheckCollisionRecs(pr, box)) {
            ActivatePickup(g, p.kind);
            p.pos.y = cfg::kCanvasH + 100;  // consumed
        }
    }
    std::erase_if(g.pickups, [](const PowerUp& p) { return p.pos.y > cfg::kCanvasH + 20; });
}

void ActivatePickup(Game& g, PowerKind kind) {
    PlaySfx(*g.audio, Sfx::Pickup);
    PushToast(g, Def(kind).toast);
    SpawnExplosion(g, g.player.pos, Def(kind).color, 14);
    g.stats.powerups++;

    switch (kind) {
    case PowerKind::Spread: g.fx.spread = cfg::kFxSpread; break;
    case PowerKind::Pierce: g.fx.pierce = cfg::kFxPierce; break;
    case PowerKind::Rapid:  g.fx.rapid = cfg::kFxRapid; break;
    case PowerKind::Freeze: {
        g.fx.freeze = cfg::kFxFreeze;
        int idx = RandomAliveInvader(g);  // one invader announces the break
        if (idx >= 0) PushBubble(g, idx, content::kFreezeBreak, cfg::kFxFreeze);
        break;
    }
    case PowerKind::Shield: g.player.shieldHits = CollectMemoFx(g).shieldCap; break;
    case PowerKind::ExtraLife:
        if (g.lives < cfg::kMaxLives) g.lives++;
        break;
    case PowerKind::BigShot: g.fx.bigShotArmed = true; break;
    case PowerKind::DejaVu: RestoreBunkers(g); break;
    default: break;
    }

    if (ActiveEffectCount(g) >= 3) TryAward(g, Ach::Hoarder);
}

int ActiveEffectCount(const Game& g) {
    int n = 0;
    if (g.fx.spread > 0) n++;
    if (g.fx.pierce > 0) n++;
    if (g.fx.rapid > 0) n++;
    if (g.fx.freeze > 0) n++;
    if (g.fx.bigShotArmed) n++;
    if (g.player.shieldHits > 0) n++;
    return n;
}

void UpdateEffects(Game& g, float dt) {
    auto tick = [&](float& t) {
        if (t > 0) {
            t -= dt;
            if (t <= 0) {
                t = 0;
                PlaySfx(*g.audio, Sfx::Expire);
            }
        }
    };
    tick(g.fx.spread);
    tick(g.fx.pierce);
    tick(g.fx.rapid);
    tick(g.fx.freeze);
}

void DrawPickups(const Game& g) {
    for (const auto& p : g.pickups) {
        const PickupDef& d = Def(p.kind);
        float bob = sinf(p.wobble * 5.0f) * 3.0f;
        Vector2 at = {p.pos.x, p.pos.y + bob};
        float s = cfg::kPickupSize;
        GlowRectRot({at.x - s / 2, at.y - s / 2, s, s}, p.wobble * 40.0f, WithAlpha(d.color, 0.35f));
        GlowCircle(at, s * 0.42f, WithAlpha(d.color, 0.9f));
        int w = MeasureText(d.glyph.data(), 20);
        DrawText(d.glyph.data(), (int)at.x - w / 2, (int)at.y - 10, 20, cfg::kColBg);
    }
}

void DrawEffectHud(const Game& g) {
    // active effect chips, bottom-left above the lives row
    float x = 12.0f;
    float y = cfg::kCanvasH - 66.0f;
    auto chip = [&](PowerKind k, float remain, bool timed) {
        const PickupDef& d = Def(k);
        DrawRectangleRec({x, y, 34, 22}, WithAlpha(d.color, 0.25f));
        DrawRectangleLinesEx({x, y, 34, 22}, 1, d.color);
        DrawText(d.glyph.data(), (int)x + 4, (int)y + 3, 16, d.color);
        if (timed)
            DrawText(TextFormat("%.0f", remain), (int)x + 18, (int)y + 5, 12, cfg::kColHud);
        x += 40;
    };
    if (g.fx.spread > 0) chip(PowerKind::Spread, g.fx.spread, true);
    if (g.fx.pierce > 0) chip(PowerKind::Pierce, g.fx.pierce, true);
    if (g.fx.rapid > 0) chip(PowerKind::Rapid, g.fx.rapid, true);
    if (g.fx.freeze > 0) chip(PowerKind::Freeze, g.fx.freeze, true);
    if (g.fx.bigShotArmed) chip(PowerKind::BigShot, 0, false);
    if (g.player.shieldHits > 0) chip(PowerKind::Shield, (float)g.player.shieldHits, true);
}
