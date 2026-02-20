#include "app/App.hpp"
#include "core/Game.hpp"

#include <SFML/Graphics.hpp>

#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace {

constexpr int kCellSize = 100;
constexpr int kGridSize = core2048::Game::kGridSize;
constexpr int kPadding = 10;
constexpr int kTopPanelHeight = 80;
constexpr char kFontRelativePath[] = "assets/fonts/Geneva.ttf";
constexpr float kButtonHorizontalPadding = 44.f;
constexpr float kButtonVerticalPadding = 24.f;
const sf::Color kPrimaryButtonColor(0, 150, 255);
const sf::Color kPrimaryButtonHoverColor(50, 180, 255);
const sf::Color kDangerButtonColor(190, 70, 70);
const sf::Color kDangerButtonHoverColor(220, 95, 95);

std::filesystem::path getExecutablePath() {
#if defined(_WIN32)
    std::vector<wchar_t> buffer(260);

    while (true) {
        const DWORD copied =
            GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));

        if (copied == 0) {
            return {};
        }

        if (copied < static_cast<DWORD>(buffer.size())) {
            return std::filesystem::path(std::wstring(buffer.data(), copied));
        }

        buffer.resize(buffer.size() * 2);
    }
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    if (size == 0) {
        return {};
    }

    std::string buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        return {};
    }

    std::filesystem::path executable(buffer.c_str());
    std::error_code ec;
    const auto resolved = std::filesystem::weakly_canonical(executable, ec);
    return ec ? executable : resolved;
#elif defined(__linux__)
    std::array<char, 4096> buffer{};
    const ssize_t count = ::readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (count <= 0) {
        return {};
    }

    buffer[static_cast<size_t>(count)] = '\0';
    return std::filesystem::path(buffer.data());
#else
    return {};
#endif
}

std::vector<std::filesystem::path>
buildAssetPathCandidates(const std::filesystem::path &relativeAssetPath) {
    std::vector<std::filesystem::path> candidates;
    const auto appendCandidate = [&](const std::filesystem::path &candidatePath) {
        const auto normalized = candidatePath.lexically_normal();
        for (const auto &existing : candidates) {
            if (existing == normalized) {
                return;
            }
        }
        candidates.push_back(normalized);
    };

    const auto executablePath = getExecutablePath();
    if (!executablePath.empty()) {
        const auto executableDir = executablePath.parent_path();
        appendCandidate(executableDir / relativeAssetPath);
        appendCandidate(executableDir.parent_path() / relativeAssetPath);
#if defined(__APPLE__)
        appendCandidate(executableDir / ".." / ".." / "Resources" / relativeAssetPath);
#endif
    }

    std::error_code ec;
    const auto cwd = std::filesystem::current_path(ec);
    if (!ec) {
        appendCandidate(cwd / relativeAssetPath);
    }

    appendCandidate(relativeAssetPath);
    return candidates;
}

std::optional<std::filesystem::path>
findFirstExistingPath(const std::vector<std::filesystem::path> &candidates) {
    for (const auto &candidate : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec) && !ec) {
            return candidate;
        }
    }
    return std::nullopt;
}

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

} // namespace

namespace app {

int run(const RunConfig &config) {
    using Clock = std::chrono::steady_clock;
    std::map<std::pair<int, int>, Clock::time_point> spawnAnimations;
    constexpr float animationDuration = 0.2f;

    enum class GameState { Splash, Playing, GameOver };
    GameState state = GameState::Splash;
    core2048::Game game;

    const auto resetGame = [&]() {
        if (config.seed.has_value()) {
            game.reset(*config.seed);
        } else {
            game.reset();
        }
        spawnAnimations.clear();
    };

    const int width = kGridSize * kCellSize + (kGridSize + 1) * kPadding;
    const int height = kTopPanelHeight + kGridSize * kCellSize + (kGridSize + 1) * kPadding;

    sf::RenderWindow window({(unsigned int)width, (unsigned int)height}, "2048",
                            sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);

    const auto fontCandidates = buildAssetPathCandidates(kFontRelativePath);
    const auto resolvedFontPath = findFirstExistingPath(fontCandidates);

    sf::Font font;
    if (!resolvedFontPath.has_value() || !font.loadFromFile(resolvedFontPath->string())) {
        std::cerr << "Font could not be loaded. Tried:\n";
        for (const auto &candidate : fontCandidates) {
            std::cerr << "  - " << candidate.string() << "\n";
        }
        return 1;
    }

    sf::Text splashTitle("2048", font, 72);
    splashTitle.setStyle(sf::Text::Bold | sf::Text::Underlined);
    splashTitle.setFillColor(sf::Color::Black);
    centerTextOrigin(splashTitle);
    splashTitle.setPosition(width / 2.f, height / 3.f);

    sf::Text splashButtonText("START", font, 36);
    splashButtonText.setFillColor(sf::Color::White);
    centerTextOrigin(splashButtonText);

    sf::RectangleShape splashButton = createButtonForText(splashButtonText);
    splashButton.setFillColor(kPrimaryButtonColor);
    splashButton.setPosition(width / 2.f, height / 2.f);

    splashButtonText.setPosition(splashButton.getPosition());

    sf::Text scoreText("", font, 24);
    scoreText.setFillColor(sf::Color::Black);

    sf::RectangleShape overFade({(float)width, (float)height});
    overFade.setFillColor(sf::Color(0, 0, 0, 150));

    sf::RectangleShape overBox({380.f, 250.f});
    overBox.setFillColor(sf::Color::White);
    overBox.setOutlineThickness(4.f);
    overBox.setOutlineColor({200, 0, 0});
    overBox.setOrigin(overBox.getSize() / 2.f);

    sf::Text overText("Game Over", font, 40);
    overText.setFillColor(sf::Color::Red);
    overText.setStyle(sf::Text::Bold);
    centerTextOrigin(overText);

    sf::Text overScoreText("", font, 26);
    overScoreText.setFillColor(sf::Color(30, 30, 30));
    overScoreText.setStyle(sf::Text::Bold);

    sf::Text overHintText("N/Enter: New Game   Q/Esc: Quit", font, 16);
    overHintText.setFillColor(sf::Color(80, 80, 80));
    centerTextOrigin(overHintText);

    sf::Text newGameText("NEW GAME", font, 22);
    newGameText.setFillColor(sf::Color::White);
    centerTextOrigin(newGameText);
    sf::RectangleShape newGameButton = createButtonForText(newGameText);
    newGameButton.setFillColor(kPrimaryButtonColor);

    sf::Text quitText("QUIT", font, 22);
    quitText.setFillColor(sf::Color::White);
    centerTextOrigin(quitText);
    sf::RectangleShape quitButton = createButtonForText(quitText);
    quitButton.setFillColor(kDangerButtonColor);

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }

            if (state == GameState::Splash) {
                if (event.type == sf::Event::KeyPressed &&
                    (event.key.code == sf::Keyboard::Escape || event.key.code == sf::Keyboard::Q)) {
                    window.close();
                }
                if (event.type == sf::Event::MouseButtonPressed &&
                    event.mouseButton.button == sf::Mouse::Left) {
                    const sf::Vector2f clickPos =
                        window.mapPixelToCoords({event.mouseButton.x, event.mouseButton.y});
                    if (splashButton.getGlobalBounds().contains(clickPos)) {
                        resetGame();
                        state = GameState::Playing;
                    }
                }
                continue;
            }

            if (state == GameState::Playing) {
                if (event.type == sf::Event::KeyPressed) {
                    if (event.key.code == sf::Keyboard::Escape) {
                        spawnAnimations.clear();
                        state = GameState::Splash;
                        continue;
                    }

                    const auto dir = mapDirection(event.key.code);
                    if (dir.has_value()) {
                        const auto result = game.applyMove(*dir);
                        if (result.spawnedTile.has_value()) {
                            const auto &spawned = *result.spawnedTile;
                            spawnAnimations[{spawned.row, spawned.col}] = Clock::now();
                        }
                    }
                }

                if (game.isGameOver()) {
                    spawnAnimations.clear();
                    state = GameState::GameOver;
                }
                continue;
            }

            if (state == GameState::GameOver) {
                if (event.type == sf::Event::KeyPressed) {
                    if (event.key.code == sf::Keyboard::N ||
                        event.key.code == sf::Keyboard::Enter) {
                        resetGame();
                        state = GameState::Playing;
                    } else if (event.key.code == sf::Keyboard::Q ||
                               event.key.code == sf::Keyboard::Escape) {
                        window.close();
                    }
                }

                if (event.type == sf::Event::MouseButtonPressed &&
                    event.mouseButton.button == sf::Mouse::Left) {
                    const sf::Vector2f clickPos =
                        window.mapPixelToCoords({event.mouseButton.x, event.mouseButton.y});
                    if (newGameButton.getGlobalBounds().contains(clickPos)) {
                        resetGame();
                        state = GameState::Playing;
                    } else if (quitButton.getGlobalBounds().contains(clickPos)) {
                        window.close();
                    }
                }
            }
        }

        const sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        if (state == GameState::Splash) {
            updateButtonColor(splashButton, splashButton.getGlobalBounds().contains(mousePos),
                              kPrimaryButtonColor, kPrimaryButtonHoverColor);
        } else if (state == GameState::GameOver) {
            updateButtonColor(newGameButton, newGameButton.getGlobalBounds().contains(mousePos),
                              kPrimaryButtonColor, kPrimaryButtonHoverColor);
            updateButtonColor(quitButton, quitButton.getGlobalBounds().contains(mousePos),
                              kDangerButtonColor, kDangerButtonHoverColor);
        }

        window.clear(sf::Color(250, 248, 239));

        if (state == GameState::Splash) {
            window.draw(splashTitle);
            window.draw(splashButton);
            window.draw(splashButtonText);
            window.display();
            continue;
        }

        sf::RectangleShape panelBg({(float)width, (float)kTopPanelHeight});
        panelBg.setFillColor({237, 224, 200});
        window.draw(panelBg);

        scoreText.setString("Score: " + std::to_string(game.getScore()));
        const auto scoreBounds = scoreText.getLocalBounds();
        scoreText.setPosition(12.f,
                              kTopPanelHeight / 2.f - (scoreBounds.top + scoreBounds.height / 2.f));
        window.draw(scoreText);

        const auto &grid = game.getGrid();
        for (int r = 0; r < kGridSize; ++r) {
            for (int c = 0; c < kGridSize; ++c) {
                const int value = grid[r][c];
                float scale = 1.f;

                const auto cellPos = std::make_pair(r, c);
                auto animIt = spawnAnimations.find(cellPos);
                if (animIt != spawnAnimations.end()) {
                    const float elapsed =
                        std::chrono::duration<float>(Clock::now() - animIt->second).count();
                    if (elapsed < animationDuration) {
                        scale = elapsed / animationDuration;
                    } else {
                        spawnAnimations.erase(animIt);
                    }
                }

                sf::RectangleShape cell({(float)kCellSize, (float)kCellSize});
                cell.setOrigin(kCellSize / 2.f, kCellSize / 2.f);
                cell.setScale(scale, scale);
                cell.setPosition(c * kCellSize + (c + 1) * kPadding + kCellSize / 2.f,
                                 kTopPanelHeight + r * kCellSize + (r + 1) * kPadding +
                                     kCellSize / 2.f);
                cell.setFillColor(value ? getTileColor(value) : sf::Color(205, 193, 180));
                window.draw(cell);

                if (value) {
                    sf::Text tileText(std::to_string(value), font, 32);
                    tileText.setFillColor(value <= 4 ? sf::Color::Black : sf::Color::White);
                    centerTextOrigin(tileText);
                    tileText.setScale(scale, scale);
                    tileText.setPosition(cell.getPosition());
                    window.draw(tileText);
                }
            }
        }

        if (state == GameState::GameOver) {
            window.draw(overFade);

            overBox.setPosition(width / 2.f, height / 2.f - 5.f);
            window.draw(overBox);

            overText.setPosition(width / 2.f, height / 2.f - 82.f);
            window.draw(overText);

            overScoreText.setString("Final Score: " + std::to_string(game.getScore()));
            centerTextOrigin(overScoreText);
            overScoreText.setPosition(width / 2.f, height / 2.f - 36.f);
            window.draw(overScoreText);

            overHintText.setPosition(width / 2.f, height / 2.f - 2.f);
            window.draw(overHintText);

            newGameButton.setPosition(width / 2.f - 95.f, height / 2.f + 66.f);
            newGameText.setPosition(newGameButton.getPosition());
            window.draw(newGameButton);
            window.draw(newGameText);

            quitButton.setPosition(width / 2.f + 95.f, height / 2.f + 66.f);
            quitText.setPosition(quitButton.getPosition());
            window.draw(quitButton);
            window.draw(quitText);
        }

        window.display();
    }

    return 0;
}

} // namespace app
