#pragma once

namespace core2048 {

struct GameState {
    int score{0};
    bool won{false};
    bool gameOver{false};
};

} // namespace core2048
