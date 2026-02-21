#include "app/App.hpp"
#include "app/AssetResolver.hpp"
#include "core/Game.hpp"

#include <SFML/Graphics.hpp>

#include <chrono>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <utility>

namespace {

constexpr int kCellSize = 100;
constexpr int kGridSize = core2048::Game::kGridSize;
constexpr int kPadding = 10;
constexpr int kTopPanelHeight = 80;
constexpr float kSpawnAnimationDuration = 0.2f;
constexpr char kFontRelativePath[] = "assets/fonts/Geneva.ttf";
constexpr float kButtonHorizontalPadding = 44.f;
constexpr float kButtonVerticalPadding = 24.f;
const sf::Color kPrimaryButtonColor(0, 150, 255);
const sf::Color kPrimaryButtonHoverColor(50, 180, 255);
const sf::Color kDangerButtonColor(190, 70, 70);
const sf::Color kDangerButtonHoverColor(220, 95, 95);

using Clock = std::chrono::steady_clock;
using SpawnAnimations = std::map<std::pair<int, int>, Clock::time_point>;

enum class SceneId { Splash, Playing, GameOver };
enum class SceneCommand { None, StartGame, ShowSplash, RestartGame, Quit };

void centerTextOrigin(sf::Text &text) {
    const auto bounds = text.getLocalBounds();
    text.setOrigin(bounds.left + bounds.width / 2.f, bounds.top + bounds.height / 2.f);
}

sf::RectangleShape createButtonForText(const sf::Text &label) {
    const auto bounds = label.getLocalBounds();
    sf::RectangleShape button(
        {bounds.width + kButtonHorizontalPadding, bounds.height + kButtonVerticalPadding});
    button.setOutlineThickness(4.f);
    button.setOutlineColor(sf::Color::White);
    button.setOrigin(button.getSize() / 2.f);
    return button;
}

void updateButtonColor(sf::RectangleShape &button, bool isHovered, const sf::Color &baseColor,
                       const sf::Color &hoverColor) {
    button.setFillColor(isHovered ? hoverColor : baseColor);
}

sf::Color getTileColor(int value) {
    switch (value) {
    case 2:
        return sf::Color(238, 228, 218);
    case 4:
        return sf::Color(237, 224, 200);
    case 8:
        return sf::Color(242, 177, 121);
    case 16:
        return sf::Color(245, 149, 99);
    case 32:
        return sf::Color(246, 124, 95);
    case 64:
        return sf::Color(246, 94, 59);
    case 128:
        return sf::Color(237, 207, 114);
    case 256:
        return sf::Color(237, 204, 97);
    case 512:
        return sf::Color(237, 200, 80);
    case 1024:
        return sf::Color(237, 197, 63);
    case 2048:
        return sf::Color(237, 194, 46);
    default:
        return sf::Color(60, 58, 50);
    }
}

std::optional<core2048::Direction> mapDirection(const sf::Keyboard::Key key) {
    switch (key) {
    case sf::Keyboard::Up:
        return core2048::Direction::Up;
    case sf::Keyboard::Down:
        return core2048::Direction::Down;
    case sf::Keyboard::Left:
        return core2048::Direction::Left;
    case sf::Keyboard::Right:
        return core2048::Direction::Right;
    default:
        return std::nullopt;
    }
}

class GameSession {
  public:
    explicit GameSession(const app::RunConfig &config) : config_(config) {
    }

    void resetGame() {
        if (config_.seed.has_value()) {
            game_.reset(*config_.seed);
        } else {
            game_.reset();
        }
        spawnAnimations_.clear();
    }

    void clearSpawnAnimations() {
        spawnAnimations_.clear();
    }

    const core2048::Game &game() const {
        return game_;
    }

    SpawnAnimations &spawnAnimations() {
        return spawnAnimations_;
    }

    void applyMove(core2048::Direction direction) {
        const auto result = game_.applyMove(direction);
        if (!result.spawnedTile.has_value()) {
            return;
        }

        const auto &spawned = *result.spawnedTile;
        spawnAnimations_[{spawned.row, spawned.col}] = Clock::now();
    }

  private:
    const app::RunConfig &config_;
    core2048::Game game_;
    SpawnAnimations spawnAnimations_;
};

class SplashScene {
  public:
    SplashScene(const sf::Font &font, const float width, const float height)
        : title_("2048", font, 72), buttonText_("START", font, 36),
          button_(createButtonForText(buttonText_)) {
        title_.setStyle(sf::Text::Bold | sf::Text::Underlined);
        title_.setFillColor(sf::Color::Black);
        centerTextOrigin(title_);
        title_.setPosition(width / 2.f, height / 3.f);

        buttonText_.setFillColor(sf::Color::White);
        centerTextOrigin(buttonText_);

        button_.setFillColor(kPrimaryButtonColor);
        button_.setPosition(width / 2.f, height / 2.f);
        buttonText_.setPosition(button_.getPosition());
    }

    SceneCommand handleEvent(const sf::Event &event, const sf::RenderWindow &window) const {
        if (event.type == sf::Event::KeyPressed &&
            (event.key.code == sf::Keyboard::Escape || event.key.code == sf::Keyboard::Q)) {
            return SceneCommand::Quit;
        }

        if (event.type == sf::Event::MouseButtonPressed &&
            event.mouseButton.button == sf::Mouse::Left) {
            const sf::Vector2f clickPos =
                window.mapPixelToCoords({event.mouseButton.x, event.mouseButton.y});
            if (button_.getGlobalBounds().contains(clickPos)) {
                return SceneCommand::StartGame;
            }
        }

        return SceneCommand::None;
    }

    void updateHover(const sf::Vector2f &mousePos) {
        updateButtonColor(button_, button_.getGlobalBounds().contains(mousePos),
                          kPrimaryButtonColor, kPrimaryButtonHoverColor);
    }

    void render(sf::RenderWindow &window) const {
        window.draw(title_);
        window.draw(button_);
        window.draw(buttonText_);
    }

  private:
    sf::Text title_;
    sf::Text buttonText_;
    sf::RectangleShape button_;
};

class PlayingScene {
  public:
    explicit PlayingScene(const sf::Font &font) : scoreText_("", font, 24) {
        scoreText_.setFillColor(sf::Color::Black);
    }

    SceneCommand handleEvent(const sf::Event &event, GameSession &session) {
        if (event.type != sf::Event::KeyPressed) {
            return SceneCommand::None;
        }

        if (event.key.code == sf::Keyboard::Escape) {
            session.clearSpawnAnimations();
            return SceneCommand::ShowSplash;
        }

        if (const auto direction = mapDirection(event.key.code); direction.has_value()) {
            session.applyMove(*direction);
        }

        return SceneCommand::None;
    }

    void render(sf::RenderWindow &window, const sf::Font &font, GameSession &session,
                const float width) {
        sf::RectangleShape panelBg({width, static_cast<float>(kTopPanelHeight)});
        panelBg.setFillColor({237, 224, 200});
        window.draw(panelBg);

        const auto &game = session.game();
        scoreText_.setString("Score: " + std::to_string(game.getScore()));
        const auto scoreBounds = scoreText_.getLocalBounds();
        scoreText_.setPosition(12.f, kTopPanelHeight / 2.f -
                                         (scoreBounds.top + scoreBounds.height / 2.f));
        window.draw(scoreText_);

        const auto &grid = game.getGrid();
        auto &spawnAnimations = session.spawnAnimations();
        for (int row = 0; row < kGridSize; ++row) {
            for (int col = 0; col < kGridSize; ++col) {
                const int value = grid[row][col];
                float scale = 1.f;

                const auto cellPos = std::make_pair(row, col);
                if (auto animIt = spawnAnimations.find(cellPos); animIt != spawnAnimations.end()) {
                    const float elapsed =
                        std::chrono::duration<float>(Clock::now() - animIt->second).count();
                    if (elapsed < kSpawnAnimationDuration) {
                        scale = elapsed / kSpawnAnimationDuration;
                    } else {
                        spawnAnimations.erase(animIt);
                    }
                }

                sf::RectangleShape cell(
                    {static_cast<float>(kCellSize), static_cast<float>(kCellSize)});
                cell.setOrigin(kCellSize / 2.f, kCellSize / 2.f);
                cell.setScale(scale, scale);
                cell.setPosition(col * kCellSize + (col + 1) * kPadding + kCellSize / 2.f,
                                 kTopPanelHeight + row * kCellSize + (row + 1) * kPadding +
                                     kCellSize / 2.f);
                cell.setFillColor(value ? getTileColor(value) : sf::Color(205, 193, 180));
                window.draw(cell);

                if (value != 0) {
                    sf::Text tileText(std::to_string(value), font, 32);
                    tileText.setFillColor(value <= 4 ? sf::Color::Black : sf::Color::White);
                    centerTextOrigin(tileText);
                    tileText.setScale(scale, scale);
                    tileText.setPosition(cell.getPosition());
                    window.draw(tileText);
                }
            }
        }
    }

  private:
    sf::Text scoreText_;
};

class GameOverScene {
  public:
    GameOverScene(const sf::Font &font, const float width, const float height)
        : overlay_({width, height}), box_({380.f, 250.f}), title_("Game Over", font, 40),
          scoreText_("", font, 26), hintText_("N/Enter: New Game   Q/Esc: Quit", font, 16),
          newGameText_("NEW GAME", font, 22), newGameButton_(createButtonForText(newGameText_)),
          quitText_("QUIT", font, 22), quitButton_(createButtonForText(quitText_)), width_(width),
          height_(height) {
        overlay_.setFillColor(sf::Color(0, 0, 0, 150));

        box_.setFillColor(sf::Color::White);
        box_.setOutlineThickness(4.f);
        box_.setOutlineColor({200, 0, 0});
        box_.setOrigin(box_.getSize() / 2.f);

        title_.setFillColor(sf::Color::Red);
        title_.setStyle(sf::Text::Bold);
        centerTextOrigin(title_);

        scoreText_.setFillColor(sf::Color(30, 30, 30));
        scoreText_.setStyle(sf::Text::Bold);

        hintText_.setFillColor(sf::Color(80, 80, 80));
        centerTextOrigin(hintText_);

        newGameText_.setFillColor(sf::Color::White);
        centerTextOrigin(newGameText_);
        newGameButton_.setFillColor(kPrimaryButtonColor);

        quitText_.setFillColor(sf::Color::White);
        centerTextOrigin(quitText_);
        quitButton_.setFillColor(kDangerButtonColor);

        layout();
    }

    SceneCommand handleEvent(const sf::Event &event, const sf::RenderWindow &window) const {
        if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::N || event.key.code == sf::Keyboard::Enter) {
                return SceneCommand::RestartGame;
            }
            if (event.key.code == sf::Keyboard::Q || event.key.code == sf::Keyboard::Escape) {
                return SceneCommand::Quit;
            }
        }

        if (event.type == sf::Event::MouseButtonPressed &&
            event.mouseButton.button == sf::Mouse::Left) {
            const sf::Vector2f clickPos =
                window.mapPixelToCoords({event.mouseButton.x, event.mouseButton.y});
            if (newGameButton_.getGlobalBounds().contains(clickPos)) {
                return SceneCommand::RestartGame;
            }
            if (quitButton_.getGlobalBounds().contains(clickPos)) {
                return SceneCommand::Quit;
            }
        }

        return SceneCommand::None;
    }

    void updateHover(const sf::Vector2f &mousePos) {
        updateButtonColor(newGameButton_, newGameButton_.getGlobalBounds().contains(mousePos),
                          kPrimaryButtonColor, kPrimaryButtonHoverColor);
        updateButtonColor(quitButton_, quitButton_.getGlobalBounds().contains(mousePos),
                          kDangerButtonColor, kDangerButtonHoverColor);
    }

    void render(sf::RenderWindow &window, const int score) {
        window.draw(overlay_);
        window.draw(box_);
        window.draw(title_);

        scoreText_.setString("Final Score: " + std::to_string(score));
        centerTextOrigin(scoreText_);
        scoreText_.setPosition(width_ / 2.f, height_ / 2.f - 36.f);
        window.draw(scoreText_);

        window.draw(hintText_);
        window.draw(newGameButton_);
        window.draw(newGameText_);
        window.draw(quitButton_);
        window.draw(quitText_);
    }

  private:
    void layout() {
        box_.setPosition(width_ / 2.f, height_ / 2.f - 5.f);
        title_.setPosition(width_ / 2.f, height_ / 2.f - 82.f);
        hintText_.setPosition(width_ / 2.f, height_ / 2.f - 2.f);

        newGameButton_.setPosition(width_ / 2.f - 95.f, height_ / 2.f + 66.f);
        newGameText_.setPosition(newGameButton_.getPosition());
        quitButton_.setPosition(width_ / 2.f + 95.f, height_ / 2.f + 66.f);
        quitText_.setPosition(quitButton_.getPosition());
    }

    sf::RectangleShape overlay_;
    sf::RectangleShape box_;
    sf::Text title_;
    sf::Text scoreText_;
    sf::Text hintText_;
    sf::Text newGameText_;
    sf::RectangleShape newGameButton_;
    sf::Text quitText_;
    sf::RectangleShape quitButton_;
    float width_;
    float height_;
};

void applySceneCommand(SceneCommand command, SceneId &scene, GameSession &session,
                       sf::RenderWindow &window) {
    switch (command) {
    case SceneCommand::None:
        return;
    case SceneCommand::StartGame:
    case SceneCommand::RestartGame:
        session.resetGame();
        scene = SceneId::Playing;
        return;
    case SceneCommand::ShowSplash:
        scene = SceneId::Splash;
        return;
    case SceneCommand::Quit:
        window.close();
        return;
    }
}

} // namespace

namespace app {

int run(const RunConfig &config) {
    const auto width =
        static_cast<unsigned int>(kGridSize * kCellSize + (kGridSize + 1) * kPadding);
    const auto height = static_cast<unsigned int>(kTopPanelHeight + kGridSize * kCellSize +
                                                  (kGridSize + 1) * kPadding);

    sf::RenderWindow window({width, height}, "2048", sf::Style::Titlebar | sf::Style::Close);
    window.setVerticalSyncEnabled(config.vSyncEnabled);
    if (config.frameLimit.has_value()) {
        window.setFramerateLimit(*config.frameLimit);
    }

    const auto fontResolution = resolveAssetPath(kFontRelativePath);

    sf::Font font;
    if (!fontResolution.resolvedPath.has_value() ||
        !font.loadFromFile(fontResolution.resolvedPath->string())) {
        std::cerr << "Font could not be loaded. Tried:\n";
        for (const auto &candidate : fontResolution.candidates) {
            std::cerr << "  - " << candidate.string() << "\n";
        }
        return 1;
    }

    GameSession session(config);
    SplashScene splashScene(font, static_cast<float>(width), static_cast<float>(height));
    PlayingScene playingScene(font);
    GameOverScene gameOverScene(font, static_cast<float>(width), static_cast<float>(height));
    SceneId scene = SceneId::Splash;

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
                continue;
            }

            SceneCommand command = SceneCommand::None;
            switch (scene) {
            case SceneId::Splash:
                command = splashScene.handleEvent(event, window);
                break;
            case SceneId::Playing:
                command = playingScene.handleEvent(event, session);
                break;
            case SceneId::GameOver:
                command = gameOverScene.handleEvent(event, window);
                break;
            }

            applySceneCommand(command, scene, session, window);
        }

        if (!window.isOpen()) {
            break;
        }

        const sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        if (scene == SceneId::Splash) {
            splashScene.updateHover(mousePos);
        } else if (scene == SceneId::GameOver) {
            gameOverScene.updateHover(mousePos);
        }

        if (scene == SceneId::Playing && session.game().isGameOver()) {
            session.clearSpawnAnimations();
            scene = SceneId::GameOver;
        }

        window.clear(sf::Color(250, 248, 239));

        if (scene == SceneId::Splash) {
            splashScene.render(window);
            window.display();
            continue;
        }

        playingScene.render(window, font, session, static_cast<float>(width));
        if (scene == SceneId::GameOver) {
            gameOverScene.render(window, session.game().getScore());
        }

        window.display();
    }

    return 0;
}

} // namespace app
