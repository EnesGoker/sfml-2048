#include "core/Game.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>

namespace {

using core2048::Direction;
using core2048::Game;

constexpr std::array<Direction, 10> kGoldenMoveSequence = {
    Direction::Up,   Direction::Left, Direction::Down,  Direction::Right, Direction::Up,
    Direction::Left, Direction::Down, Direction::Right, Direction::Up,    Direction::Left,
};

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
        std::array<int, 4>{4, 2, 8, 0},
        std::array<int, 4>{8, 0, 2, 0},
        std::array<int, 4>{4, 0, 0, 0},
        std::array<int, 4>{0, 0, 0, 0},
    };

    const std::array<bool, kGoldenMoveSequence.size()> expectedMovedFlags = {
        true, true, true, true, true, true, true, true, true, true,
    };

    REQUIRE(gridA == expectedGrid);
    REQUIRE(scoreA == 32);
    REQUIRE(movedA == expectedMovedFlags);
}
