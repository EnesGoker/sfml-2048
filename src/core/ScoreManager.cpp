#include "core/ScoreManager.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

namespace core2048 {

namespace {

using Json = nlohmann::json;

std::optional<ScoreEntry> parseEntry(const Json &item) {
    if (!item.is_object()) {
        return std::nullopt;
    }

    if (!item.contains("score") || !item["score"].is_number_integer()) {
        return std::nullopt;
    }

    if (!item.contains("played_at") || !item["played_at"].is_string()) {
        return std::nullopt;
    }

    ScoreEntry entry;
    entry.score = item["score"].get<int>();
    entry.playedAtUtc = item["played_at"].get<std::string>();
    entry.playerName = "Oyuncu";
    if (item.contains("player_name") && item["player_name"].is_string()) {
        const auto candidate = item["player_name"].get<std::string>();
        if (!candidate.empty()) {
            entry.playerName = candidate;
        }
    }

    return entry;
}

Json toJson(const ScoreEntry &entry) {
    Json item;
    item["score"] = entry.score;
    item["played_at"] = entry.playedAtUtc;
    item["player_name"] = entry.playerName;
    return item;
}

} // namespace

ScoreManager::ScoreManager(std::filesystem::path scoreFilePath)
    : scoreFilePath_(std::move(scoreFilePath)) {
}

bool ScoreManager::load() {
    entries_.clear();

    std::error_code ec;
    if (!std::filesystem::exists(scoreFilePath_, ec)) {
        return !ec;
    }
    if (ec) {
        return false;
    }

    std::ifstream in(scoreFilePath_);
    if (!in.is_open()) {
        return false;
    }

    Json root;
    try {
        in >> root;
    } catch (const Json::parse_error &) {
        return false;
    }

    if (!root.is_object() || !root.contains("scores") || !root["scores"].is_array()) {
        return false;
    }

    for (const auto &item : root["scores"]) {
        const auto parsed = parseEntry(item);
        if (parsed.has_value()) {
            entries_.push_back(*parsed);
        }
    }

    sortAndTrim();
    return true;
}

bool ScoreManager::save() const {
    const auto parent = scoreFilePath_.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            return false;
        }
    }

    Json root;
    root["scores"] = Json::array();
    for (const auto &entry : entries_) {
        root["scores"].push_back(toJson(entry));
    }

    std::ofstream out(scoreFilePath_);
    if (!out.is_open()) {
        return false;
    }

    out << root.dump(2) << '\n';
    return static_cast<bool>(out);
}

void ScoreManager::addScore(const int score, std::string playerName,
                            std::optional<std::string> playedAtUtc) {
    ScoreEntry entry;
    entry.score = score;
    if (playerName.empty()) {
        playerName = "Oyuncu";
    }
    entry.playerName = std::move(playerName);
    entry.playedAtUtc = playedAtUtc.value_or(currentUtcIso8601());
    entries_.push_back(std::move(entry));
    sortAndTrim();
}

const std::vector<ScoreEntry> &ScoreManager::topScores() const noexcept {
    return entries_;
}

int ScoreManager::bestScore() const noexcept {
    if (entries_.empty()) {
        return 0;
    }
    return entries_.front().score;
}

const std::filesystem::path &ScoreManager::scoreFilePath() const noexcept {
    return scoreFilePath_;
}

std::string ScoreManager::currentUtcIso8601() {
    const std::time_t now = std::time(nullptr);

    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif

    std::ostringstream out;
    out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

void ScoreManager::sortAndTrim() {
    std::stable_sort(entries_.begin(), entries_.end(),
                     [](const ScoreEntry &lhs, const ScoreEntry &rhs) {
                         if (lhs.score != rhs.score) {
                             return lhs.score > rhs.score;
                         }
                         return lhs.playedAtUtc > rhs.playedAtUtc;
                     });

    if (entries_.size() > kMaxEntries) {
        entries_.resize(kMaxEntries);
    }
}

} // namespace core2048
