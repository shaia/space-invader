#include "game.h"
#include "render.h"
#include <cmath>

namespace {
// Classic bunker silhouette: arch with legs, in a 24x18 cell grid.
bool SilhouetteCell(int col, int row) {
    const int W = cfg::kBunkerCols, H = cfg::kBunkerRows;
    // top corners cut diagonally
    int cut = (H / 3) - row;                       // >0 near top
    if (cut > 0 && (col < cut * 2 || col >= W - cut * 2)) return false;
    // doorway notch at the bottom center
    int doorHalf = W / 5;
    int doorTop = H - H / 3;
    if (row >= doorTop && col >= W / 2 - doorHalf && col < W / 2 + doorHalf) {
        int depth = row - doorTop;
        int narrow = doorHalf - depth;             // arch shape
        if (col >= W / 2 - narrow && col < W / 2 + narrow) return false;
    }
    return true;
}

Vector2 CellCenter(const Bunker& b, int col, int row) {
    return {b.topLeft.x + (col + 0.5f) * cfg::kBunkerCell,
            b.topLeft.y + (row + 0.5f) * cfg::kBunkerCell};
}
} // namespace

void InitBunkers(Game& g) {
    float bw = cfg::kBunkerCols * cfg::kBunkerCell;
    float gap = (cfg::kCanvasW - cfg::kBunkerCount * bw) / (cfg::kBunkerCount + 1);
    for (int i = 0; i < cfg::kBunkerCount; i++) {
        g.bunkers[i].topLeft = {gap + i * (bw + gap), cfg::kBunkerY};
    }
    RestoreBunkers(g);
}

void RestoreBunkers(Game& g) {
    for (auto& b : g.bunkers) {
        b.aliveCells = 0;
        for (int r = 0; r < cfg::kBunkerRows; r++)
            for (int c = 0; c < cfg::kBunkerCols; c++) {
                bool solid = SilhouetteCell(c, r);
                b.cells[r * cfg::kBunkerCols + c] = solid ? 1 : 0;
                if (solid) b.aliveCells++;
            }
    }
}

bool AnyBunkerAlive(const Game& g) {
    for (const auto& b : g.bunkers)
        if (b.aliveCells > 0) return true;
    return false;
}

// Scan the shot's column band for the first solid cell in its travel direction.
bool ShotHitsBunker(const Game& g, const Shot& s, Vector2& hitPoint) {
    float bw = cfg::kBunkerCols * cfg::kBunkerCell;
    float bh = cfg::kBunkerRows * cfg::kBunkerCell;
    bool movingDown = s.vel.y > 0;
    for (const auto& b : g.bunkers) {
        if (b.aliveCells == 0) continue;
        Rectangle box = {b.topLeft.x, b.topLeft.y, bw, bh};
        if (s.pos.x < box.x || s.pos.x >= box.x + box.width) continue;
        if (s.pos.y < box.y - 10 || s.pos.y > box.y + box.height + 10) continue;
        int col = (int)((s.pos.x - box.x) / cfg::kBunkerCell);
        if (col < 0) col = 0;
        if (col >= cfg::kBunkerCols) col = cfg::kBunkerCols - 1;
        int rowAt = (int)((s.pos.y - box.y) / cfg::kBunkerCell);
        if (movingDown) {
            for (int r = 0; r <= rowAt && r < cfg::kBunkerRows; r++) {
                if (r < 0) continue;
                if (b.cells[r * cfg::kBunkerCols + col]) {
                    hitPoint = CellCenter(b, col, r);
                    return true;
                }
            }
        } else {
            for (int r = cfg::kBunkerRows - 1; r >= rowAt && r >= 0; r--) {
                if (r >= cfg::kBunkerRows) continue;
                if (b.cells[r * cfg::kBunkerCols + col]) {
                    hitPoint = CellCenter(b, col, r);
                    return true;
                }
            }
        }
    }
    return false;
}

bool CarveBunkers(Game& g, Vector2 hit, float radius) {
    bool carved = false;
    for (auto& b : g.bunkers) {
        if (b.aliveCells == 0) continue;
        for (int r = 0; r < cfg::kBunkerRows; r++) {
            for (int c = 0; c < cfg::kBunkerCols; c++) {
                uint8_t& cell = b.cells[r * cfg::kBunkerCols + c];
                if (!cell) continue;
                Vector2 cc = CellCenter(b, c, r);
                float dx = cc.x - hit.x, dy = cc.y - hit.y;
                // ragged edge: jitter the radius per cell
                float rr = radius * (0.75f + 0.5f * g.rng.uniform());
                if (dx * dx + dy * dy <= rr * rr) {
                    cell = 0;
                    b.aliveCells--;
                    carved = true;
                    if (g.rng.chance(0.3f))
                        SpawnDebris(g, cc, cfg::kColBunker, 1);
                }
            }
        }
    }
    return carved;
}

void DrawBunkers(const Game& g) {
    const Modifier& m = CurrentMod(g);
    Color tint = m.discoHue ? HueCycle(cfg::kColBunker, g.time) : cfg::kColBunker;
    for (const auto& b : g.bunkers) {
        if (b.aliveCells == 0) continue;
        float bw = cfg::kBunkerCols * cfg::kBunkerCell;
        float bh = cfg::kBunkerRows * cfg::kBunkerCell;
        DrawRectangleRec({b.topLeft.x - 4, b.topLeft.y - 4, bw + 8, bh + 8}, WithAlpha(tint, 0.08f));
        for (int r = 0; r < cfg::kBunkerRows; r++)
            for (int c = 0; c < cfg::kBunkerCols; c++)
                if (b.cells[r * cfg::kBunkerCols + c])
                    DrawRectangleRec({b.topLeft.x + c * cfg::kBunkerCell,
                                      b.topLeft.y + r * cfg::kBunkerCell,
                                      cfg::kBunkerCell + 0.5f, cfg::kBunkerCell + 0.5f}, tint);
    }
}
