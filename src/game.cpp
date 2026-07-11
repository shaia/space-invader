#include "content.h"
#include "game.h"
#include "render.h"
#include "rlgl.h"
#include <cmath>

const Modifier& CurrentMod(const Game& g) { return GetModifier(g.wave.modifier); }

Rectangle PlayerRect(const Game& g) {
    return {g.player.pos.x - cfg::kPlayerW / 2, g.player.pos.y - cfg::kPlayerH / 2,
            cfg::kPlayerW, cfg::kPlayerH};
}

bool WorldFrozen(const Game& g) {
    return !g.player.alive || g.wave.clearing;
}

void AddScore(Game& g, int points) {
    g.score += (int)((float)points * CurrentMod(g).scoreMult);
    if (g.score > g.hiScore) g.hiScore = g.score;
}

int RandomAliveInvader(Game& g) {
    if (g.aliveCount <= 0) return -1;
    for (int tries = 0; tries < 30; tries++) {
        int idx = g.rng.irange(0, cfg::kGridCount - 1);
        if (g.invaders[idx].alive) return idx;
    }
    for (int i = 0; i < cfg::kGridCount; i++)  // deterministic fallback
        if (g.invaders[i].alive) return i;
    return -1;
}

// Score a kill through the Productivity Streak. Each kill inside the window
// extends the chain; crossing a tier threshold multiplies score and fires a
// deadpan callout. The score pop grows and shifts toward the accent color with
// the tier so the reward reads visually, not just numerically.
void ComboKill(Game& g, Vector2 pos, int basePts, Color c) {
    Combo& cb = g.combo;
    int oldTier = cb.tier;
    cb.chain++;
    cb.timer = cfg::kComboWindow;
    int tier = 0;
    for (int t = 0; t < 3; t++)
        if (cb.chain >= cfg::kComboTierChain[t]) tier = t + 1;
    cb.tier = tier;
    if (cb.chain > g.stats.maxChain) g.stats.maxChain = cb.chain;

    AddScore(g, basePts * cfg::kComboMult[tier]);

    // a brief freeze on the kill, deepening with the streak
    g.hitStop = fmaxf(g.hitStop,
                      fminf(cfg::kHitStopKill + (float)cb.chain * cfg::kHitStopComboBonus,
                            cfg::kHitStopMax));

    const float sizeByTier[4] = {16.0f, 18.0f, 22.0f, 26.0f};
    SpawnScorePop(g, pos, basePts * cfg::kComboMult[tier],
                  tier > 0 ? cfg::kColAccent : c, sizeByTier[tier]);

    if (tier > oldTier) {  // crossed into a higher tier this kill
        PushToast(g, content::kComboTierLines[tier - 1]);
        PlaySfx(*g.audio, Sfx::Ding, 1.0f + 0.2f * (float)tier);
    }
}

void InitStarfield(Game& g) {
    g.stars.clear();
    g.stars.reserve((size_t)cfg::kStarLayers * cfg::kStarsPerLayer);
    // varied star tints: white / azure / rose / gold
    const Color tints[4] = {{235, 240, 255, 255}, {150, 210, 255, 255},
                            {255, 170, 220, 255}, {255, 225, 160, 255}};
    for (int layer = 0; layer < cfg::kStarLayers; layer++) {
        float depth = (float)layer / (float)(cfg::kStarLayers - 1);  // 0 far .. 1 near
        for (int i = 0; i < cfg::kStarsPerLayer; i++) {
            Star s;
            s.pos = {g.rng.range(0, (float)cfg::kCanvasW), g.rng.range(0, (float)cfg::kCanvasH)};
            s.speed = 6.0f + depth * 26.0f;    // near layers scroll faster
            s.size = 0.6f + depth * 1.9f;
            s.phase = g.rng.range(0, 6.2831f);
            s.tint = tints[g.rng.irange(0, 3)];
            g.stars.push_back(s);
        }
    }
}

void ResetRun(Game& g) {
    AudioBank* audio = g.audio;
    int hi = g.hiScore;
    uint32_t rngState = g.rng.s;
    g = Game{};
    g.audio = audio;
    g.hiScore = hi;
    g.rng.s = rngState ? rngState : 0x9E3779B9u;
    g.player.pos = {cfg::kCanvasW / 2.0f, cfg::kPlayerY};
    g.ufo.spawnTimer = g.rng.range(cfg::kUfoMinGap, cfg::kUfoMaxGap);
    InitStarfield(g);
    InitBunkers(g);
    StartWave(g, 1);
}

void StartWave(Game& g, int number) {
    g.wave.number = number;
    g.wave.clearing = false;
    g.wave.intermission = 0;
    g.wave.bossWave = (number % cfg::kBossEvery == 0);
    g.wave.modifier = ModifierId::None;
    g.boss.active = false;
    g.stepFlash = 0;
    g.stats.waveGrazes = 0;
    g.comboBroken = false;

    // enemy leftovers don't carry across waves
    std::erase_if(g.shots, [](const Shot& s) { return !s.fromPlayer; });
    g.fallers.clear();

    if (g.wave.bossWave) {
        for (auto& v : g.invaders) v.alive = false;
        g.aliveCount = 0;
        StartBoss(g);
        return;
    }

    if (number >= cfg::kFirstModifierWave)
        g.wave.modifier = PickNextModifier(g);

    SpawnGrid(g);

    const Modifier& m = CurrentMod(g);
    if (m.id != ModifierId::None) {
        Announce(g, TextFormat("WAVE %d: %s", number, m.name.data()), m.tagline, cfg::kWaveCardDur);
    } else {
        const char* quip = content::kPlainWaveSmall[g.rng.irange(0, content::kPlainWaveSmallCount - 1)];
        Announce(g, TextFormat("WAVE %d", number), number == 1 ? "They have demands." : quip,
                 cfg::kWaveCardDur);
    }
}

namespace {

void FinishWave(Game& g) {
    g.wave.clearing = true;
    g.wave.intermission = cfg::kIntermission;
    g.stats.wavesCleared++;
    int bonus = cfg::kWaveBonusPer * g.wave.number;
    AddScore(g, bonus);
    PushToast(g, TextFormat("Wave %d cleared. Severance: %d pts.", g.wave.number, bonus));
    if (!AnyBunkerAlive(g)) TryAward(g, Ach::ThisIsFine);
}

void UpdateShots(Game& g, float dt) {
    for (auto& s : g.shots) {
        s.pos.x += s.vel.x * dt;
        s.pos.y += s.vel.y * dt;
        if (s.kind == ShotKind::Clipboard) s.spin += 420.0f * dt;
        if (s.fromPlayer && s.kind == ShotKind::PlayerShot && g.rng.chance(0.5f))
            SpawnTrail(g, s.pos, s.pierce ? cfg::kColAccent : cfg::kColShot);
        if (s.kind == ShotKind::BigShot && g.rng.chance(0.8f))
            SpawnTrail(g, {s.pos.x + g.rng.range(-20, 20), s.pos.y + 20}, cfg::kColPlayer);
    }
    std::erase_if(g.shots, [](const Shot& s) {
        return s.pos.y < cfg::kHudTopH - 20 || s.pos.y > cfg::kCanvasH + 30 ||
               s.pos.x < -40 || s.pos.x > cfg::kCanvasW + 40;
    });
}

Rectangle ShotRect(const Shot& s) {
    switch (s.kind) {
    case ShotKind::BigShot: return {s.pos.x - 26, s.pos.y - 26, 52, 52};
    case ShotKind::Clipboard: return {s.pos.x - 8, s.pos.y - 10, 16, 20};
    case ShotKind::Brick: return {s.pos.x - 22, s.pos.y - 10, 44, 20};
    case ShotKind::Compliment: return {s.pos.x - 20, s.pos.y - 8, 40, 16};
    default: return {s.pos.x - cfg::kShotW / 2, s.pos.y - cfg::kShotH / 2, cfg::kShotW, cfg::kShotH};
    }
}

#if DEBUG_KEYS
void DebugKeys(Game& g) {
    if (IsKeyPressed(KEY_F2)) {  // skip wave (kills the boss too; UpdateBoss notices hp<=0)
        for (auto& v : g.invaders) v.alive = false;
        g.aliveCount = 0;
        if (g.boss.active) g.boss.hp = 0;
        if (!g.wave.clearing && !g.boss.active) FinishWave(g);
    }
    if (IsKeyPressed(KEY_F3))
        ActivatePickup(g, (PowerKind)g.rng.irange(0, (int)PowerKind::COUNT - 1));
    if (IsKeyPressed(KEY_F4)) {  // force-cycle modifier
        int next = ((int)g.wave.modifier + 1) % (int)ModifierId::COUNT;
        if (next == 0) next = 1;
        g.wave.modifier = (ModifierId)next;
        const Modifier& m = CurrentMod(g);
        Announce(g, m.name, m.tagline, 1.5f);
    }
    if (IsKeyPressed(KEY_F5)) {  // kill all but one
        bool kept = false;
        for (int i = cfg::kGridCount - 1; i >= 0; i--) {
            if (!g.invaders[i].alive) continue;
            if (!kept) { kept = true; continue; }
            g.invaders[i].alive = false;
            g.aliveCount--;
        }
    }
    if (IsKeyPressed(KEY_F6)) {  // sacrifice a life (tests death/game-over flow)
        g.player.invuln = 0;
        HitPlayer(g, "debug");
    }
}
#endif

} // namespace

void ResolveCollisions(Game& g) {
    // ---- player shots ----
    for (auto& s : g.shots) {
        if (!s.fromPlayer) continue;
        bool consumed = false;

        // your own bunkers block friendly fire (a classic workplace hazard)
        Vector2 hitPoint;
        if (s.kind != ShotKind::BigShot && ShotHitsBunker(g, s, hitPoint)) {
            CarveBunkers(g, hitPoint, cfg::kCarveShot);
            PlaySfx(*g.audio, Sfx::Crunch, 1.4f);
            s.pos.y = -100;  // consumed
            continue;
        }

        Rectangle sr = ShotRect(s);

        for (int i = 0; i < cfg::kGridCount && !consumed; i++) {
            Invader& v = g.invaders[i];
            if (!v.alive) continue;
            if (!CheckCollisionRecs(sr, InvaderRect(g, i))) continue;
            if (!s.tallied) { s.tallied = true; g.stats.shotsHit++; }
            v.hp -= s.kind == ShotKind::BigShot ? 2 : 1;
            if (v.hp > 0) {
                v.hitFlash = 0.25f;
                v.squash = 0.5f;
                PlaySfx(*g.audio, Sfx::Crunch);
            } else {
                KillInvader(g, i);
            }
            consumed = !s.pierce;
        }

        if (!consumed && g.ufo.active) {
            Rectangle ur = {g.ufo.pos.x - cfg::kUfoW / 2, g.ufo.pos.y - cfg::kUfoH / 2,
                            cfg::kUfoW, cfg::kUfoH};
            if (CheckCollisionRecs(sr, ur)) {
                if (!s.tallied) { s.tallied = true; g.stats.shotsHit++; }
                int pts = 50 * g.rng.irange(1, 6);
                ComboKill(g, {g.ufo.pos.x, g.ufo.pos.y - 20.0f}, pts, cfg::kColUfo);
                SpawnExplosion(g, g.ufo.pos, cfg::kColUfo, 30);
                SpawnConfetti(g, g.ufo.pos, 20);
                PushToast(g, TextFormat("THE CONSULTANT: invoiced for %d pts.", pts));
                PlaySfx(*g.audio, Sfx::Pop, 0.7f);
                g.ufo.active = false;
                g.ufo.spawnTimer = g.rng.range(cfg::kUfoMinGap, cfg::kUfoMaxGap);
                consumed = !s.pierce;
            }
        }

        if (!consumed && g.boss.active) {
            bool bhit = BossShotHit(g, s);
            if (bhit && !s.tallied) { s.tallied = true; g.stats.shotsHit++; }
            consumed = bhit;
        }

        // BigShot bulldozes enemy paperwork
        if (s.kind == ShotKind::BigShot) {
            for (auto& e : g.shots) {
                if (e.fromPlayer) continue;
                if (CheckCollisionRecs(sr, ShotRect(e))) {
                    SpawnExplosion(g, e.pos, cfg::kColBomb, 6);
                    e.pos.y = cfg::kCanvasH + 100;
                }
            }
            consumed = false;
        }

        if (consumed) s.pos.y = -100;
    }

    // ---- enemy shots ----
    for (auto& s : g.shots) {
        if (s.fromPlayer) continue;

        Vector2 hitPoint;
        if (ShotHitsBunker(g, s, hitPoint)) {
            float r = s.kind == ShotKind::Brick ? cfg::kCarveBoss : cfg::kCarveBomb;
            bool hadCells = AnyBunkerAlive(g);
            CarveBunkers(g, hitPoint, r);
            PlaySfx(*g.audio, Sfx::Crunch, s.kind == ShotKind::Brick ? 0.7f : 1.0f);
            if (s.kind == ShotKind::Brick) {
                SpawnExplosion(g, s.pos, {255, 90, 90, 255}, 20);
                g.shake = fmaxf(g.shake, 6.0f);
            }
            if (hadCells && !AnyBunkerAlive(g))
                PushBubble(g, kBubbleAnchorFixed, content::kBunkerDown, 3.0f);
            s.pos.y = cfg::kCanvasH + 100;
            continue;
        }

        // graze: an enemy shot sliding past (near but not touching) pays hazard pay.
        // Checked before the hit test so the two are mutually exclusive this frame.
        if (!s.grazed && g.player.alive && g.player.invuln <= 0) {
            Rectangle pr = PlayerRect(g);
            Rectangle grazeBox = {pr.x - cfg::kGrazeRadius, pr.y - cfg::kGrazeRadius,
                                  pr.width + 2 * cfg::kGrazeRadius, pr.height + 2 * cfg::kGrazeRadius};
            Rectangle sr = ShotRect(s);
            if (CheckCollisionRecs(sr, grazeBox) && !CheckCollisionRecs(sr, pr)) {
                s.grazed = true;
                AddScore(g, cfg::kGrazePoints);
                g.stats.grazes++;
                g.stats.waveGrazes++;
                if (g.combo.timer > 0)  // extends an active streak; never starts one
                    g.combo.timer = fminf(cfg::kComboWindow, g.combo.timer + cfg::kGrazeComboExtend);
                SpawnScorePop(g, {s.pos.x, s.pos.y - 10.0f}, cfg::kGrazePoints, cfg::kColUfo, 14.0f,
                              content::kHazardPay);
                PlaySfx(*g.audio, Sfx::Blip, 1.8f);
            }
        }

        if (g.player.alive && CheckCollisionRecs(ShotRect(s), PlayerRect(g))) {
            HitPlayer(g, s.kind == ShotKind::Compliment ? "compliment" : "projectile");
            s.pos.y = cfg::kCanvasH + 100;
        }

        if (s.kind == ShotKind::Brick && s.pos.y > cfg::kPlayerY + 20) {
            SpawnExplosion(g, s.pos, {255, 90, 90, 255}, 24);
            g.shake = fmaxf(g.shake, 5.0f);
            PlaySfx(*g.audio, Sfx::Crunch, 0.6f);
            s.pos.y = cfg::kCanvasH + 100;
        }
    }

    // ---- invaders vs bunkers and player ----
    for (int i = 0; i < cfg::kGridCount; i++) {
        const Invader& v = g.invaders[i];
        if (!v.alive) continue;
        if (v.pos.y + cfg::kInvaderH / 2 > cfg::kBunkerY - 4 &&
            v.pos.y - cfg::kInvaderH / 2 < cfg::kBunkerY + cfg::kBunkerRows * cfg::kBunkerCell + 4)
            CarveBunkers(g, v.pos, cfg::kInvaderW * 0.55f);
        if (g.player.alive && CheckCollisionRecs(InvaderRect(g, i), PlayerRect(g)))
            HitPlayer(g, "invader");
    }

    if (BossTouchesPlayer(g)) HitPlayer(g, "management");
}

void UpdatePlaying(Game& g, float dt) {
    g.time += dt;
    if (g.shake > 0) g.shake = fmaxf(0.0f, g.shake - cfg::kShakeDecay * dt * (1.0f + g.shake));

    // hit-stop: an impactful kill briefly freezes the whole simulation for extra
    // punch. Cosmetic layers (particles, bubbles, toasts) keep running on real dt
    // so sparks keep flying during the freeze — that reads as juice, not a stall.
    float wdt = dt;
    if (g.hitStop > 0.0f) { g.hitStop = fmaxf(0.0f, g.hitStop - dt); wdt = 0.0f; }

    UpdateUiFx(g, dt);
    UpdateParticles(g, dt);
    UpdatePlayer(g, wdt);

#if DEBUG_KEYS
    DebugKeys(g);
#endif

    // pacifist watch (only meaningful at the very start of a run)
    if (!g.pacifistChecked && g.noShootTimer >= 0) {
        g.noShootTimer += dt;
        if (g.noShootTimer >= 7.0f) {
            g.pacifistChecked = true;
            TryAward(g, Ach::PacifistRun);
        }
    }

    if (g.wave.clearing) {
        g.wave.intermission -= dt;  // real dt: the between-wave beat isn't frozen
        if (g.wave.intermission <= 0)
            StartWave(g, g.wave.number + 1);
        return;
    }

    if (!g.player.alive) return;  // death freeze: the world respectfully pauses

    UpdateInvaders(g, wdt);
    UpdateFallers(g, wdt);
    UpdateUfo(g, wdt);
    UpdateBoss(g, wdt);
    UpdateShots(g, wdt);
    UpdatePickups(g, wdt);
    UpdateEffects(g, wdt);
    ResolveCollisions(g);

    // the streak lapses when its window elapses — silently (the silence is the deadpan)
    if (g.combo.timer > 0) {
        g.combo.timer -= wdt;
        if (g.combo.timer <= 0) { g.combo.timer = 0; g.combo.chain = 0; g.combo.tier = 0; }
    }

    if (!g.wave.bossWave && g.aliveCount == 0 && !g.wave.clearing)
        FinishWave(g);
}

void DrawBackground(const Game& g, float time) {
    // slow-drifting nebula clouds — soft color depth that feeds the bloom
    for (int i = 0; i < 3; i++) {
        float px = cfg::kCanvasW * (0.22f + 0.28f * i) + sinf(time * 0.05f + i * 2.1f) * 60.0f;
        float py = cfg::kCanvasH * (0.24f + 0.22f * i) + cosf(time * 0.04f + i * 1.3f) * 40.0f;
        float rad = 250.0f + 60.0f * sinf(time * 0.07f + i);
        DrawCircleGradient((int)px, (int)py, rad, WithAlpha(cfg::kColNebula[i], 0.18f),
                           WithAlpha(cfg::kColNebula[i], 0.0f));
    }
    // parallax starfield (near layers scroll faster, twinkle)
    for (const auto& s : g.stars) {
        float y = fmodf(s.pos.y + time * s.speed, (float)cfg::kCanvasH);
        float tw = 0.4f + 0.6f * (0.5f + 0.5f * sinf(time * 2.2f + s.phase));
        if (s.size <= 1.0f)
            DrawPixelV({s.pos.x, y}, WithAlpha(s.tint, tw * 0.7f));
        else
            DrawCircleV({s.pos.x, y}, s.size * 0.5f, WithAlpha(s.tint, tw * 0.85f));
    }
}

namespace {

void DrawHud(const Game& g) {
    DrawRectangle(0, 0, cfg::kCanvasW, (int)cfg::kHudTopH, cfg::kColHudBar);
    DrawRectangle(0, (int)cfg::kHudTopH - 2, cfg::kCanvasW, 2, WithAlpha(cfg::kColAccent, 0.5f));

    GlowText(TextFormat("SCORE %06d", g.score), 16, 12, 20, cfg::kColHud);
    const char* hi = TextFormat("HI %06d", g.hiScore);
    int hw = MeasureText(hi, 20);
    GlowText(hi, cfg::kCanvasW / 2 - hw / 2, 12, 20, WithAlpha(cfg::kColHud, 0.8f));
    const char* wv = g.wave.bossWave ? "WAVE: MGMT" : TextFormat("WAVE %d", g.wave.number);
    int ww = MeasureText(wv, 20);
    GlowText(wv, cfg::kCanvasW - ww - 16, 12, 20, cfg::kColHud);

    const Modifier& m = CurrentMod(g);
    if (m.id != ModifierId::None) {
        int mw = MeasureText(m.name.data(), 15);
        DrawText(m.name.data(), cfg::kCanvasW / 2 - mw / 2, 38, 15, WithAlpha(cfg::kColAccent, 0.9f));
    }

    // productivity streak meter, under the score
    if (g.combo.chain >= 2) {
        GlowText(TextFormat("STREAK x%d   MULT x%d", g.combo.chain, cfg::kComboMult[g.combo.tier]),
                 16, 36, 16, cfg::kColAccent);
        float frac = g.combo.timer / cfg::kComboWindow;
        frac = frac < 0 ? 0 : (frac > 1 ? 1 : frac);
        DrawRectangle(16, 53, (int)(150 * frac), 3, WithAlpha(cfg::kColAccent, 0.9f));
    }

    // lives as tiny cannons, bottom-left
    for (int i = 0; i < g.lives; i++)
        DrawPlayerArt({26.0f + i * 34.0f, cfg::kCanvasH - 22.0f}, 24, 14, cfg::kColPlayer, 0);
    DrawLineEx({0, cfg::kCanvasH - 44.0f}, {(float)cfg::kCanvasW, cfg::kCanvasH - 44.0f}, 1,
               WithAlpha(cfg::kColPlayer, 0.35f));

#if DEBUG_KEYS
    if (IsKeyDown(KEY_F1)) {
        DrawRectangle(8, 64, 260, 128, WithAlpha({0, 0, 0, 255}, 0.7f));
        DrawText(TextFormat("fps %d  dt %0.1fms", GetFPS(), GetFrameTime() * 1000), 14, 70, 12, GREEN);
        DrawText(TextFormat("alive %d shots %d parts %d", g.aliveCount, (int)g.shots.size(),
                            (int)g.particles.size()), 14, 86, 12, GREEN);
        DrawText(TextFormat("wave %d mod %d boss %d hp %.0f", g.wave.number, (int)g.wave.modifier,
                            (int)g.boss.active, g.boss.hp), 14, 102, 12, GREEN);
        DrawText(TextFormat("fx s%.1f p%.1f r%.1f f%.1f", g.fx.spread, g.fx.pierce, g.fx.rapid,
                            g.fx.freeze), 14, 118, 12, GREEN);
        DrawText(TextFormat("combo x%d t%d  hitstop %.0fms", g.combo.chain, g.combo.tier,
                            g.hitStop * 1000.0f), 14, 134, 12, GREEN);
        DrawText(TextFormat("acc %d/%d  grazes %d", g.stats.shotsHit, g.stats.shotsFired,
                            g.stats.grazes), 14, 150, 12, GREEN);
        DrawText("F2 wave F3 pwr F4 mod F5 thin F6 die", 14, 172, 12, GREEN);
    }
#endif
}

} // namespace

void DrawPlaying(const Game& g) {
    ClearBackground(cfg::kColBg);
    DrawBackground(g, g.time);

    const Modifier& m = CurrentMod(g);

    DrawBunkers(g);
    DrawPickups(g);

    // invader grid
    for (int i = 0; i < cfg::kGridCount; i++) {
        const Invader& v = g.invaders[i];
        if (!v.alive) continue;
        int row = i / cfg::kGridCols;
        Color tint = v.tough && v.hp >= 2 ? cfg::kColOverachiever : cfg::kColRow[row];
        if (m.discoHue) tint = HueCycle(tint, g.time + row * 0.2f);
        if (g.fx.freeze > 0) tint = {140, 190, 255, 255};
        float alpha = 1.0f;
        if (m.invisibleInvaders)
            alpha = (g.stepFlash > 0 || v.hitFlash > 0) ? 0.9f : 0.05f;
        if (v.hitFlash > 0) tint = RAYWHITE;
        float s = GridScale(g);
        DrawInvaderArt(v.pos, cfg::kInvaderW * s, cfg::kInvaderH * s, row, g.marchFrame,
                       v.squash, WithAlpha(tint, alpha), m.wobbly, g.time, i);
    }

    // fallers: tumbling refuseniks, fast frame flip = flailing limbs
    for (const auto& f : g.fallers) {
        Color tint = m.discoHue ? HueCycle(cfg::kColRow[f.row], g.time) : cfg::kColRow[f.row];
        rlPushMatrix();
        rlTranslatef(f.pos.x, f.pos.y, 0);
        rlRotatef(f.angle, 0, 0, 1);
        rlTranslatef(-f.pos.x, -f.pos.y, 0);
        DrawInvaderArt(f.pos, cfg::kInvaderW, cfg::kInvaderH, f.row,
                       ((int)(g.time * 8.0f)) & 1, 0.0f, tint, false, g.time, f.seed);
        rlPopMatrix();
    }

    if (g.ufo.active)
        DrawUfoArt(g.ufo.pos, cfg::kUfoW, cfg::kUfoH,
                   m.discoHue ? HueCycle(cfg::kColUfo, g.time) : cfg::kColUfo, g.time);

    DrawBoss(g);

    // ghost cannon (Mirror Match)
    if (m.mirrorCannon) {
        Vector2 ghost = {cfg::kCanvasW - g.player.pos.x, cfg::kHudTopH + 26.0f};
        DrawPlayerArt({ghost.x, ghost.y}, cfg::kPlayerW, -cfg::kPlayerH,
                      WithAlpha(cfg::kColPlayer, 0.35f), 0);
    }

    // player
    if (g.player.alive) {
        float a = 1.0f;
        if (g.player.invuln > 0) a = 0.3f + 0.7f * (0.5f + 0.5f * sinf(g.time * 24.0f));
        Color pc = m.discoHue ? HueCycle(cfg::kColPlayer, g.time) : cfg::kColPlayer;
        DrawPlayerArt(g.player.pos, cfg::kPlayerW, cfg::kPlayerH, WithAlpha(pc, a), g.player.squash, g.time);
        if (g.player.shieldHits > 0)
            GlowCircle(g.player.pos, cfg::kPlayerW * 0.9f,
                       WithAlpha(cfg::kColUfo, 0.10f + 0.05f * g.player.shieldHits));
    }

    for (const auto& s : g.shots) DrawShotArt(g, s);
    if (!g.shots.empty()) {
        // DEADLINE bricks carry their label
        for (const auto& s : g.shots) {
            if (s.kind == ShotKind::Brick) {
                GlowRect({s.pos.x - 22, s.pos.y - 10, 44, 20}, cfg::kColBrick);
                DrawText("DEADLINE", (int)s.pos.x - 20, (int)s.pos.y - 4, 8, RAYWHITE);
            }
        }
    }

    DrawParticles(g);
    DrawUiFx(g);
    DrawHud(g);
    DrawEffectHud(g);
}
