// Boss memos: after each boss you sign one perk-with-a-catch for the rest of the run.
// Behavior is data checked at hook sites (see CollectMemoFx callers), not classes.
#include "game.h"
#include "render.h"

namespace {
// Index matches MemoId. Only the fields a memo actually changes deviate from identity.
const Memo kMemos[(int)MemoId::COUNT] = {
    {.id = MemoId::EspressoBudget, .name = "ESPRESSO BUDGET",
     .buff = "+1 shot on screen", .drawback = "Their bombs fall 25% faster",
     .extraShotCap = 1, .bombSpeedMult = 1.25f},
    {.id = MemoId::PerformanceBonus, .name = "PERFORMANCE BONUS",
     .buff = "Score x1.25", .drawback = "Half the power-up drops",
     .scoreMult = 1.25f, .dropMult = 0.5f},
    {.id = MemoId::KeyPersonInsurance, .name = "KEY PERSON INSURANCE",
     .buff = "+1 life, right now", .drawback = "Shields take 1 hit, not 3",
     .lifeNow = 1, .shieldCap = 1},
    {.id = MemoId::FlexibleHours, .name = "FLEXIBLE HOURS",
     .buff = "Invaders march 12% slower", .drawback = "But bomb 30% more often",
     .marchMult = 1.12f, .bombRateMult = 1.3f},
    {.id = MemoId::OpenFloorPlan, .name = "OPEN FLOOR PLAN",
     .buff = "Graze reach x1.6", .drawback = "Your hitbox grows 15%",
     .grazeMult = 1.6f, .hitboxMult = 1.15f},
    {.id = MemoId::StockOptions, .name = "STOCK OPTIONS",
     .buff = "The UFO visits twice as often", .drawback = "But pays out half",
     .ufoGapMult = 0.5f, .ufoPayMult = 0.5f},
};

void SignMemo(Game& g, MemoId id) {
    g.memoMask |= (1u << (int)id);
    g.stats.memosSigned++;
    const Memo& m = kMemos[(int)id];
    if (m.lifeNow > 0 && g.lives < cfg::kMaxLives) g.lives += m.lifeNow;
    PushToast(g, TextFormat("SIGNED: %s", m.name.data()));
    PlaySfx(*g.audio, Sfx::Ding);
    if (g.stats.memosSigned >= 3) TryAward(g, Ach::PaperTrail);
}
} // namespace

const Memo& GetMemo(MemoId id) { return kMemos[(int)id]; }

MemoFx CollectMemoFx(const Game& g) {
    MemoFx fx;
    for (int i = 0; i < (int)MemoId::COUNT; i++) {
        if (!(g.memoMask & (1u << i))) continue;
        const Memo& m = kMemos[i];
        fx.extraShotCap += m.extraShotCap;
        fx.bombSpeedMult *= m.bombSpeedMult;
        fx.scoreMult *= m.scoreMult;
        fx.dropMult *= m.dropMult;
        fx.marchMult *= m.marchMult;
        fx.bombRateMult *= m.bombRateMult;
        fx.grazeMult *= m.grazeMult;
        fx.hitboxMult *= m.hitboxMult;
        fx.ufoGapMult *= m.ufoGapMult;
        fx.ufoPayMult *= m.ufoPayMult;
        if (m.shieldCap > 0) fx.shieldCap = m.shieldCap;  // KEY PERSON caps shields
    }
    return fx;
}

void OfferMemos(Game& g) {
    int avail[(int)MemoId::COUNT], nAvail = 0;
    for (int i = 0; i < (int)MemoId::COUNT; i++)
        if (!(g.memoMask & (1u << i))) avail[nAvail++] = i;
    if (nAvail == 0) return;  // everything's signed; no offer this time

    MemoOffer& o = g.memoOffer;
    o = MemoOffer{};
    o.count = nAvail < 3 ? nAvail : 3;
    for (int k = 0; k < o.count; k++) {  // partial Fisher-Yates from the front
        int j = k + (int)(g.setupRng.uniform() * (float)(nAvail - k));  // deterministic in daily mode
        if (j >= nAvail) j = nAvail - 1;
        int tmp = avail[k]; avail[k] = avail[j]; avail[j] = tmp;
        o.pick[k] = (MemoId)avail[k];
    }
    o.active = true;
    o.timer = cfg::kMemoTimeout;
}

void UpdateMemoOffer(Game& g, float dt) {
    MemoOffer& o = g.memoOffer;
    o.timer -= dt;

    int sign = -1;
    if (IsKeyPressed(KEY_ONE)) sign = 0;
    if (IsKeyPressed(KEY_TWO) && o.count >= 2) sign = 1;
    if (IsKeyPressed(KEY_THREE) && o.count >= 3) sign = 2;
    if (sign >= 0) {
        SignMemo(g, o.pick[sign]);
        o.active = false;
        return;
    }
    if (IsKeyPressed(KEY_ZERO) || IsKeyPressed(KEY_ESCAPE) || o.timer <= 0) {
        PushToast(g, "Memo returned unsigned. Noted.");
        o.active = false;
    }
}

void DrawMemoOffer(const Game& g) {
    const MemoOffer& o = g.memoOffer;
    DrawRectangle(0, 0, cfg::kCanvasW, cfg::kCanvasH, WithAlpha({6, 6, 14, 255}, 0.78f));

    const char* header = "INTEROFFICE MEMORANDUM - SIGN ONE (1)";
    int hw = MeasureText(header, 24);
    GlowText(header, cfg::kCanvasW / 2 - hw / 2, 150, 24, cfg::kColAccent);

    const float cardW = 640, cardH = 128, x = (cfg::kCanvasW - cardW) / 2;
    for (int i = 0; i < o.count; i++) {
        const Memo& m = kMemos[(int)o.pick[i]];
        float y = 230 + i * (cardH + 18);
        DrawRectangleRec({x, y, cardW, cardH}, WithAlpha({20, 22, 40, 255}, 0.95f));
        DrawRectangleLinesEx({x, y, cardW, cardH}, 2, WithAlpha(cfg::kColAccent, 0.8f));
        GlowText(TextFormat("[%d]", i + 1), (int)x + 18, (int)y + 16, 30, cfg::kColPlayer);
        GlowText(m.name.data(), (int)x + 80, (int)y + 18, 26, cfg::kColHud);
        DrawText(TextFormat("+ %s", m.buff.data()), (int)x + 82, (int)y + 60, 18, cfg::kColPlayer);
        DrawText(TextFormat("- %s", m.drawback.data()), (int)x + 82, (int)y + 88, 18, cfg::kColHurt);
    }

    // countdown + decline hint
    float frac = o.timer / cfg::kMemoTimeout;
    if (frac < 0) frac = 0;
    DrawRectangle((int)x, (int)(230 + o.count * (cardH + 18)) + 6, (int)(cardW * frac), 4, cfg::kColAccent);
    const char* hint = TextFormat("Press 1-%d to sign   -   0/ESC to decline   -   %.0fs", o.count, o.timer);
    int tw = MeasureText(hint, 16);
    DrawText(hint, cfg::kCanvasW / 2 - tw / 2, (int)(230 + o.count * (cardH + 18)) + 20, 16,
             WithAlpha(cfg::kColHud, 0.85f));
}
