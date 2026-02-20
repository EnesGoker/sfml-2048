#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <random>

namespace core2048 {

enum class Direction { Up, Down, Left, Right };

struct SpawnedTile {
    int row;
    int col;
    int value;
};

struct MoveResult {
    bool moved{false};
    int scoreDelta{0};
    std::optional<SpawnedTile> spawnedTile;
};

class Game {
  public:
    static constexpr int kGridSize = 4;
    using Grid = std::array<std::array<int, kGridSize>, kGridSize>;

    Game();
    explicit Game(std::uint32_t seed);

    void reset();
    void reset(std::uint32_t seed);
    void loadState(const Grid &grid, int score = 0);

    MoveResult applyMove(Direction dir, bool spawnOnMove = true);

    const Grid &getGrid() const noexcept;
    int getScore() const noexcept;
    bool isGameOver() const;

  private:
    struct LineResult {
        std::array<int, kGridSize> values{};
        bool moved{false};
        int scoreDelta{0};
    };

    static LineResult slideAndMergeLine(const std::array<int, kGridSize> &line);
    std::optional<SpawnedTile> spawnTile();

    Grid grid_{};
    int score_{0};
    std::mt19937 rng_;
    std::uint32_t seed_{0};
};

} // namespace core2048
