// Entry point: window + RenderTexture setup, screen dispatch, the loop everything hangs off.
#include "game.h"
#include "highscores.h"
#include "postfx.h"
#include "render.h"
#include <algorithm>

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(cfg::kCanvasW, cfg::kCanvasH, "SPACE INVADERS: WE HAVE DEMANDS");
    SetExitKey(KEY_NULL);  // ESC is ours (pause/back)
    SetTargetFPS(60);

    // size the window to the monitor (upscale on big/high-DPI displays, shrink on small)
    int monitor = GetCurrentMonitor();
    int monH = GetMonitorHeight(monitor);
    if (monH > 0) {
        float scale = (float)monH * cfg::kWindowFit / (float)cfg::kCanvasH;
        if (scale < 0.5f) scale = 0.5f;
        if (scale > 2.5f) scale = 2.5f;
        SetWindowSize((int)(cfg::kCanvasW * scale), (int)(cfg::kCanvasH * scale));
        SetWindowPosition((GetMonitorWidth(monitor) - GetScreenWidth()) / 2,
                          (monH - GetScreenHeight()) / 2);
    }
    SetWindowMinSize(cfg::kCanvasW / 2, cfg::kCanvasH / 2);

    InitAudioDevice();
    AudioBank audio{};
    InitAudioBank(audio);

    // logical 800x950 canvas rasterized at 2x so it stays crisp at any window scale
    RenderTexture2D canvas = LoadRenderTexture(cfg::kCanvasW * cfg::kSupersample,
                                               cfg::kCanvasH * cfg::kSupersample);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);
    Camera2D ssCam{};
    ssCam.zoom = (float)cfg::kSupersample;

    // bloom + CRT post-processing (falls back to a plain blit if shaders won't compile)
    PostFx postfx{};
    InitPostFx(postfx, cfg::kCanvasW * cfg::kSupersample, cfg::kCanvasH * cfg::kSupersample);

    HighScores hs;
    hs.LoadOrDefaults();

    Game g{};
    g.audio = &audio;
    g.rng.s = (uint32_t)(GetTime() * 1e6) ^ 0x9E3779B9u;
    if (g.rng.s == 0) g.rng.s = 0x9E3779B9u;
    g.hiScore = hs.table[0].score;
    InitStarfield(g);  // so the title screen has a starfield before the first run

    Screen screen = Screen::Title;
    float screenTimer = 0.0f;
    ScoreEntryState entry{};

    while (!WindowShouldClose() && screen != Screen::Quit) {
        float dt = std::min(GetFrameTime(), cfg::kDtClamp);
        screenTimer += dt;
        TickMusic(audio);

        Screen next = screen;

        BeginTextureMode(canvas);
        BeginMode2D(ssCam);
        switch (screen) {
        case Screen::Title:
            next = UpdateDrawTitle(g, hs, screenTimer);
            break;
        case Screen::Playing:
            UpdatePlaying(g, dt);
            DrawPlaying(g);
            if (g.gameOver) {
                if (g.score % 10000 == 1978) TryAward(g, Ach::Y1978);
                if (g.mode == RunMode::Daily) TryAward(g, Ach::MandatoryOvertime);
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
        case Screen::PerformanceReview:
            next = UpdateDrawPerformanceReview(g, hs, screenTimer, dt);
            break;
        case Screen::HighScoreEntry:
            next = UpdateDrawHighScoreEntry(g, hs, entry, screenTimer);
            break;
        case Screen::Quit:
            break;
        }
        EndMode2D();
        EndTextureMode();

        if (next != screen) {
            if (next == Screen::Playing && screen != Screen::Paused) {
                ResetRun(g);
                g.hiScore = (g.mode == RunMode::Daily) ? hs.daily[0].score : hs.table[0].score;
                entry = ScoreEntryState{};
            }
            screen = next;
            screenTimer = 0.0f;
        }

        // blit the canvas to the window: aspect-preserving letterbox, shaken as drama requires
        float winW = (float)GetScreenWidth(), winH = (float)GetScreenHeight();
        float fit = std::min(winW / (float)cfg::kCanvasW, winH / (float)cfg::kCanvasH);
        float dstW = cfg::kCanvasW * fit, dstH = cfg::kCanvasH * fit;
        Vector2 shake = {0, 0};
        if (g.shake > 0.01f && (screen == Screen::Playing || screen == Screen::GameOver ||
                                screen == Screen::PerformanceReview)) {
            shake.x = g.rng.range(-g.shake, g.shake) * fit;
            shake.y = g.rng.range(-g.shake, g.shake) * fit;
        }
        Rectangle dst = {(winW - dstW) / 2 + shake.x, (winH - dstH) / 2 + shake.y, dstW, dstH};

        // bloom passes render into their own targets before we touch the backbuffer
        if (postfx.enabled) RenderBloom(postfx, canvas.texture);

        // speech bubbles draw AFTER the bright-pass: their white panels must not bloom
        if (screen == Screen::Playing || screen == Screen::Paused || screen == Screen::GameOver ||
            screen == Screen::PerformanceReview) {
            BeginTextureMode(canvas);
            BeginMode2D(ssCam);
            DrawSpeechBubbles(g);
            EndMode2D();
            EndTextureMode();
        }

        BeginDrawing();
        ClearBackground(BLACK);
        if (postfx.enabled) {
            DrawComposite(postfx, canvas.texture, dst, cfg::kScanlines);
        } else {
            DrawTexturePro(canvas.texture,
                           {0, 0, (float)canvas.texture.width, -(float)canvas.texture.height},
                           dst, {0, 0}, 0.0f, WHITE);
        }
        EndDrawing();
    }

    UnloadPostFx(postfx);
    UnloadRenderTexture(canvas);
    UnloadAudioBank(audio);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
