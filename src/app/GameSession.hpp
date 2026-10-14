#pragma once

#include "core/Game.hpp"

#include <cstdint>
#include <string>

namespace app {

// Player and round bookkeeping, separate from screen state and gameplay rules.
class GameSession {
  public:
    GameSession();
    explicit GameSession(std::uint32_t seed);

    // A mutable session exclusively owns its round identity.
    GameSession(const GameSession &) = delete;
    GameSession &operator=(const GameSession &) = delete;
    GameSession(GameSession &&) = delete;
    GameSession &operator=(GameSession &&) = delete;

    void setPlayerName(std::string playerName);
    const std::string &playerName() const noexcept;
    const std::string &gameId() const noexcept;

    void resetGame();
    void resetGame(std::uint32_t seed);
    core2048::MoveResult applyMove(core2048::Direction direction);
    const core2048::Game &game() const noexcept;

    bool shouldRecordFinalScore() const noexcept;
    void markFinalScoreRecorded() noexcept;

  private:
    std::string playerName_{"Oyuncu"};
    core2048::Game game_;
    std::string gameId_;
    bool finalScoreRecorded_{false};
};

} // namespace app
