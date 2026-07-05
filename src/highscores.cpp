#include "highscores.h"
#include "content.h"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace {
const char* kHeader = "SIWHD v1";

void FillDefaults(HighScores& hs) {
    for (size_t i = 0; i < hs.table.size(); i++) {
        hs.table[i].score = content::kDefaultScores[i].score;
        hs.table[i].name.assign(content::kDefaultScores[i].name);
    }
}
} // namespace

void HighScores::LoadOrDefaults() {
    FillDefaults(*this);
    achMask = 0;

    std::ifstream in(ConfigDir() / "scores.txt");
    if (!in) return;

    std::string line;
    if (!std::getline(in, line) || line != kHeader) return;  // corrupt -> keep defaults
    if (!std::getline(in, line)) return;
    achMask = (uint32_t)std::strtoul(line.c_str(), nullptr, 10);

    std::array<ScoreEntry, 10> loaded{};
    size_t n = 0;
    while (n < loaded.size() && std::getline(in, line)) {
        std::istringstream ss(line);
        int score = 0;
        std::string name;
        if (!(ss >> score)) return;  // corrupt -> keep defaults
        std::getline(ss, name);
        // trim the single tab separator
        if (!name.empty() && (name[0] == '\t' || name[0] == ' ')) name.erase(0, 1);
        if (name.empty()) name = "???";
        loaded[n++] = {score, name};
    }
    if (n == loaded.size()) table = loaded;
}

void HighScores::Save() const {
    std::ofstream out(ConfigDir() / "scores.txt", std::ios::trunc);
    if (!out) return;  // read-only dir: the scores die with the session, gracefully
    out << kHeader << "\n" << achMask << "\n";
    for (const auto& e : table)
        out << e.score << "\t" << e.name << "\n";
}

bool HighScores::Qualifies(int score) const {
    return score > table.back().score;
}

int HighScores::Insert(int score, const std::string& name) {
    if (!Qualifies(score)) return -1;
    ScoreEntry e{score, name.empty() ? "???" : name};
    table.back() = e;
    std::stable_sort(table.begin(), table.end(),
                     [](const ScoreEntry& a, const ScoreEntry& b) { return a.score > b.score; });
    for (size_t i = 0; i < table.size(); i++)
        if (table[i].score == e.score && table[i].name == e.name) return (int)i;
    return -1;
}
