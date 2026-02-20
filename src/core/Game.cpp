#include "core/Game.hpp"

#include <algorithm>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

namespace core2048 {

namespace {

std::uint32_t nextBounded(std::mt19937 &rng, const std::uint32_t upperExclusive) {
    if (upperExclusive == 0U) {
        return 0U;
    }

    constexpr std::uint64_t range = static_cast<std::uint64_t>(std::mt19937::max()) + 1ULL;
    const std::uint64_t bucketSize = range / upperExclusive;
    const std::uint64_t rejectionLimit = bucketSize * upperExclusive;

    std::uint64_t value = 0;
    do {
        value = static_cast<std::uint64_t>(rng());
    } while (value >= rejectionLimit);

    return static_cast<std::uint32_t>(value / bucketSize);
}

} // namespace

Game::Game() : Game(static_cast<std::uint32_t>(std::random_device{}())) {
}

Game::Game(std::uint32_t seed) {
    reset(seed);
}

void Game::reset() {
    reset(static_cast<std::uint32_t>(std::random_device{}()));
}

void Game::reset(std::uint32_t seed) {
    seed_ = seed;
    rng_.seed(seed_);

    for (auto &row : grid_) {
        row.fill(0);
    }

    score_ = 0;
    spawnTile();
    spawnTile();
}

void Game::loadState(const Grid &grid, int score) {
    grid_ = grid;
    score_ = score;
}

const Game::Grid &Game::getGrid() const noexcept {
    return grid_;
}

int Game::getScore() const noexcept {
    return score_;
}

bool Game::isGameOver() const {
    for (int r = 0; r < kGridSize; ++r) {
        for (int c = 0; c < kGridSize; ++c) {
            if (grid_[r][c] == 0) {
                return false;
            }
        }
    }

    for (int r = 0; r < kGridSize; ++r) {
        for (int c = 0; c < kGridSize; ++c) {
            if (c < (kGridSize - 1) && grid_[r][c] == grid_[r][c + 1]) {
                return false;
            }

            if (r < (kGridSize - 1) && grid_[r][c] == grid_[r + 1][c]) {
                return false;
            }
        }
    }

    return true;
}

Game::LineResult Game::slideAndMergeLine(const std::array<int, kGridSize> &line) {
    std::vector<int> compact;
    compact.reserve(kGridSize);

    for (int value : line) {
        if (value != 0) {
            compact.push_back(value);
        }
    }

    LineResult result;

    int writeIndex = 0;
    for (size_t i = 0; i < compact.size(); ++i) {
        if (i + 1 < compact.size() && compact[i] == compact[i + 1]) {
            const int mergedValue = compact[i] * 2;
            result.values[writeIndex++] = mergedValue;
            result.scoreDelta += mergedValue;
            ++i;
            continue;
        }

        result.values[writeIndex++] = compact[i];
    }

    result.moved = result.values != line;
    return result;
}

std::optional<SpawnedTile> Game::spawnTile() {
    std::vector<std::pair<int, int>> emptyCells;
    emptyCells.reserve(kGridSize * kGridSize);

    for (int r = 0; r < kGridSize; ++r) {
        for (int c = 0; c < kGridSize; ++c) {
            if (grid_[r][c] == 0) {
                emptyCells.emplace_back(r, c);
            }
        }
    }

    if (emptyCells.empty()) {
        return std::nullopt;
    }

    const size_t cellIndex =
        static_cast<size_t>(nextBounded(rng_, static_cast<std::uint32_t>(emptyCells.size())));
    const auto [row, col] = emptyCells[cellIndex];

    const int tileValue = (nextBounded(rng_, 10U) == 0U) ? 4 : 2;

    grid_[row][col] = tileValue;
    return SpawnedTile{row, col, tileValue};
}

MoveResult Game::applyMove(Direction dir, const bool spawnOnMove) {
    bool moved = false;
    int scoreDelta = 0;

    const auto applyToRow = [&](int row, bool reverse) {
        std::array<int, kGridSize> line{};

        for (int c = 0; c < kGridSize; ++c) {
            line[c] = reverse ? grid_[row][kGridSize - 1 - c] : grid_[row][c];
        }

        const LineResult lineResult = slideAndMergeLine(line);

        if (lineResult.moved) {
            moved = true;
        }

        scoreDelta += lineResult.scoreDelta;

        for (int c = 0; c < kGridSize; ++c) {
            if (reverse) {
                grid_[row][kGridSize - 1 - c] = lineResult.values[c];
            } else {
                grid_[row][c] = lineResult.values[c];
            }
        }
    };

    const auto applyToCol = [&](int col, bool reverse) {
        std::array<int, kGridSize> line{};

        for (int r = 0; r < kGridSize; ++r) {
            line[r] = reverse ? grid_[kGridSize - 1 - r][col] : grid_[r][col];
        }

        const LineResult lineResult = slideAndMergeLine(line);

        if (lineResult.moved) {
            moved = true;
        }

        scoreDelta += lineResult.scoreDelta;

        for (int r = 0; r < kGridSize; ++r) {
            if (reverse) {
                grid_[kGridSize - 1 - r][col] = lineResult.values[r];
            } else {
                grid_[r][col] = lineResult.values[r];
            }
        }
    };

    switch (dir) {
    case Direction::Left:
        for (int r = 0; r < kGridSize; ++r) {
            applyToRow(r, false);
        }
        break;
    case Direction::Right:
        for (int r = 0; r < kGridSize; ++r) {
            applyToRow(r, true);
        }
        break;
    case Direction::Up:
        for (int c = 0; c < kGridSize; ++c) {
            applyToCol(c, false);
        }
        break;
    case Direction::Down:
        for (int c = 0; c < kGridSize; ++c) {
            applyToCol(c, true);
        }
        break;
    }

    if (!moved) {
        return MoveResult{};
    }

    score_ += scoreDelta;

    MoveResult result;
    result.moved = true;
    result.scoreDelta = scoreDelta;
    if (spawnOnMove) {
        result.spawnedTile = spawnTile();
    }
    return result;
}

} // namespace core2048
