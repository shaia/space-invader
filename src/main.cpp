// Entry point: window + RenderTexture setup, screen dispatch, the loop everything hangs off.
#include "game.h"
#include "highscores.h"
#include "render.h"
#include <algorithm>

int main() {
    InitWindow(cfg::kCanvasW, cfg::kCanvasH, "SPACE INVADERS: WE HAVE DEMANDS");
    SetExitKey(KEY_NULL);  // ESC is ours (pause/back)
    SetTargetFPS(60);

    // fit small screens: shrink the window, keep the logical canvas fixed
    int monitor = GetCurrentMonitor();
    int monH = GetMonitorHeight(monitor);
    if (monH > 0 && monH - 100 < cfg::kCanvasH) {
        float scale = (float)(monH - 100) / (float)cfg::kCanvasH;
        SetWindowSize((int)(cfg::kCanvasW * scale), (int)(cfg::kCanvasH * scale));
        SetWindowPosition((GetMonitorWidth(monitor) - GetScreenWidth()) / 2, 40);
    }

    InitAudioDevice();
    AudioBank audio{};
    InitAudioBank(audio);

    RenderTexture2D canvas = LoadRenderTexture(cfg::kCanvasW, cfg::kCanvasH);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);

    HighScores hs;
    hs.LoadOrDefaults();

    Game g{};
    g.audio = &audio;
    g.rng.s = (uint32_t)(GetTime() * 1e6) ^ 0x9E3779B9u;
    if (g.rng.s == 0) g.rng.s = 0x9E3779B9u;
    g.hiScore = hs.table[0].score;

    Screen screen = Screen::Title;
    float screenTimer = 0.0f;
    ScoreEntryState entry{};

    while (!WindowShouldClose() && screen != Screen::Quit) {
        float dt = std::min(GetFrameTime(), cfg::kDtClamp);
        screenTimer += dt;
        TickMusic(audio);

        Screen next = screen;

        BeginTextureMode(canvas);
        switch (screen) {
        case Screen::Title:
            next = UpdateDrawTitle(g, hs, screenTimer);
            break;
        case Screen::Playing:
            UpdatePlaying(g, dt);
            DrawPlaying(g);
            if (g.gameOver) {
                if (g.score % 10000 == 1978) TryAward(g, Ach::Y1978);
                hs.achMask |= g.uifx.achAwarded;
                hs.Save();
                next = Screen::GameOver;
            } else if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) {
                PlaySfx(audio, Sfx::Blip, 0.8f);
                next = Screen::Paused;
            }
            break;
        case Screen::Paused:
            next = UpdateDrawPaused(g);
            break;
        case Screen::GameOver:
            next = UpdateDrawGameOver(g, hs, screenTimer, dt);
            break;
        case Screen::HighScoreEntry:
            next = UpdateDrawHighScoreEntry(g, hs, entry, screenTimer);
            break;
        case Screen::Quit:
            break;
        }
        DrawScanlines();
        EndTextureMode();

        if (next != screen) {
            if (next == Screen::Playing && screen != Screen::Paused) {
                ResetRun(g);
                entry = ScoreEntryState{};
            }
            screen = next;
            screenTimer = 0.0f;
        }

        // blit the fixed canvas to the real window, shaken as required by drama
        float winW = (float)GetScreenWidth(), winH = (float)GetScreenHeight();
        Vector2 shake = {0, 0};
        if (g.shake > 0.01f && (screen == Screen::Playing || screen == Screen::GameOver)) {
            shake.x = g.rng.range(-g.shake, g.shake);
            shake.y = g.rng.range(-g.shake, g.shake);
        }
        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(canvas.texture,
                       {0, 0, (float)cfg::kCanvasW, -(float)cfg::kCanvasH},
                       {shake.x, shake.y, winW, winH}, {0, 0}, 0.0f, WHITE);
        EndDrawing();
    }

    UnloadRenderTexture(canvas);
    UnloadAudioBank(audio);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
