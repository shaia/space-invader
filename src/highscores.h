// Persistent top-10 table + achievement mask. Plain text file in the platform config dir.
#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

std::filesystem::path ConfigDir();  // platform_paths.cpp; created on first call

struct ScoreEntry {
    int score = 0;
    std::string name = "???";
};

struct HighScores {
    std::array<ScoreEntry, 10> table{};
    uint32_t achMask = 0;   // all achievements ever earned

    void LoadOrDefaults();
    void Save() const;
    bool Qualifies(int score) const;
    int Insert(int score, const std::string& name);  // returns rank 0-9, or -1
};
