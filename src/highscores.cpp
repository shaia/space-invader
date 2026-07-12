#include "highscores.h"
#include "content.h"
#include <algorithm>
#include <ctime>
#include <fstream>
#include <sstream>

uint32_t DailySeed() {
    time_t t = time(nullptr);
    tm lt{};
#if defined(_WIN32)
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    return (uint32_t)((lt.tm_year + 1900) * 10000 + (lt.tm_mon + 1) * 100 + lt.tm_mday);
}

namespace {
const char* kHeaderV1 = "SIWHD v1";
const char* kHeaderV2 = "SIWHD v2";

void FillDefaults(HighScores& hs) {
    for (size_t i = 0; i < hs.table.size(); i++) {
        hs.table[i].score = content::kDefaultScores[i].score;
        hs.table[i].name.assign(content::kDefaultScores[i].name);
    }
    for (auto& e : hs.daily) { e.score = 0; e.name = "---"; }  // no entries yet today
}

// read up to 10 "score \t name" lines into `into`; returns count parsed
size_t ReadTable(std::ifstream& in, std::array<ScoreEntry, 10>& into) {
    std::string line;
    size_t n = 0;
    while (n < into.size() && std::getline(in, line)) {
        std::istringstream ss(line);
        int score = 0;
        std::string name;
        if (!(ss >> score)) break;
        std::getline(ss, name);
        if (!name.empty() && (name[0] == '\t' || name[0] == ' ')) name.erase(0, 1);
        if (name.empty()) name = "???";
        into[n++] = {score, name};
    }
    return n;
}
} // namespace

void HighScores::LoadOrDefaults() {
    FillDefaults(*this);
    achMask = 0;
    bestGrade = 0;
    dailyDate = 0;

    std::ifstream in(ConfigDir() / "scores.txt");
    if (!in) return;

    std::string line;
    if (!std::getline(in, line)) return;
    bool v2 = (line == kHeaderV2);
    if (!v2 && line != kHeaderV1) return;  // unknown header -> keep defaults

    if (!std::getline(in, line)) return;
    achMask = (uint32_t)std::strtoul(line.c_str(), nullptr, 10);

    if (v2) {
        if (!std::getline(in, line)) return;
        bestGrade = std::atoi(line.c_str());
        if (!std::getline(in, line)) return;
        dailyDate = std::atoi(line.c_str());
    }

    std::array<ScoreEntry, 10> mainT{};
    if (ReadTable(in, mainT) == mainT.size()) table = mainT;

    if (v2) {
        std::array<ScoreEntry, 10> dailyT{};
        // only adopt the stored daily ledger if it belongs to today; else it stays
        // the empty default and a new day's ledger starts fresh
        if (ReadTable(in, dailyT) == dailyT.size() && dailyDate == (int)DailySeed())
            daily = dailyT;
        else
            dailyDate = 0;
    }
}

void HighScores::Save() const {
    std::ofstream out(ConfigDir() / "scores.txt", std::ios::trunc);
    if (!out) return;  // read-only dir: the scores die with the session, gracefully
    out << kHeaderV2 << "\n" << achMask << "\n" << bestGrade << "\n" << dailyDate << "\n";
    for (const auto& e : table)
        out << e.score << "\t" << e.name << "\n";
    for (const auto& e : daily)
        out << e.score << "\t" << e.name << "\n";
}

bool HighScores::Qualifies(int score, bool useDaily) const {
    const auto& t = useDaily ? daily : table;
    return score > t.back().score;
}

int HighScores::Insert(int score, const std::string& name, bool useDaily) {
    if (!Qualifies(score, useDaily)) return -1;
    auto& t = useDaily ? daily : table;
    ScoreEntry e{score, name.empty() ? "???" : name};
    t.back() = e;
    std::stable_sort(t.begin(), t.end(),
                     [](const ScoreEntry& a, const ScoreEntry& b) { return a.score > b.score; });
    for (size_t i = 0; i < t.size(); i++)
        if (t[i].score == e.score && t[i].name == e.name) return (int)i;
    return -1;
}
