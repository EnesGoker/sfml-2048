#pragma once

#include "core/Board.hpp"

#include <optional>

namespace app::ui {

enum class SceneId { Splash, HighScores, Playing, Win, GameOver };
enum class SceneCommand {
    None,
    StartGame,
    ShowHighScores,
    ShowSplash,
    RestartGame,
    ToggleSound,
    ContinueGame
};

struct SceneAction {
    SceneCommand command{SceneCommand::None};
    std::optional<core2048::Direction> direction;
};

} // namespace app::ui
