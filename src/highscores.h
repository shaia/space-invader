// Persistent top-10 table + achievement mask. Plain text file in the platform config dir.
#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

std::filesystem::path ConfigDir();  // platform_paths.cpp; created on first call
uint32_t DailySeed();               // today's yyyymmdd, the daily-challenge seed

struct ScoreEntry {
    int score = 0;
    std::string name = "???";
};

struct HighScores {
    std::array<ScoreEntry, 10> table{};   // endless-mode records
    std::array<ScoreEntry, 10> daily{};   // today's daily-challenge records
    uint32_t achMask = 0;   // all achievements ever earned
    int bestGrade = 0;      // best Performance Review grade, 0=F .. 5=S
    int dailyDate = 0;      // yyyymmdd the daily table belongs to (0 = none)

    void LoadOrDefaults();
    void Save() const;
    // `daily` selects the daily table instead of the endless table
    bool Qualifies(int score, bool daily = false) const;
    int Insert(int score, const std::string& name, bool daily = false);  // rank 0-9, or -1
};
