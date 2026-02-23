#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace core2048 {

struct ScoreEntry {
    int score{0};
    std::string playedAtUtc;
    std::string playerName;
};

class ScoreManager {
  public:
    static constexpr std::size_t kMaxEntries = 5;

    explicit ScoreManager(std::filesystem::path scoreFilePath);

    bool load();
    bool save() const;

    void addScore(int score, std::string playerName = "Oyuncu",
                  std::optional<std::string> playedAtUtc = std::nullopt);

    const std::vector<ScoreEntry> &topScores() const noexcept;
    int bestScore() const noexcept;
    const std::filesystem::path &scoreFilePath() const noexcept;

  private:
    static std::string currentUtcIso8601();
    void sortAndTrim();

    std::filesystem::path scoreFilePath_;
    std::vector<ScoreEntry> entries_;
};

} // namespace core2048
