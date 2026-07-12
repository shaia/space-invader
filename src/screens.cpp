#include "content.h"
#include "game.h"
#include "highscores.h"
#include "render.h"
#include <cmath>

namespace {

void DrawCenteredGlow(const char* text, int y, int size, Color c) {
    int w = MeasureText(text, size);
    GlowText(text, cfg::kCanvasW / 2 - w / 2, y, size, c);
}

void DrawCentered(const char* text, int y, int size, Color c) {
    int w = MeasureText(text, size);
    DrawText(text, cfg::kCanvasW / 2 - w / 2, y, size, c);
}

void DimOverlay(float alpha) {
    DrawRectangle(0, 0, cfg::kCanvasW, cfg::kCanvasH, WithAlpha({6, 6, 14, 255}, alpha));
}

} // namespace

Screen UpdateDrawTitle(Game& g, const HighScores& hs, float timer) {
    ClearBackground(cfg::kColBg);
    DrawBackground(g, timer);

    // attract-mode demo grid marching behind the logo
    float t = timer;
    int frame = ((int)(t * 2.0f)) & 1;
    float sway = sinf(t * 0.6f) * 60.0f;
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 8; c++) {
            Vector2 at = {120.0f + c * 78.0f + sway, 110.0f + r * 54.0f + sinf(t + c * 0.4f) * 4.0f};
            DrawInvaderArt(at, 30, 20, r == 0 ? 0 : (r <= 1 ? 1 : 3), frame,
                           0, WithAlpha(cfg::kColRow[r], 0.16f), false, t, r * 8 + c);
        }
    }

    DrawCenteredGlow("SPACE INVADERS", 330, 64, cfg::kColAccent);
    DrawCenteredGlow("WE  HAVE  DEMANDS", 400, 30, cfg::kColPlayer);

    const char* flavor = content::kTitleFlavor[((int)(timer / 6.0f)) % content::kTitleFlavorCount];
    DrawCentered(flavor, 448, 18, cfg::kColHud);

    if (fmodf(timer, 1.0f) < 0.6f)
        DrawCenteredGlow("ENTER: NEGOTIATE    M: MANDATORY OVERTIME (DAILY)", 510, 20, RAYWHITE);

    // leaderboard — TAB flips between the endless records and today's daily ledger
    const auto& board = g.titleShowDaily ? hs.daily : hs.table;
    if (g.titleShowDaily)
        DrawCentered(TextFormat("OVERTIME LEDGER - %u", (unsigned)DailySeed()), 568, 18,
                     WithAlpha(cfg::kColAccent, 0.9f));
    else
        DrawCentered("COLLECTIVE BARGAINING RECORDS", 568, 18, WithAlpha(cfg::kColAccent, 0.9f));
    for (size_t i = 0; i < board.size(); i++) {
        const ScoreEntry& e = board[i];
        const char* line = TextFormat("%2d.  %-10s %8d", (int)i + 1, e.name.c_str(), e.score);
        int w = MeasureText(line, 18);
        DrawText(line, cfg::kCanvasW / 2 - w / 2, 598 + (int)i * 24, 18,
                 WithAlpha(cfg::kColHud, i % 2 ? 0.7f : 0.95f));
    }

    DrawCentered("ARROWS/AD MOVE   SPACE FIRE   P PAUSE   TAB SWITCH BOARD", 850, 16,
                 WithAlpha(cfg::kColHud, 0.6f));
    DrawCentered("(c) 1978, allegedly", 882, 15, WithAlpha(cfg::kColHud, 0.4f));

    if (IsKeyPressed(KEY_TAB)) { g.titleShowDaily = !g.titleShowDaily; PlaySfx(*g.audio, Sfx::Blip, 1.2f); }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        g.mode = RunMode::Endless;
        PlaySfx(*g.audio, Sfx::Blip);
        return Screen::Playing;
    }
    if (IsKeyPressed(KEY_M)) {
        g.mode = RunMode::Daily;
        g.dailySeed = DailySeed();
        PlaySfx(*g.audio, Sfx::Blip);
        return Screen::Playing;
    }
    if (IsKeyPressed(KEY_ESCAPE)) return Screen::Quit;
    return Screen::Title;
}

Screen UpdateDrawPaused(const Game& g) {
    DrawPlaying(g);  // frozen playfield underneath
    DimOverlay(0.72f);

    DrawCenteredGlow("PAUSED", 360, 48, cfg::kColAccent);
    const char* line = content::kPauseLines[((int)(GetTime() / 8.0)) % content::kPauseLineCount];
    DrawCentered(line, 430, 20, cfg::kColHud);
    DrawCentered("ESC/P RESUME      Q QUIT TO TITLE", 482, 18, WithAlpha(cfg::kColHud, 0.7f));

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) return Screen::Playing;
    if (IsKeyPressed(KEY_Q)) return Screen::Title;
    return Screen::Paused;
}

Screen UpdateDrawGameOver(Game& g, const HighScores& hs, float timer, float dt) {
    // keep toasts/bubbles animating over the frozen battlefield
    UpdateUiFx(g, dt);
    UpdateParticles(g, dt);
    DrawPlaying(g);
    DimOverlay(0.66f);

    DrawCenteredGlow("GAME OVER", 340, 56, cfg::kColHurt);
    const char* verdict = g.overrun ? content::kOverrun : content::kGameOver;
    Vector2 sz = MeasureTextEx(GetFontDefault(), verdict, 20, 1.0f);
    DrawTextEx(GetFontDefault(), verdict, {cfg::kCanvasW / 2.0f - sz.x / 2, 420}, 20, 1.0f,
               cfg::kColHud);

    DrawCenteredGlow(TextFormat("FINAL SCORE: %d", g.score), 490, 28, cfg::kColPlayer);
    if (hs.Qualifies(g.score, g.mode == RunMode::Daily))
        DrawCentered(g.mode == RunMode::Daily ? "A new overtime ledger record."
                                              : "A new collective bargaining record.",
                     530, 16, cfg::kColAccent);

    if (timer > 1.2f && fmodf(timer, 1.0f) < 0.6f)
        DrawCentered("PRESS ENTER", 590, 20, RAYWHITE);

    if (timer > 1.2f && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))) {
        PlaySfx(*g.audio, Sfx::Blip);
        return Screen::PerformanceReview;
    }
    return Screen::GameOver;
}

int GradeScore(const Game& g) {
    const RunStats& s = g.stats;
    float acc = s.shotsFired > 0 ? (float)s.shotsHit / (float)s.shotsFired : 0.0f;
    if (acc > 1.0f) acc = 1.0f;
    auto frac = [](int v, int full) { float f = (float)v / (float)full; return f > 1.0f ? 1.0f : f; };
    float total = 0.0f;
    total += acc * cfg::kGradeWAccuracy;
    total += frac(s.wavesCleared, cfg::kGradeWavesFull) * cfg::kGradeWWaves;
    total += frac(s.bossesDefeated, cfg::kGradeBossesFull) * cfg::kGradeWBosses;
    total += frac(s.maxChain, cfg::kGradeChainFull) * cfg::kGradeWChain;
    total += frac(s.grazes, cfg::kGradeGrazesFull) * cfg::kGradeWGrazes;
    total += (1.0f - frac(s.bunkersLost, cfg::kGradeBunkersFull)) * cfg::kGradeWBunkers;
    total += frac(s.powerups, cfg::kGradePowerupsFull) * cfg::kGradeWPowerups;
    return (int)(total + 0.5f);
}

int GradeLetterIndex(int gs) {
    if (gs >= cfg::kGradeS) return 5;
    if (gs >= cfg::kGradeA) return 4;
    if (gs >= cfg::kGradeB) return 3;
    if (gs >= cfg::kGradeC) return 2;
    if (gs >= cfg::kGradeD) return 1;
    return 0;
}

Screen UpdateDrawPerformanceReview(Game& g, HighScores& hs, float timer, float dt) {
    UpdateUiFx(g, dt);
    UpdateParticles(g, dt);
    DrawPlaying(g);
    DimOverlay(0.74f);

    DrawCenteredGlow("PERFORMANCE REVIEW", 110, 42, cfg::kColAccent);
    DrawCentered("Attendance was mandatory.", 158, 16, WithAlpha(cfg::kColHud, 0.75f));

    const RunStats& s = g.stats;
    int accPct = s.shotsFired > 0 ? (s.shotsHit * 100) / s.shotsFired : 0;
    if (accPct > 100) accPct = 100;

    const float y0 = 205.0f, step = 40.0f;
    auto row = [&](int i, const char* label, const char* value, const char* verdict) {
        float appear = 0.35f + i * 0.18f;
        if (timer < appear) return;
        float f = (timer - appear) / 0.3f;
        if (f > 1.0f) f = 1.0f;
        int y = (int)(y0 + i * step);
        DrawText(label, 120, y, 22, WithAlpha(cfg::kColHud, f));
        int vw = MeasureText(value, 22);
        GlowText(value, 450 - vw, y, 22, WithAlpha(cfg::kColPlayer, f));
        DrawText(verdict, 480, y + 4, 15, WithAlpha(cfg::kColHud, 0.65f * f));
    };
    row(0, "ACCURACY",        TextFormat("%d%%", accPct),         content::kReviewAccuracy[accPct >= 50]);
    row(1, "WAVES CLEARED",   TextFormat("%d", s.wavesCleared),   content::kReviewWaves[s.wavesCleared >= 6]);
    row(2, "BOSSES DEFEATED", TextFormat("%d", s.bossesDefeated), content::kReviewBosses[s.bossesDefeated >= 1]);
    row(3, "BEST STREAK",     TextFormat("x%d", s.maxChain),      content::kReviewChain[s.maxChain >= 10]);
    row(4, "HAZARD PAY",      TextFormat("%d", s.grazes),         content::kReviewGrazes[s.grazes >= 10]);
    row(5, "BUNKERS LOST",    TextFormat("%d", s.bunkersLost),    content::kReviewBunkers[s.bunkersLost <= 4]);
    row(6, "POWER-UPS",       TextFormat("%d", s.powerups),       content::kReviewPowerups[s.powerups >= 5]);
    row(7, "INCIDENTS",       TextFormat("%d", s.incidents),      content::kReviewIncidents[s.incidents <= 2]);

    int gi = GradeLetterIndex(GradeScore(g));
    float gradeAppear = 0.35f + 8 * 0.18f + 0.25f;
    bool revealed = timer > gradeAppear;
    if (revealed) {
        float f = (timer - gradeAppear) / 0.5f;
        if (f > 1.0f) f = 1.0f;
        Color gc = gi >= 4 ? cfg::kColPlayer : (gi >= 2 ? cfg::kColAccent : cfg::kColHurt);
        const char* letter = content::kGradeLetters[gi];
        int lw = MeasureText(letter, 88);
        GlowText(letter, cfg::kCanvasW / 2 - lw / 2, 585, 88, WithAlpha(gc, f));
        DrawCentered(content::kGradeLines[gi], 690, 20, WithAlpha(cfg::kColHud, f));
        if (gi > hs.bestGrade)
            DrawCentered("A new personal best. Suspicious.", 722, 15, WithAlpha(cfg::kColAccent, f));
    }

    if (revealed && fmodf(timer, 1.0f) < 0.6f)
        DrawCentered("PRESS ENTER", 850, 20, RAYWHITE);

    if (revealed && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))) {
        if (gi > hs.bestGrade) { hs.bestGrade = gi; hs.Save(); }
        PlaySfx(*g.audio, Sfx::Blip);
        return hs.Qualifies(g.score, g.mode == RunMode::Daily) ? Screen::HighScoreEntry
                                                               : Screen::Title;
    }
    return Screen::PerformanceReview;
}

Screen UpdateDrawHighScoreEntry(Game& g, HighScores& hs, ScoreEntryState& es, float timer) {
    ClearBackground(cfg::kColBg);
    DrawBackground(g, timer);

    DrawCenteredGlow("NEW HIGH SCORE", 260, 40, cfg::kColAccent);
    DrawCenteredGlow(TextFormat("%d", g.score), 320, 32, cfg::kColPlayer);
    DrawCentered("The union requires your initials for the paperwork.", 380, 18, cfg::kColHud);

    // arcade-style initials
    for (int i = 0; i < 3; i++) {
        float x = cfg::kCanvasW / 2.0f + (i - 1) * 70.0f;
        Color c = (i == es.cursor) ? cfg::kColAccent : cfg::kColHud;
        const char letter[2] = {es.initials[i], 0};
        int w = MeasureText(letter, 56);
        GlowText(letter, (int)x - w / 2, 440, 56, c);
        if (i == es.cursor && fmodf(timer, 0.8f) < 0.5f)
            DrawRectangle((int)x - 24, 504, 48, 4, cfg::kColAccent);
    }

    DrawCentered("UP/DOWN CHANGE   LEFT/RIGHT MOVE   ENTER SIGN", 560, 18,
                 WithAlpha(cfg::kColHud, 0.7f));

    auto bump = [&](int dir) {
        char& ch = es.initials[es.cursor];
        ch = (char)(ch + dir);
        if (ch < 'A') ch = 'Z';
        if (ch > 'Z') ch = 'A';
        PlaySfx(*g.audio, Sfx::Blip, 1.4f);
    };
    if (IsKeyPressed(KEY_UP)) bump(1);
    if (IsKeyPressed(KEY_DOWN)) bump(-1);
    if (IsKeyPressed(KEY_LEFT) && es.cursor > 0) { es.cursor--; PlaySfx(*g.audio, Sfx::Blip); }
    if (IsKeyPressed(KEY_RIGHT) && es.cursor < 2) { es.cursor++; PlaySfx(*g.audio, Sfx::Blip); }

    // direct typing also works
    for (int key = KEY_A; key <= KEY_Z; key++) {
        if (IsKeyPressed(key)) {
            es.initials[es.cursor] = (char)('A' + (key - KEY_A));
            if (es.cursor < 2) es.cursor++;
            PlaySfx(*g.audio, Sfx::Blip, 1.4f);
        }
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        bool daily = g.mode == RunMode::Daily;
        hs.Insert(g.score, es.initials, daily);
        if (daily) hs.dailyDate = (int)g.dailySeed;  // stamp today's ledger
        hs.Save();
        PlaySfx(*g.audio, Sfx::Ding);
        return Screen::Title;
    }
    return Screen::HighScoreEntry;
}
