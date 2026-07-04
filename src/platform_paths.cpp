// The only file that knows what OS this is. Uses getenv, never windows.h
// (raylib.h and windows.h fight over Rectangle/CloseWindow/DrawText/PlaySound).
#include "highscores.h"
#include <cstdlib>

namespace fs = std::filesystem;

fs::path ConfigDir() {
    fs::path dir;
#if defined(_WIN32)
    if (const char* appdata = std::getenv("APPDATA"))
        dir = fs::path(appdata) / "SpaceInvaderPlus";
    else
        dir = fs::path(".") / "SpaceInvaderPlus";
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME"))
        dir = fs::path(home) / "Library" / "Application Support" / "SpaceInvaderPlus";
    else
        dir = fs::path(".") / "SpaceInvaderPlus";
#else
    if (const char* xdg = std::getenv("XDG_DATA_HOME"))
        dir = fs::path(xdg) / "space-invader-plus";
    else if (const char* home = std::getenv("HOME"))
        dir = fs::path(home) / ".local" / "share" / "space-invader-plus";
    else
        dir = fs::path(".") / "space-invader-plus";
#endif
    std::error_code ec;
    fs::create_directories(dir, ec);  // best effort; Save() copes with failure
    return dir;
}
