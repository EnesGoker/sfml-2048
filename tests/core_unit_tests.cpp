#include "core/Game.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <random>

namespace {

using core2048::Direction;
using core2048::Game;

constexpr std::array<Direction, 10> kGoldenMoveSequence = {
    Direction::Up,   Direction::Left, Direction::Down,  Direction::Right, Direction::Up,
    Direction::Left, Direction::Down, Direction::Right, Direction::Up,    Direction::Left,
};

constexpr std::array<Direction, 4> kAllDirections = {
    Direction::Up,
    Direction::Down,
    Direction::Left,
    Direction::Right,
};

bool isPowerOfTwoOrZero(int value) {
    if (value == 0) {
        return true;
    }

    return value > 0 && (value & (value - 1)) == 0;
}

int sumGrid(const Game::Grid &grid) {
    int total = 0;
    for (const auto &row : grid) {
        for (int value : row) {
            total += value;
        }
    }
    return total;
}

Game::Grid mirrorHorizontal(const Game::Grid &grid) {
    Game::Grid mirrored{};

    for (int r = 0; r < Game::kGridSize; ++r) {
        for (int c = 0; c < Game::kGridSize; ++c) {
            mirrored[r][c] = grid[r][Game::kGridSize - 1 - c];
        }
    }

    return mirrored;
}

Game::Grid randomValidGrid(std::mt19937 &rng) {
    std::uniform_int_distribution<int> expDist(0, 11);
    Game::Grid grid{};

    for (int r = 0; r < Game::kGridSize; ++r) {
        for (int c = 0; c < Game::kGridSize; ++c) {
            const int exp = expDist(rng);
            grid[r][c] = (exp == 0) ? 0 : (1 << (exp - 1));
        }
    }

    return grid;
}

} // namespace

TEST_CASE("2,2,2,2 left merges to 4,4,0,0 with correct score", "[merge]") {
    Game game(0);
    const Game::Grid grid = {
        std::array<int, 4>{2, 2, 2, 2},
        std::array<int, 4>{0, 0, 0, 0},
        std::array<int, 4>{0, 0, 0, 0},
        std::array<int, 4>{0, 0, 0, 0},
    };

    game.loadState(grid, 0);
    const auto result = game.applyMove(Direction::Left, false);

    REQUIRE(result.moved);
    REQUIRE(result.scoreDelta == 8);
    REQUIRE_FALSE(result.spawnedTile.has_value());

    const Game::Grid expected = {
        std::array<int, 4>{4, 4, 0, 0},
        std::array<int, 4>{0, 0, 0, 0},
        std::array<int, 4>{0, 0, 0, 0},
        std::array<int, 4>{0, 0, 0, 0},
    };

    REQUIRE(game.getGrid() == expected);
    REQUIRE(game.getScore() == 8);
}

TEST_CASE("double merges happen once per tile", "[merge]") {
    Game game(0);
    const Game::Grid grid = {
        std::array<int, 4>{2, 2, 4, 4},
        std::array<int, 4>{0, 0, 0, 0},
        std::array<int, 4>{0, 0, 0, 0},
        std::array<int, 4>{0, 0, 0, 0},
    };

    game.loadState(grid, 0);
    const auto result = game.applyMove(Direction::Left, false);

    REQUIRE(result.moved);
    REQUIRE(result.scoreDelta == 12);

    const Game::Grid expected = {
        std::array<int, 4>{4, 8, 0, 0},
        std::array<int, 4>{0, 0, 0, 0},
        std::array<int, 4>{0, 0, 0, 0},
        std::array<int, 4>{0, 0, 0, 0},
    };

    REQUIRE(game.getGrid() == expected);
    REQUIRE(game.getScore() == 12);
}

TEST_CASE("no-op move does not mutate grid or spawn", "[move]") {
    Game game(0);
    const Game::Grid grid = {
        std::array<int, 4>{2, 4, 8, 16},
        std::array<int, 4>{32, 64, 128, 256},
        std::array<int, 4>{512, 1024, 2, 4},
        std::array<int, 4>{8, 16, 32, 64},
    };

    game.loadState(grid, 99);
    const auto result = game.applyMove(Direction::Left);

    REQUIRE_FALSE(result.moved);
    REQUIRE(result.scoreDelta == 0);
    REQUIRE_FALSE(result.spawnedTile.has_value());
    REQUIRE(game.getGrid() == grid);
    REQUIRE(game.getScore() == 99);
}

TEST_CASE("game-over detection works for dead and alive boards", "[gameover]") {
    Game game(0);

    const Game::Grid deadGrid = {
        std::array<int, 4>{2, 4, 8, 16},
        std::array<int, 4>{32, 64, 128, 256},
        std::array<int, 4>{512, 1024, 2, 4},
        std::array<int, 4>{8, 16, 32, 64},
    };

    game.loadState(deadGrid, 0);
    REQUIRE(game.isGameOver());

    const Game::Grid aliveGrid = {
        std::array<int, 4>{2, 4, 8, 16},
        std::array<int, 4>{32, 64, 128, 256},
        std::array<int, 4>{512, 1024, 2, 4},
        std::array<int, 4>{8, 16, 32, 32},
    };

    game.loadState(aliveGrid, 0);
    REQUIRE_FALSE(game.isGameOver());
}

TEST_CASE("score accumulation is correct across moves", "[score]") {
    Game game(0);
    const Game::Grid grid = {
        std::array<int, 4>{2, 2, 0, 0},
        std::array<int, 4>{4, 4, 0, 0},
        std::array<int, 4>{0, 0, 0, 0},
        std::array<int, 4>{0, 0, 0, 0},
    };

    game.loadState(grid, 0);

    const auto first = game.applyMove(Direction::Left, false);
    REQUIRE(first.moved);
    REQUIRE(first.scoreDelta == 12);
    REQUIRE(game.getScore() == 12);

    const auto second = game.applyMove(Direction::Left, false);
    REQUIRE_FALSE(second.moved);
    REQUIRE(second.scoreDelta == 0);
    REQUIRE(game.getScore() == 12);
}

TEST_CASE("seed=1234 with 10 moves matches expected snapshot", "[golden]") {
    const auto playSequence = [](const std::uint32_t seed) {
        Game game(seed);

        std::array<bool, kGoldenMoveSequence.size()> movedFlags{};
        for (size_t i = 0; i < kGoldenMoveSequence.size(); ++i) {
            const auto result = game.applyMove(kGoldenMoveSequence[i]);
            movedFlags[i] = result.moved;
        }

        return std::tuple{game.getGrid(), game.getScore(), movedFlags};
    };

    const auto [gridA, scoreA, movedA] = playSequence(1234);
    const auto [gridB, scoreB, movedB] = playSequence(1234);

    REQUIRE(gridA == gridB);
    REQUIRE(scoreA == scoreB);
    REQUIRE(movedA == movedB);

    const Game::Grid expectedGrid = {
        std::array<int, 4>{2, 16, 0, 0},
        std::array<int, 4>{2, 0, 0, 0},
        std::array<int, 4>{4, 0, 2, 0},
        std::array<int, 4>{0, 0, 0, 0},
    };

    const std::array<bool, kGoldenMoveSequence.size()> expectedMovedFlags = {
        true, true, true, true, true, true, true, true, true, true,
    };

    REQUIRE(gridA == expectedGrid);
    REQUIRE(scoreA == 48);
    REQUIRE(movedA == expectedMovedFlags);
}

TEST_CASE("random valid boards preserve invariants after move without spawn", "[property]") {
    std::mt19937 rng(20260220);

    for (int iter = 0; iter < 300; ++iter) {
        Game game(0);
        game.loadState(randomValidGrid(rng), 0);

        for (const Direction direction : kAllDirections) {
            const auto before = game.getGrid();
            const int beforeSum = sumGrid(before);

            const auto result = game.applyMove(direction, false);
            const auto &after = game.getGrid();

            REQUIRE(sumGrid(after) == beforeSum);
            REQUIRE(result.scoreDelta >= 0);

            if (!result.moved) {
                REQUIRE(after == before);
            }

            for (const auto &row : after) {
                for (const int value : row) {
                    REQUIRE(isPowerOfTwoOrZero(value));
                }
            }
        }
    }
}

TEST_CASE("left and right moves are mirror symmetric without spawn", "[property]") {
    std::mt19937 rng(424242);

    for (int iter = 0; iter < 200; ++iter) {
        const Game::Grid grid = randomValidGrid(rng);
        const Game::Grid mirroredGrid = mirrorHorizontal(grid);

        Game leftGame(0);
        leftGame.loadState(grid, 0);
        const auto leftResult = leftGame.applyMove(Direction::Left, false);

        Game rightGame(0);
        rightGame.loadState(mirroredGrid, 0);
        const auto rightResult = rightGame.applyMove(Direction::Right, false);

        REQUIRE(leftResult.moved == rightResult.moved);
        REQUIRE(leftResult.scoreDelta == rightResult.scoreDelta);
        REQUIRE(leftGame.getGrid() == mirrorHorizontal(rightGame.getGrid()));
    }
}
