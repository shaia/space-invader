// Neon-glow drawing helpers and vector entity art.
#pragma once
#include "raylib.h"

struct Game;

Color WithAlpha(Color c, float a);
Color HueCycle(Color c, float time);   // Disco Inferno tint

void GlowRect(Rectangle r, Color c);
void GlowRectRot(Rectangle r, float rotationDeg, Color c);  // rotates around center
void GlowCircle(Vector2 center, float radius, Color c);
void GlowLine(Vector2 a, Vector2 b, float thick, Color c);
void GlowText(const char* text, int x, int y, int size, Color c);

// Entity art (center-based). frame is 0/1 march parity; squash scales comedy.
void DrawInvaderArt(Vector2 c, float w, float h, int row, int frame, float squash,
                    Color tint, bool wobbly, float time, int seed);
void DrawPlayerArt(Vector2 c, float w, float h, Color tint, float squash);
void DrawUfoArt(Vector2 c, float w, float h, Color tint, float time);
void DrawShotArt(const Game& g, const struct Shot& s);

void DrawScanlines();
