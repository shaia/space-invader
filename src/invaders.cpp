#include "content.h"
#include "game.h"
#include "render.h"
#include <cmath>

float GridScale(const Game& g) { return CurrentMod(g).scale; }

Rectangle InvaderRect(const Game& g, int idx) {
    float s = GridScale(g);
    const Invader& v = g.invaders[idx];
    float w = cfg::kInvaderW * s, h = cfg::kInvaderH * s;
    return {v.pos.x - w / 2, v.pos.y - h / 2, w, h};
}

void SpawnGrid(Game& g) {
    const Modifier& m = CurrentMod(g);
    float gridW = (cfg::kGridCols - 1) * cfg::kGridSpacingX;
    float x0 = (cfg::kCanvasW - gridW) / 2.0f;
    float y0 = cfg::kGridTopY + m.startRowsLower * cfg::kMarchDropY;

    g.aliveCount = 0;
    for (int r = 0; r < cfg::kGridRows; r++) {
        for (int c = 0; c < cfg::kGridCols; c++) {
            Invader& v = g.invaders[r * cfg::kGridCols + c];
            v.pos = {x0 + c * cfg::kGridSpacingX, y0 + r * cfg::kGridSpacingY};
            v.alive = true;
            v.hp = 1;
            v.tough = false;
            v.squash = 0;
            v.hitFlash = 0;
            if (g.wave.number >= 4 && g.setupRng.chance(cfg::kOverachieverFrac)) {
                v.hp = 2;
                v.tough = true;
            }
            g.aliveCount++;
        }
    }
    g.marchDir = 1;
    g.marchTimer = 0;
    g.descendPending = false;
    g.marchFrame = 0;
    g.marchNoteIdx = 0;
    g.bombTimer = 0;
}

namespace {

float StepInterval(const Game& g) {
    float t = (float)(g.aliveCount - 1) / (float)(cfg::kGridCount - 1);
    if (t < 0) t = 0;
    float base = cfg::kMarchFastest + t * (cfg::kMarchSlowest - cfg::kMarchFastest);
    float waveScale = powf(cfg::kWaveSpeedMult, (float)(g.wave.number - 1));
    float interval = base / waveScale;
    if (g.aliveCount == 1) interval *= cfg::kPanicSpeedMult;  // the last one double-times
    interval *= CollectMemoFx(g).marchMult;   // FLEXIBLE HOURS slows the march
    return interval;
}

void MarchStep(Game& g) {
    float margin = cfg::kPlayfieldMargin;
    float half = cfg::kInvaderW * GridScale(g) / 2.0f;

    if (g.descendPending) {
        for (auto& v : g.invaders)
            if (v.alive) v.pos.y += cfg::kMarchDropY;
        g.marchDir = -g.marchDir;
        g.descendPending = false;
        if (CurrentMod(g).reorg) {  // QUARTERLY REORG: shuffle survivor column positions
            int idxs[cfg::kGridCount], n = 0;
            for (int i = 0; i < cfg::kGridCount; i++)
                if (g.invaders[i].alive) idxs[n++] = i;
            for (int i = n - 1; i > 0; i--) {
                int j = g.setupRng.irange(0, i);
                float tx = g.invaders[idxs[i]].pos.x;
                g.invaders[idxs[i]].pos.x = g.invaders[idxs[j]].pos.x;
                g.invaders[idxs[j]].pos.x = tx;
            }
        }
    } else {
        float dx = cfg::kMarchStepX * (float)g.marchDir;
        float minX = 1e9f, maxX = -1e9f;
        for (const auto& v : g.invaders) {
            if (!v.alive) continue;
            minX = fminf(minX, v.pos.x);
            maxX = fmaxf(maxX, v.pos.x);
        }
        if (minX + dx - half < margin || maxX + dx + half > cfg::kCanvasW - margin) {
            g.descendPending = true;
        } else {
            for (auto& v : g.invaders)
                if (v.alive) v.pos.x += dx;
        }
    }

    g.marchFrame ^= 1;
    g.stepFlash = 0.14f;
    const Modifier& m = CurrentMod(g);
    for (auto& v : g.invaders)
        if (v.alive) v.squash = m.discoHue ? 0.45f : 0.18f;

    float pitch = m.scale < 1.0f ? 1.7f : 1.0f;
    if (g.aliveCount == 1) pitch *= cfg::kPanicPitch;  // the last one's heartbeat races
    PlaySfx(*g.audio, (Sfx)((int)Sfx::March0 + g.marchNoteIdx), pitch);
    g.marchNoteIdx = (g.marchNoteIdx + 1) % 4;

    // losing condition: they made it down
    for (const auto& v : g.invaders) {
        if (v.alive && v.pos.y + cfg::kInvaderH / 2 >= cfg::kLoseLineY) {
            g.overrun = true;
            g.gameOver = true;
            return;
        }
    }
}

void DropBomb(Game& g) {
    // bottom-most alive invader of a random occupied column fires
    int cols[cfg::kGridCols];
    int nCols = 0;
    for (int c = 0; c < cfg::kGridCols; c++) {
        for (int r = cfg::kGridRows - 1; r >= 0; r--) {
            if (g.invaders[r * cfg::kGridCols + c].alive) {
                cols[nCols++] = r * cfg::kGridCols + c;
                break;
            }
        }
    }
    if (nCols == 0) return;
    int shooterIdx = cols[g.rng.irange(0, nCols - 1)];
    const Invader& shooter = g.invaders[shooterIdx];

    const Modifier& m = CurrentMod(g);
    float bs = CollectMemoFx(g).bombSpeedMult;  // ESPRESSO BUDGET speeds their bombs up
    Shot s;
    s.pos = {shooter.pos.x, shooter.pos.y + cfg::kInvaderH / 2};
    s.fromPlayer = false;
    s.owner = shooterIdx;  // for the exit interview if this bomb lands the kill
    if (m.complimentBombs) {
        s.kind = ShotKind::Compliment;
        s.vel = {0, cfg::kBombSpeed * 0.8f * bs};
        s.label = content::kCompliments[g.rng.irange(0, content::kComplimentCount - 1)];
    } else {
        s.kind = ShotKind::Bomb;
        float vx = 0.0f;
        if (m.homingBombs) {  // MICROMANAGEMENT: aim toward the player
            float dir = g.player.pos.x - shooter.pos.x;
            vx = (dir > 0 ? 1.0f : -1.0f) * g.rng.range(30.0f, 60.0f);
        }
        s.vel = {vx, cfg::kBombSpeed * bs};
    }
    g.shots.push_back(s);
}

} // namespace

void UpdateInvaders(Game& g, float dt) {
    if (g.stepFlash > 0) g.stepFlash -= dt;
    for (auto& v : g.invaders) {
        if (v.squash > 0) v.squash = fmaxf(0.0f, v.squash - dt * 3.0f);
        if (v.hitFlash > 0) v.hitFlash -= dt;
    }
    g.panicTimer = (g.aliveCount == 1) ? g.panicTimer + dt : 0.0f;
    if (g.aliveCount == 0 || g.fx.freeze > 0) return;

    g.marchTimer += dt;
    float interval = StepInterval(g);
    if (g.marchTimer >= interval) {
        g.marchTimer -= interval;
        if (g.marchTimer > interval) g.marchTimer = 0;  // don't burst after long hitches
        MarchStep(g);
    }

    float rate = cfg::kBombBaseRate * (1.0f + cfg::kBombRateWave * (float)(g.wave.number - 1));
    if (g.aliveCount == 1) rate *= cfg::kPanicBombMult;  // panic fire
    rate *= CollectMemoFx(g).bombRateMult;   // FLEXIBLE HOURS trades march speed for bomb rate
    g.bombTimer += dt;
    float gap = 1.0f / rate;
    if (g.bombTimer >= gap) {
        g.bombTimer -= gap;
        DropBomb(g);
    }
}

void KillInvader(Game& g, int idx) {
    Invader& v = g.invaders[idx];
    if (!v.alive) return;
    v.alive = false;
    g.aliveCount--;

    int row = idx / cfg::kGridCols;
    int pts = cfg::kPtsRow[row] * (v.tough ? 2 : 1);
    Color c = cfg::kColRow[row];
    ComboKill(g, {v.pos.x, v.pos.y - cfg::kInvaderH}, pts, c);  // score + pop + streak callouts

    if (g.rng.chance(cfg::kFallChance)) {
        SpawnFaller(g, idx);
        PlaySfx(*g.audio, Sfx::Scream, g.rng.range(0.9f, 1.1f));
    } else {
        SpawnExplosion(g, v.pos, c, 16);
        SpawnDebris(g, v.pos, c, 5);
        // Pop pitch rises with the streak (composes with the TinyWave squeak override)
        float basePitch = GridScale(g) < 1.0f ? 1.7f : g.rng.range(0.9f, 1.1f);
        int ch = g.combo.chain < cfg::kComboChainPitchCap ? g.combo.chain : cfg::kComboChainPitchCap;
        PlaySfx(*g.audio, Sfx::Pop, basePitch * (1.0f + cfg::kComboPitchStep * (float)ch));
    }
    g.shake = fmaxf(g.shake, 2.0f);

    MaybeDropPickup(g, v.pos, idx);

    // commentary milestones
    if (g.aliveCount == cfg::kGridCount - 1 && g.wave.number == 1)
        PushBubble(g, -1, content::kFirstBlood, 3.0f);
    if (g.aliveCount == 10) PushBubble(g, -1, content::kTenLeft, 3.0f);
    if (g.aliveCount == 1) {
        for (int i = 0; i < cfg::kGridCount; i++)
            if (g.invaders[i].alive) { PushBubble(g, i, content::kOneLeft, 4.0f); break; }
    }
    // downing the last one earns a little extra fanfare
    if (g.aliveCount == 0 && !g.wave.bossWave) {
        SpawnConfetti(g, v.pos, 40);
        SpawnShockwave(g, v.pos, cfg::kColPlayer, 20.0f);
        g.shake = fmaxf(g.shake, 6.0f);
        g.hitStop = fmaxf(g.hitStop, cfg::kHitStopBossPhase);
        if (g.panicTimer >= cfg::kPanicAchSecs) TryAward(g, Ach::SeverancePackage);
    }
}

void SpawnFaller(Game& g, int idx) {
    const Invader& v = g.invaders[idx];
    FallingInvader f;
    f.pos = v.pos;
    f.vel = {g.rng.range(-cfg::kFallDriftX, cfg::kFallDriftX), cfg::kFallInitVy};
    f.spin = (g.rng.chance(0.5f) ? 1.0f : -1.0f) * g.rng.range(cfg::kFallSpinMin, cfg::kFallSpinMax);
    f.row = idx / cfg::kGridCols;
    f.seed = idx;
    f.id = g.fallerIdNext++;
    g.fallers.push_back(f);
    PushBubble(g, kBubbleAnchorFallerBase - (int)f.id,
               content::kFallerQuips[g.rng.irange(0, content::kFallerQuipCount - 1)],
               cfg::kFallQuipDur);
}

void UpdateFallers(Game& g, float dt) {
    for (auto& f : g.fallers) {
        f.vel.y += cfg::kFallGravity * dt;
        f.pos.x += f.vel.x * dt;
        f.pos.y += f.vel.y * dt;
        f.angle += f.spin * dt;
        if (g.rng.chance(0.3f)) SpawnTrail(g, f.pos, cfg::kColRow[f.row]);

        // chips bunkers like a bomb while passing through the bunker band
        if (f.pos.y + cfg::kInvaderH / 2 > cfg::kBunkerY - 4 &&
            f.pos.y - cfg::kInvaderH / 2 < cfg::kBunkerY + cfg::kBunkerRows * cfg::kBunkerCell + 4)
            if (CarveBunkers(g, f.pos, cfg::kCarveBomb))
                PlaySfx(*g.audio, Sfx::Crunch);

        Rectangle fr = {f.pos.x - cfg::kInvaderW / 2, f.pos.y - cfg::kInvaderH / 2,
                        cfg::kInvaderW, cfg::kInvaderH};
        if (g.player.alive && CheckCollisionRecs(fr, PlayerRect(g))) {
            HitPlayer(g, "falling coworker", kBubbleAnchorFallerBase - (int)f.id);
            SpawnExplosion(g, f.pos, cfg::kColRow[f.row], 16);
            f.pos.y = cfg::kCanvasH + 100;
        }
    }
    std::erase_if(g.fallers, [](const FallingInvader& f) { return f.pos.y > cfg::kCanvasH + 40; });
}

void UpdateUfo(Game& g, float dt) {
    Ufo& u = g.ufo;
    if (!u.active) {
        if (g.wave.bossWave || g.aliveCount == 0) return;
        u.spawnTimer -= dt;
        if (u.spawnTimer <= 0) {
            u.active = true;
            u.dir = g.rng.chance(0.5f) ? 1.0f : -1.0f;
            u.pos = {u.dir > 0 ? -cfg::kUfoW : cfg::kCanvasW + cfg::kUfoW, cfg::kUfoY};
            PlaySfx(*g.audio, Sfx::UfoWarble);
            if (g.rng.chance(0.6f))
                PushBubble(g, kBubbleAnchorUfo,
                           content::kUfoLines[g.rng.irange(0, content::kUfoLineCount - 1)], 3.5f);
        }
        return;
    }
    u.pos.x += u.dir * cfg::kUfoSpeed * dt;
    if (g.rng.chance(0.3f)) SpawnTrail(g, {u.pos.x - u.dir * cfg::kUfoW * 0.5f, u.pos.y}, cfg::kColUfo);
    if (u.pos.x < -cfg::kUfoW * 1.5f || u.pos.x > cfg::kCanvasW + cfg::kUfoW * 1.5f) {
        u.active = false;
        u.spawnTimer = g.rng.range(cfg::kUfoMinGap, cfg::kUfoMaxGap) * CollectMemoFx(g).ufoGapMult;
    }
}
