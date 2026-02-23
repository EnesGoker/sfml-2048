#include "app/App.hpp"
#include "app/AssetResolver.hpp"
#include "app/SoundManager.hpp"
#include "core/Game.hpp"
#include "core/ScoreManager.hpp"

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

constexpr int kCellSize = 100;
constexpr int kGridSize = core2048::Game::kGridSize;
constexpr int kPadding = 10;
constexpr int kTopPanelHeight = 80;
constexpr char kFontRelativePath[] = "assets/fonts/Inter-Variable.ttf";
constexpr char kScoresRelativePath[] = "scores.json";
constexpr char kSettingsRelativePath[] = "settings.json";
constexpr char kSoundsRelativePath[] = "assets/sounds";
constexpr unsigned int kWindowAntialiasingLevel = 16;
constexpr std::size_t kRoundedCornerPointCount = 32;
constexpr float kClickPadding = 4.f;

constexpr float kTileCornerRadius = 14.f;
constexpr float kButtonCornerRadius = 16.f;
constexpr float kPanelCornerRadius = 12.f;
constexpr float kButtonHorizontalPadding = 44.f;
constexpr float kButtonVerticalPadding = 24.f;

constexpr float kSlideAnimationDuration = 0.09f;
constexpr float kMergePopDuration = 0.12f;
constexpr float kSpawnFadeDuration = 0.14f;
constexpr float kFloatingScoreDuration = 0.85f;

constexpr float kPi = 3.14159265358979323846f;

const sf::Color kBoardBackgroundColor(250, 248, 239);
const sf::Color kEmptyTileColor(205, 193, 180);
const sf::Color kPrimaryButtonColor(0, 150, 255);
const sf::Color kPrimaryButtonHoverColor(50, 180, 255);
const sf::Color kDangerButtonColor(190, 70, 70);
const sf::Color kDangerButtonHoverColor(220, 95, 95);
const sf::Color kMenuButtonColor(138, 128, 110);
const sf::Color kMenuButtonHoverColor(160, 149, 129);
const sf::Color kMenuPanelColor(247, 241, 229);
const sf::Color kMenuPanelOutlineColor(217, 206, 184);
const sf::Color kSoundOnButtonColor(76, 157, 87);
const sf::Color kSoundOnButtonHoverColor(101, 182, 112);
const sf::Color kSoundOffButtonColor(145, 145, 145);
const sf::Color kSoundOffButtonHoverColor(170, 170, 170);
const sf::Color kTextInputColor(249, 247, 240);
const sf::Color kTextInputOutline(180, 170, 150);
const sf::Color kTextInputFocusedOutline(0, 150, 255);

using Clock = std::chrono::steady_clock;

enum class SceneId { Splash, HighScores, Playing, GameOver };
enum class SceneCommand {
    None,
    StartGame,
    ShowHighScores,
    ShowSplash,
    RestartGame,
    ToggleSound,
    Quit
};

struct BoardCell {
    int row;
    int col;

    bool operator<(const BoardCell &other) const {
        if (row != other.row) {
            return row < other.row;
        }
        return col < other.col;
    }
};

struct MovingTileVisual {
    int value;
    BoardCell from;
    BoardCell to;
    bool partOfMerge{false};
};

struct MergeCellVisual {
    BoardCell cell;
    int value;
};

struct MoveVisualPlan {
    std::vector<MovingTileVisual> movingTiles;
    std::vector<MergeCellVisual> mergeCells;
};

class RoundedRectShape final : public sf::Shape {
  public:
    RoundedRectShape(sf::Vector2f size = {}, float radius = 0.f,
                     std::size_t cornerPointCount = kRoundedCornerPointCount)
        : size_(size), radius_(radius),
          cornerPointCount_(std::max<std::size_t>(cornerPointCount, 2U)) {
        update();
    }

    void setSize(sf::Vector2f size) {
        size_ = size;
        update();
    }

    sf::Vector2f getSize() const {
        return size_;
    }

    void setCornersRadius(float radius) {
        radius_ = radius;
        update();
    }

    float getCornersRadius() const {
        const float maxRadius = std::min(size_.x, size_.y) * 0.5f;
        return std::clamp(radius_, 0.f, maxRadius);
    }

    void setCornerPointCount(std::size_t cornerPointCount) {
        cornerPointCount_ = std::max<std::size_t>(cornerPointCount, 2U);
        update();
    }

    std::size_t getPointCount() const override {
        return cornerPointCount_ * 4U;
    }

    sf::Vector2f getPoint(std::size_t index) const override {
        const float radius = getCornersRadius();

        if (radius <= 0.f) {
            switch ((index / cornerPointCount_) % 4U) {
            case 0:
                return {0.f, 0.f};
            case 1:
                return {size_.x, 0.f};
            case 2:
                return {size_.x, size_.y};
            default:
                return {0.f, size_.y};
            }
        }

        const std::array<sf::Vector2f, 4> centers = {
            sf::Vector2f(radius, radius),
            sf::Vector2f(size_.x - radius, radius),
            sf::Vector2f(size_.x - radius, size_.y - radius),
            sf::Vector2f(radius, size_.y - radius),
        };

        const std::array<float, 4> baseAngles = {180.f, 270.f, 0.f, 90.f};

        const std::size_t corner = (index / cornerPointCount_) % 4U;
        const std::size_t pointInCorner = index % cornerPointCount_;
        const float step = 90.f / static_cast<float>(cornerPointCount_ - 1U);
        const float angle =
            (baseAngles[corner] + static_cast<float>(pointInCorner) * step) * (kPi / 180.f);

        return {
            centers[corner].x + std::cos(angle) * radius,
            centers[corner].y + std::sin(angle) * radius,
        };
    }

  private:
    sf::Vector2f size_;
    float radius_{0.f};
    std::size_t cornerPointCount_{kRoundedCornerPointCount};
};

float clamp01(const float value) {
    return std::clamp(value, 0.f, 1.f);
}

sf::Uint8 toAlpha(const float normalized) {
    return static_cast<sf::Uint8>(std::clamp(normalized, 0.f, 1.f) * 255.f);
}

float lerp(const float a, const float b, const float t) {
    return a + (b - a) * t;
}

sf::Vector2f lerp(const sf::Vector2f &a, const sf::Vector2f &b, const float t) {
    return {lerp(a.x, b.x, t), lerp(a.y, b.y, t)};
}

bool isPrimaryMouseRelease(const sf::Event &event) {
    return event.type == sf::Event::MouseButtonReleased &&
           event.mouseButton.button == sf::Mouse::Left;
}

sf::Vector2f cursorPosition(const sf::RenderWindow &window) {
    return window.mapPixelToCoords(sf::Mouse::getPosition(window));
}

bool containsWithPadding(const sf::FloatRect &bounds, const sf::Vector2f &point,
                         const float padding = kClickPadding) {
    return sf::FloatRect(bounds.left - padding, bounds.top - padding, bounds.width + 2.f * padding,
                         bounds.height + 2.f * padding)
        .contains(point);
}

sf::String toUnicode(const std::string_view utf8Text) {
    return sf::String::fromUtf8(utf8Text.begin(), utf8Text.end());
}

bool supportsText(const sf::Font &font, const sf::String &text) {
    for (std::size_t i = 0; i < text.getSize(); ++i) {
        const sf::Uint32 codePoint = text[i];
        if (codePoint == U' ' || codePoint == U'\n' || codePoint == U'\t' || codePoint == U'\r') {
            continue;
        }
        if (!font.hasGlyph(codePoint)) {
            return false;
        }
    }
    return true;
}

sf::String localizedText(const sf::Font &font, const std::string_view preferredUtf8,
                         const std::string_view asciiFallback) {
    const sf::String preferred = toUnicode(preferredUtf8);
    if (supportsText(font, preferred)) {
        return preferred;
    }
    return toUnicode(asciiFallback);
}

void centerTextOrigin(sf::Text &text) {
    const auto bounds = text.getLocalBounds();
    text.setOrigin(bounds.left + bounds.width / 2.f, bounds.top + bounds.height / 2.f);
}

sf::Vector2f cellCenter(const BoardCell &cell) {
    return {
        static_cast<float>(cell.col * kCellSize + (cell.col + 1) * kPadding + kCellSize / 2),
        static_cast<float>(kTopPanelHeight + cell.row * kCellSize + (cell.row + 1) * kPadding +
                           kCellSize / 2),
    };
}

RoundedRectShape createButtonForText(const sf::Text &label, const sf::Color &fillColor) {
    const auto bounds = label.getLocalBounds();
    RoundedRectShape button(
        {bounds.width + kButtonHorizontalPadding, bounds.height + kButtonVerticalPadding},
        kButtonCornerRadius, kRoundedCornerPointCount);
    button.setFillColor(fillColor);
    button.setOutlineThickness(3.f);
    button.setOutlineColor(sf::Color::White);
    button.setOrigin(button.getSize() / 2.f);
    return button;
}

void updateButtonColor(sf::Shape &button, const bool isHovered, const sf::Color &baseColor,
                       const sf::Color &hoverColor) {
    button.setFillColor(isHovered ? hoverColor : baseColor);
}

sf::Color getTileColor(const int value) {
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
    case 4096:
        return sf::Color(129, 168, 84);
    default:
        return sf::Color(60, 58, 50);
    }
}

void drawEmptyCell(sf::RenderWindow &window, const sf::Vector2f &center, const float scale = 1.f) {
    RoundedRectShape cell({static_cast<float>(kCellSize), static_cast<float>(kCellSize)},
                          kTileCornerRadius, kRoundedCornerPointCount);
    cell.setOrigin(cell.getSize() / 2.f);
    cell.setPosition(center);
    cell.setScale(scale, scale);
    cell.setFillColor(kEmptyTileColor);
    window.draw(cell);
}

void drawTile(sf::RenderWindow &window, const sf::Font &font, const int value,
              const sf::Vector2f &center, const float scale = 1.f, const sf::Uint8 alpha = 255) {
    if (value <= 0) {
        return;
    }

    RoundedRectShape cell({static_cast<float>(kCellSize), static_cast<float>(kCellSize)},
                          kTileCornerRadius, kRoundedCornerPointCount);
    cell.setOrigin(cell.getSize() / 2.f);
    cell.setScale(scale, scale);
    cell.setPosition(center);
    auto tileColor = getTileColor(value);
    tileColor.a = alpha;
    cell.setFillColor(tileColor);
    window.draw(cell);

    const unsigned int charSize = (value < 100)     ? 34U
                                  : (value < 1000)  ? 30U
                                  : (value < 10000) ? 24U
                                                    : 20U;
    sf::Text tileText(std::to_string(value), font, charSize);
    auto textColor = (value <= 4) ? sf::Color(119, 110, 101) : sf::Color::White;
    textColor.a = alpha;
    tileText.setFillColor(textColor);
    centerTextOrigin(tileText);
    tileText.setScale(scale, scale);
    tileText.setPosition(center);
    window.draw(tileText);
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

BoardCell cellAtLineIndex(const int line, const int index, const core2048::Direction direction) {
    switch (direction) {
    case core2048::Direction::Left:
        return {line, index};
    case core2048::Direction::Right:
        return {line, kGridSize - 1 - index};
    case core2048::Direction::Up:
        return {index, line};
    case core2048::Direction::Down:
        return {kGridSize - 1 - index, line};
    }

    return {line, index};
}

MoveVisualPlan buildMoveVisualPlan(const core2048::Game::Grid &beforeGrid,
                                   const core2048::Direction direction) {
    struct LineToken {
        int value;
        int fromIndex;
    };

    MoveVisualPlan plan;

    for (int line = 0; line < kGridSize; ++line) {
        std::vector<LineToken> tokens;
        tokens.reserve(kGridSize);

        for (int index = 0; index < kGridSize; ++index) {
            const auto cell = cellAtLineIndex(line, index, direction);
            const int value = beforeGrid[cell.row][cell.col];
            if (value != 0) {
                tokens.push_back(LineToken{value, index});
            }
        }

        int targetIndex = 0;
        for (std::size_t i = 0; i < tokens.size(); ++i) {
            const bool mergesWithNext =
                (i + 1U < tokens.size()) && (tokens[i].value == tokens[i + 1U].value);

            if (mergesWithNext) {
                const auto fromA = cellAtLineIndex(line, tokens[i].fromIndex, direction);
                const auto fromB = cellAtLineIndex(line, tokens[i + 1U].fromIndex, direction);
                const auto to = cellAtLineIndex(line, targetIndex, direction);

                plan.movingTiles.push_back(MovingTileVisual{tokens[i].value, fromA, to, true});
                plan.movingTiles.push_back(MovingTileVisual{tokens[i + 1U].value, fromB, to, true});
                plan.mergeCells.push_back(MergeCellVisual{to, tokens[i].value * 2});

                ++i;
            } else {
                if (tokens[i].fromIndex != targetIndex) {
                    const auto from = cellAtLineIndex(line, tokens[i].fromIndex, direction);
                    const auto to = cellAtLineIndex(line, targetIndex, direction);
                    plan.movingTiles.push_back(MovingTileVisual{tokens[i].value, from, to, false});
                }
            }

            ++targetIndex;
        }
    }

    return plan;
}

std::filesystem::path resolveScoreFilePath() {
    std::error_code ec;
    const auto cwd = std::filesystem::current_path(ec);
    if (!ec) {
        return cwd / kScoresRelativePath;
    }
    return std::filesystem::path(kScoresRelativePath);
}

std::filesystem::path resolveSettingsFilePath() {
    std::error_code ec;
    const auto cwd = std::filesystem::current_path(ec);
    if (!ec) {
        return cwd / kSettingsRelativePath;
    }
    return std::filesystem::path(kSettingsRelativePath);
}

class GameSession {
  public:
    GameSession() = default;

    void setPlayerName(std::string playerName) {
        if (playerName.empty()) {
            playerName_ = "Oyuncu";
            return;
        }
        playerName_ = std::move(playerName);
    }

    const std::string &playerName() const noexcept {
        return playerName_;
    }

    void resetGame() {
        game_.reset();
    }

    core2048::MoveResult applyMove(const core2048::Direction direction) {
        return game_.applyMove(direction);
    }

    const core2048::Game &game() const {
        return game_;
    }

  private:
    std::string playerName_{"Oyuncu"};
    core2048::Game game_;
};

class SplashScene {
  public:
    SplashScene(const sf::Font &font, const float width, const float height)
        : title_("2048", font, 72),
          nameLabel_(localizedText(font, "Oyuncu Adı", "Oyuncu Adi"), font, 20),
          nameText_("", font, 26), startText_(localizedText(font, "BAŞLA", "BASLA"), font, 32),
          startButton_(createButtonForText(startText_, kPrimaryButtonColor)),
          scoresText_(localizedText(font, "EN İYİ 5 SKOR", "EN IYI 5 SKOR"), font, 22),
          scoresButton_(createButtonForText(scoresText_, kMenuButtonColor)),
          validationText_("", font, 16),
          nameBox_({320.f, 56.f}, kButtonCornerRadius, kRoundedCornerPointCount) {
        title_.setStyle(sf::Text::Bold | sf::Text::Underlined);
        title_.setFillColor(sf::Color(40, 40, 40));
        centerTextOrigin(title_);
        title_.setPosition(width / 2.f, height * 0.23f);

        nameLabel_.setFillColor(sf::Color(90, 84, 76));
        centerTextOrigin(nameLabel_);
        nameLabel_.setPosition(width / 2.f, height * 0.44f - 36.f);

        nameBox_.setOrigin(nameBox_.getSize() / 2.f);
        nameBox_.setPosition(width / 2.f, height * 0.44f + 18.f);
        nameBox_.setFillColor(kTextInputColor);
        nameBox_.setOutlineThickness(2.f);
        nameBox_.setOutlineColor(kTextInputOutline);

        nameText_.setFillColor(sf::Color(60, 58, 50));
        centerTextOrigin(nameText_);
        nameText_.setPosition(nameBox_.getPosition());

        startText_.setFillColor(sf::Color::White);
        centerTextOrigin(startText_);
        startButton_.setPosition(width / 2.f, height * 0.66f);
        startText_.setPosition(startButton_.getPosition());

        scoresText_.setFillColor(sf::Color::White);
        centerTextOrigin(scoresText_);
        scoresButton_.setPosition(width / 2.f, height * 0.78f);
        scoresText_.setPosition(scoresButton_.getPosition());

        validationText_.setFillColor(sf::Color(185, 64, 64));
        centerTextOrigin(validationText_);
        validationText_.setPosition(width / 2.f, height * 0.88f);

        refreshNameText();
    }

    const std::string &playerName() const noexcept {
        return playerName_;
    }

    SceneCommand handleEvent(const sf::Event &event, const sf::RenderWindow &window) {
        if (event.type == sf::Event::KeyPressed &&
            (event.key.code == sf::Keyboard::Escape || event.key.code == sf::Keyboard::Q)) {
            return SceneCommand::Quit;
        }

        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Enter) {
            return validateAndStart();
        }

        if (isPrimaryMouseRelease(event)) {
            const sf::Vector2f clickPos = cursorPosition(window);

            nameFocused_ = containsWithPadding(nameBox_.getGlobalBounds(), clickPos);
            if (containsWithPadding(startButton_.getGlobalBounds(), clickPos)) {
                return validateAndStart();
            }
            if (containsWithPadding(scoresButton_.getGlobalBounds(), clickPos)) {
                validationText_.setString("");
                return SceneCommand::ShowHighScores;
            }
            return SceneCommand::None;
        }

        if (nameFocused_ && event.type == sf::Event::TextEntered) {
            const auto unicode = event.text.unicode;
            if (unicode == 8U) {
                if (nameInput_.getSize() > 0U) {
                    nameInput_.erase(nameInput_.getSize() - 1U, 1U);
                    refreshNameText();
                }
            } else if (unicode >= 32U && unicode != 127U && unicode <= 0x10FFFFU) {
                if (nameInput_.getSize() < 18U) {
                    nameInput_ += unicode;
                    refreshNameText();
                }
            }
        }

        return SceneCommand::None;
    }

    void updateHover(const sf::Vector2f &mousePos) {
        updateButtonColor(startButton_, startButton_.getGlobalBounds().contains(mousePos),
                          kPrimaryButtonColor, kPrimaryButtonHoverColor);
        updateButtonColor(scoresButton_, scoresButton_.getGlobalBounds().contains(mousePos),
                          kMenuButtonColor, kMenuButtonHoverColor);
        nameBox_.setOutlineColor(nameFocused_ ? kTextInputFocusedOutline : kTextInputOutline);
    }

    void render(sf::RenderWindow &window) const {
        window.draw(title_);
        window.draw(nameLabel_);
        window.draw(nameBox_);
        window.draw(nameText_);
        window.draw(startButton_);
        window.draw(startText_);
        window.draw(scoresButton_);
        window.draw(scoresText_);
        window.draw(validationText_);
    }

  private:
    static bool isWhitespace(const sf::Uint32 codePoint) {
        return codePoint == U' ' || codePoint == U'\t' || codePoint == U'\n' || codePoint == U'\r';
    }

    static sf::String trimInput(const sf::String &value) {
        std::size_t begin = 0;
        while (begin < value.getSize() && isWhitespace(value[begin])) {
            ++begin;
        }

        std::size_t end = value.getSize();
        while (end > begin && isWhitespace(value[end - 1])) {
            --end;
        }

        return value.substring(begin, end - begin);
    }

    static std::string toUtf8(const sf::String &value) {
        const auto utf8 = value.toUtf8();
        return std::string(utf8.begin(), utf8.end());
    }

    void refreshNameText() {
        const sf::String trimmedName = trimInput(nameInput_);
        playerName_ = toUtf8(trimmedName);
        if (playerName_.empty()) {
            nameText_.setString(
                localizedText(*nameText_.getFont(), "Adınızı yazın", "Adinizi yazin"));
            nameText_.setFillColor(sf::Color(150, 144, 135));
        } else {
            nameText_.setString(trimmedName);
            nameText_.setFillColor(sf::Color(60, 58, 50));
        }
        centerTextOrigin(nameText_);
        nameText_.setPosition(nameBox_.getPosition());
    }

    SceneCommand validateAndStart() {
        refreshNameText();
        if (playerName_.empty()) {
            validationText_.setString(localizedText(
                *validationText_.getFont(), "Lütfen adınızı girin.", "Lutfen adinizi girin."));
            centerTextOrigin(validationText_);
            return SceneCommand::None;
        }

        validationText_.setString("");
        return SceneCommand::StartGame;
    }

    sf::Text title_;
    sf::Text nameLabel_;
    sf::Text nameText_;
    sf::Text startText_;
    RoundedRectShape startButton_;
    sf::Text scoresText_;
    RoundedRectShape scoresButton_;
    sf::Text validationText_;
    RoundedRectShape nameBox_;

    bool nameFocused_{false};
    sf::String nameInput_;
    std::string playerName_;
};

class HighScoresScene {
  public:
    HighScoresScene(const sf::Font &font, const float width, const float height)
        : font_(font), title_(localizedText(font, "En İyi 5 Skor", "En Iyi 5 Skor"), font, 46),
          subtitle_(localizedText(font, "Ad", "Ad"), font, 19),
          scoreHeader_(localizedText(font, "Skor", "Skor"), font, 19),
          emptyText_(localizedText(font, "Henüz skor yok. Oynamaya başla!",
                                   "Henuz skor yok. Oynamaya basla!"),
                     font, 20),
          backText_(localizedText(font, "GERİ", "GERI"), font, 24),
          backButton_(createButtonForText(backText_, kPrimaryButtonColor)),
          tableBox_({width - 36.f, height - 170.f}, kButtonCornerRadius, kRoundedCornerPointCount),
          width_(width), height_(height) {
        title_.setFillColor(sf::Color(45, 42, 36));
        centerTextOrigin(title_);
        title_.setPosition(width_ / 2.f, 64.f);

        tableBox_.setPosition(18.f, 102.f);
        tableBox_.setFillColor(sf::Color(241, 234, 220));
        tableBox_.setOutlineThickness(2.f);
        tableBox_.setOutlineColor(sf::Color(214, 200, 176));

        subtitle_.setFillColor(sf::Color(96, 86, 72));
        scoreHeader_.setFillColor(sf::Color(96, 86, 72));

        emptyText_.setFillColor(sf::Color(122, 112, 98));
        centerTextOrigin(emptyText_);
        emptyText_.setPosition(width_ / 2.f,
                               tableBox_.getPosition().y + tableBox_.getSize().y * 0.5f);

        backText_.setFillColor(sf::Color::White);
        centerTextOrigin(backText_);
        backButton_.setPosition(width_ / 2.f, height_ - 34.f);
        backText_.setPosition(backButton_.getPosition());
    }

    SceneCommand handleEvent(const sf::Event &event, const sf::RenderWindow &window) const {
        if (event.type == sf::Event::KeyPressed &&
            (event.key.code == sf::Keyboard::Escape || event.key.code == sf::Keyboard::Q ||
             event.key.code == sf::Keyboard::BackSpace)) {
            return SceneCommand::ShowSplash;
        }

        if (isPrimaryMouseRelease(event)) {
            const sf::Vector2f clickPos = cursorPosition(window);
            if (containsWithPadding(backButton_.getGlobalBounds(), clickPos)) {
                return SceneCommand::ShowSplash;
            }
        }
        return SceneCommand::None;
    }

    void updateHover(const sf::Vector2f &mousePos) {
        updateButtonColor(backButton_, backButton_.getGlobalBounds().contains(mousePos),
                          kPrimaryButtonColor, kPrimaryButtonHoverColor);
    }

    void render(sf::RenderWindow &window, const std::vector<core2048::ScoreEntry> &entries) {
        window.draw(title_);
        window.draw(tableBox_);

        const float left = tableBox_.getPosition().x;
        const float top = tableBox_.getPosition().y;
        const float width = tableBox_.getSize().x;
        const float tableRight = left + width - 12.f;
        const float scoreColumnRight = tableRight - 14.f;
        const float rowStartY = top + 48.f;
        const float rowHeight = 52.f;

        subtitle_.setPosition(left + 54.f, top + 16.f);
        window.draw(subtitle_);
        const auto scoreHeaderBounds = scoreHeader_.getLocalBounds();
        scoreHeader_.setOrigin(scoreHeaderBounds.left + scoreHeaderBounds.width,
                               scoreHeaderBounds.top);
        scoreHeader_.setPosition(scoreColumnRight, top + 16.f);
        window.draw(scoreHeader_);

        for (std::size_t i = 0; i < core2048::ScoreManager::kMaxEntries; ++i) {
            RoundedRectShape rowBg({width - 24.f, rowHeight - 8.f}, 10.f, kRoundedCornerPointCount);
            rowBg.setPosition(left + 12.f, rowStartY + static_cast<float>(i) * rowHeight);
            rowBg.setFillColor((i % 2U == 0U) ? sf::Color(249, 244, 236)
                                              : sf::Color(244, 237, 227));
            window.draw(rowBg);

            sf::Text rankText("#" + std::to_string(i + 1), font_, 20);
            rankText.setFillColor(sf::Color(94, 84, 72));
            rankText.setPosition(left + 24.f, rowStartY + static_cast<float>(i) * rowHeight + 10.f);
            window.draw(rankText);

            const bool hasEntry = i < entries.size();
            const std::string name = hasEntry ? entries[i].playerName : "-";
            const std::string score = hasEntry ? std::to_string(entries[i].score) : "-";

            sf::Text nameText(limitText(name, 15), font_, 22);
            nameText.setFillColor(sf::Color(58, 54, 48));
            nameText.setPosition(left + 72.f, rowStartY + static_cast<float>(i) * rowHeight + 8.f);
            window.draw(nameText);

            sf::Text scoreText(score, font_, 22);
            scoreText.setFillColor(sf::Color(58, 54, 48));
            const auto scoreBounds = scoreText.getLocalBounds();
            scoreText.setOrigin(scoreBounds.left + scoreBounds.width, scoreBounds.top);
            scoreText.setPosition(scoreColumnRight,
                                  rowStartY + static_cast<float>(i) * rowHeight + 8.f);
            window.draw(scoreText);
        }

        if (entries.empty()) {
            window.draw(emptyText_);
        }

        window.draw(backButton_);
        window.draw(backText_);
    }

  private:
    static sf::String limitText(const std::string &value, const std::size_t maxChars) {
        const sf::String unicode = sf::String::fromUtf8(value.begin(), value.end());
        if (unicode.getSize() <= maxChars) {
            return unicode;
        }
        sf::String truncated = unicode.substring(0, maxChars - 1U);
        truncated += '.';
        return truncated;
    }

    const sf::Font &font_;
    sf::Text title_;
    sf::Text subtitle_;
    sf::Text scoreHeader_;
    sf::Text emptyText_;
    sf::Text backText_;
    RoundedRectShape backButton_;
    RoundedRectShape tableBox_;
    float width_;
    float height_;
};

class PlayingScene {
  public:
    explicit PlayingScene(const sf::Font &font)
        : scoreText_("", font, 24), bestText_("", font, 20),
          menuButton_({46.f, 46.f}, kButtonCornerRadius, kRoundedCornerPointCount),
          menuPanel_({206.f, 112.f}, kButtonCornerRadius, kRoundedCornerPointCount),
          menuNewGameButton_({182.f, 40.f}, kButtonCornerRadius, kRoundedCornerPointCount),
          menuSoundButton_({182.f, 40.f}, kButtonCornerRadius, kRoundedCornerPointCount),
          menuNewGameText_(localizedText(font, "YENİ OYUN", "YENI OYUN"), font, 18),
          menuSoundText_("", font, 18) {
        scoreText_.setFillColor(sf::Color::Black);
        bestText_.setFillColor(sf::Color(40, 40, 40));

        menuButton_.setFillColor(kMenuButtonColor);
        menuButton_.setOutlineThickness(2.f);
        menuButton_.setOutlineColor(sf::Color::White);

        menuPanel_.setFillColor(kMenuPanelColor);
        menuPanel_.setOutlineThickness(2.f);
        menuPanel_.setOutlineColor(kMenuPanelOutlineColor);

        menuNewGameButton_.setFillColor(kPrimaryButtonColor);
        menuNewGameButton_.setOutlineThickness(2.f);
        menuNewGameButton_.setOutlineColor(sf::Color::White);

        menuSoundButton_.setOutlineThickness(2.f);
        menuSoundButton_.setOutlineColor(sf::Color::White);

        menuNewGameText_.setFillColor(sf::Color::White);
        centerTextOrigin(menuNewGameText_);

        menuSoundText_.setFillColor(sf::Color::White);
        setSoundEnabled(true);
    }

    SceneCommand handleEvent(const sf::Event &event, const sf::RenderWindow &window,
                             GameSession &session, app::SoundManager &soundManager,
                             const float width) {
        tickVisuals();
        layoutMenu(width);

        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
            resetVisualEffects();
            menuOpen_ = false;
            return SceneCommand::ShowSplash;
        }

        if (event.type == sf::Event::KeyPressed &&
            (event.key.code == sf::Keyboard::N || event.key.code == sf::Keyboard::Enter)) {
            resetVisualEffects();
            menuOpen_ = false;
            return SceneCommand::RestartGame;
        }

        if (isPrimaryMouseRelease(event)) {
            const sf::Vector2f clickPos = cursorPosition(window);
            if (containsWithPadding(menuButton_.getGlobalBounds(), clickPos)) {
                menuOpen_ = !menuOpen_;
                return SceneCommand::None;
            }

            if (menuOpen_) {
                if (containsWithPadding(menuNewGameButton_.getGlobalBounds(), clickPos)) {
                    resetVisualEffects();
                    menuOpen_ = false;
                    return SceneCommand::RestartGame;
                }
                if (containsWithPadding(menuSoundButton_.getGlobalBounds(), clickPos)) {
                    menuOpen_ = false;
                    return SceneCommand::ToggleSound;
                }
                if (!containsWithPadding(menuPanel_.getGlobalBounds(), clickPos)) {
                    menuOpen_ = false;
                }
            }
        }

        if (moveAnimationActive_) {
            return SceneCommand::None;
        }

        if (event.type != sf::Event::KeyPressed) {
            return SceneCommand::None;
        }

        const auto direction = mapDirection(event.key.code);
        if (!direction.has_value()) {
            return SceneCommand::None;
        }

        const auto beforeGrid = session.game().getGrid();
        const auto moveResult = session.applyMove(*direction);
        if (!moveResult.moved) {
            return SceneCommand::None;
        }

        soundManager.play(app::SoundEffect::TileSlide);
        if (moveResult.scoreDelta > 0) {
            soundManager.play(app::SoundEffect::Merge);
        }
        if (moveResult.spawnedTile.has_value()) {
            soundManager.play(app::SoundEffect::Spawn);
        }

        const auto plan = buildMoveVisualPlan(beforeGrid, *direction);
        startMoveVisuals(plan, moveResult);
        return SceneCommand::None;
    }

    void setSoundEnabled(const bool enabled) {
        soundEnabled_ = enabled;
        menuSoundText_.setString(localizedText(*menuSoundText_.getFont(),
                                               soundEnabled_ ? "SES AÇIK" : "SES KAPALI",
                                               soundEnabled_ ? "SES ACIK" : "SES KAPALI"));
        centerTextOrigin(menuSoundText_);
        menuSoundButton_.setFillColor(soundEnabled_ ? kSoundOnButtonColor : kSoundOffButtonColor);
    }

    void updateHover(const sf::Vector2f &mousePos, const float width) {
        layoutMenu(width);

        updateButtonColor(menuButton_, menuButton_.getGlobalBounds().contains(mousePos),
                          kMenuButtonColor, kMenuButtonHoverColor);

        if (!menuOpen_) {
            return;
        }

        updateButtonColor(menuNewGameButton_,
                          menuNewGameButton_.getGlobalBounds().contains(mousePos),
                          kPrimaryButtonColor, kPrimaryButtonHoverColor);
        updateButtonColor(menuSoundButton_, menuSoundButton_.getGlobalBounds().contains(mousePos),
                          soundEnabled_ ? kSoundOnButtonColor : kSoundOffButtonColor,
                          soundEnabled_ ? kSoundOnButtonHoverColor : kSoundOffButtonHoverColor);
    }

    void tickVisuals() {
        const auto now = Clock::now();
        updateMoveAnimationState(now);
        updateFloatingScores(now);
    }

    bool hasActiveAnimations() const {
        return moveAnimationActive_;
    }

    void resetVisualEffects() {
        moveAnimationActive_ = false;
        menuOpen_ = false;
        movingTiles_.clear();
        mergeCells_.clear();
        hiddenDuringSlide_.clear();
        hiddenDuringSpawn_.clear();
        spawnedTile_.reset();
        floatingScores_.clear();
    }

    void render(sf::RenderWindow &window, const sf::Font &font, GameSession &session,
                const float width, const int bestScore) {
        tickVisuals();
        layoutMenu(width);

        RoundedRectShape panelBg({width, static_cast<float>(kTopPanelHeight)}, kPanelCornerRadius,
                                 kRoundedCornerPointCount);
        panelBg.setFillColor(sf::Color(237, 224, 200));
        panelBg.setPosition(0.f, 0.f);
        window.draw(panelBg);

        const auto &game = session.game();
        scoreText_.setString("Skor: " + std::to_string(game.getScore()));
        const auto scoreBounds = scoreText_.getLocalBounds();
        scoreText_.setPosition(12.f, 10.f - scoreBounds.top);
        window.draw(scoreText_);

        sf::String bestLabel = localizedText(*bestText_.getFont(), "En İyi: ", "En Iyi: ");
        bestLabel += sf::String(std::to_string(bestScore));
        bestText_.setString(bestLabel);
        const auto bestBounds = bestText_.getLocalBounds();
        bestText_.setPosition(12.f, 40.f - bestBounds.top);
        window.draw(bestText_);

        renderFloatingScores(window, font);

        const auto now = Clock::now();
        const float elapsed = moveAnimationActive_
                                  ? std::chrono::duration<float>(now - moveAnimationStart_).count()
                                  : 0.f;
        const bool inSlideStage = moveAnimationActive_ && elapsed < kSlideAnimationDuration;
        const bool inSpawnStage = moveAnimationActive_ && spawnedTile_.has_value() &&
                                  elapsed >= kSlideAnimationDuration &&
                                  elapsed < (kSlideAnimationDuration + kSpawnFadeDuration);
        const bool hideSpawnTile = moveAnimationActive_ && spawnedTile_.has_value() &&
                                   elapsed < (kSlideAnimationDuration + kSpawnFadeDuration);
        const bool inMergeStage = moveAnimationActive_ && !mergeCells_.empty() &&
                                  elapsed >= kSlideAnimationDuration &&
                                  elapsed < (kSlideAnimationDuration + kMergePopDuration);

        const auto &grid = game.getGrid();
        for (int row = 0; row < kGridSize; ++row) {
            for (int col = 0; col < kGridSize; ++col) {
                const BoardCell cell{row, col};
                const sf::Vector2f center = cellCenter(cell);
                drawEmptyCell(window, center);

                const int value = grid[row][col];
                if (value == 0) {
                    continue;
                }

                bool hideValue = false;
                if (inSlideStage && hiddenDuringSlide_.contains(cell)) {
                    hideValue = true;
                }
                if (!hideValue && hideSpawnTile && hiddenDuringSpawn_.contains(cell)) {
                    hideValue = true;
                }
                if (hideValue) {
                    continue;
                }

                float scale = 1.f;
                if (inMergeStage) {
                    for (const auto &mergeCell : mergeCells_) {
                        if (mergeCell.cell.row == row && mergeCell.cell.col == col) {
                            const float mergeProgress =
                                clamp01((elapsed - kSlideAnimationDuration) / kMergePopDuration);
                            scale = 1.f + 0.17f * std::sin(mergeProgress * kPi);
                            break;
                        }
                    }
                }

                drawTile(window, font, value, center, scale);
            }
        }

        if (inSlideStage) {
            const float slideProgress = clamp01(elapsed / kSlideAnimationDuration);
            for (const auto &tile : movingTiles_) {
                const auto start = cellCenter(tile.from);
                const auto end = cellCenter(tile.to);
                drawTile(window, font, tile.value, lerp(start, end, slideProgress));
            }
        }

        if (inSpawnStage && spawnedTile_.has_value()) {
            const float spawnProgress =
                clamp01((elapsed - kSlideAnimationDuration) / kSpawnFadeDuration);
            const sf::Vector2f spawnCenter =
                cellCenter(BoardCell{spawnedTile_->row, spawnedTile_->col});
            drawTile(window, font, spawnedTile_->value, spawnCenter,
                     0.82f + (0.18f * spawnProgress), toAlpha(spawnProgress));
        }

        // Draw the menu as the top-most layer so tiles/animations cannot overlap it.
        window.draw(menuButton_);
        drawMenuIcon(window);
        if (menuOpen_) {
            window.draw(menuPanel_);
            window.draw(menuNewGameButton_);
            window.draw(menuNewGameText_);
            window.draw(menuSoundButton_);
            window.draw(menuSoundText_);
        }
    }

  private:
    struct FloatingScoreEffect {
        int scoreDelta;
        Clock::time_point startedAt;
    };

    void layoutMenu(const float width) {
        constexpr float panelPadding = 12.f;

        menuButton_.setPosition(width - menuButton_.getSize().x - panelPadding,
                                (kTopPanelHeight - menuButton_.getSize().y) * 0.5f);

        menuPanel_.setPosition(width - menuPanel_.getSize().x - panelPadding,
                               kTopPanelHeight + 8.f);

        const float itemX = menuPanel_.getPosition().x +
                            (menuPanel_.getSize().x - menuNewGameButton_.getSize().x) * 0.5f;
        menuNewGameButton_.setPosition(itemX, menuPanel_.getPosition().y + 10.f);
        menuSoundButton_.setPosition(itemX, menuPanel_.getPosition().y + 58.f);

        menuNewGameText_.setPosition(
            menuNewGameButton_.getPosition().x + menuNewGameButton_.getSize().x * 0.5f,
            menuNewGameButton_.getPosition().y + menuNewGameButton_.getSize().y * 0.5f);
        menuSoundText_.setPosition(
            menuSoundButton_.getPosition().x + menuSoundButton_.getSize().x * 0.5f,
            menuSoundButton_.getPosition().y + menuSoundButton_.getSize().y * 0.5f);
    }

    void drawMenuIcon(sf::RenderWindow &window) const {
        sf::RectangleShape line({20.f, 3.f});
        line.setFillColor(sf::Color::White);
        line.setOrigin(line.getSize().x * 0.5f, line.getSize().y * 0.5f);

        const sf::Vector2f center(menuButton_.getPosition().x + menuButton_.getSize().x * 0.5f,
                                  menuButton_.getPosition().y + menuButton_.getSize().y * 0.5f);

        line.setPosition(center.x, center.y - 8.f);
        window.draw(line);
        line.setPosition(center.x, center.y);
        window.draw(line);
        line.setPosition(center.x, center.y + 8.f);
        window.draw(line);
    }

    void startMoveVisuals(const MoveVisualPlan &plan, const core2048::MoveResult &result) {
        movingTiles_ = plan.movingTiles;
        mergeCells_ = plan.mergeCells;
        spawnedTile_ = result.spawnedTile;

        hiddenDuringSlide_.clear();
        for (const auto &tile : movingTiles_) {
            hiddenDuringSlide_.insert(tile.to);
        }
        if (spawnedTile_.has_value()) {
            hiddenDuringSlide_.insert(BoardCell{spawnedTile_->row, spawnedTile_->col});
            hiddenDuringSpawn_.clear();
            hiddenDuringSpawn_.insert(BoardCell{spawnedTile_->row, spawnedTile_->col});
        } else {
            hiddenDuringSpawn_.clear();
        }

        if (result.scoreDelta > 0) {
            floatingScores_.push_back(FloatingScoreEffect{result.scoreDelta, Clock::now()});
        }

        moveAnimationStart_ = Clock::now();
        moveAnimationActive_ = true;
    }

    void updateMoveAnimationState(const Clock::time_point now) {
        if (!moveAnimationActive_) {
            return;
        }

        const float elapsed = std::chrono::duration<float>(now - moveAnimationStart_).count();
        const float totalDuration = std::max(
            kSlideAnimationDuration + (mergeCells_.empty() ? 0.f : kMergePopDuration),
            kSlideAnimationDuration + (spawnedTile_.has_value() ? kSpawnFadeDuration : 0.f));

        if (elapsed < totalDuration) {
            return;
        }

        moveAnimationActive_ = false;
        movingTiles_.clear();
        mergeCells_.clear();
        hiddenDuringSlide_.clear();
        hiddenDuringSpawn_.clear();
        spawnedTile_.reset();
    }

    void updateFloatingScores(const Clock::time_point now) {
        floatingScores_.erase(
            std::remove_if(floatingScores_.begin(), floatingScores_.end(),
                           [&](const auto &effect) {
                               const float elapsed =
                                   std::chrono::duration<float>(now - effect.startedAt).count();
                               return elapsed >= kFloatingScoreDuration;
                           }),
            floatingScores_.end());
    }

    void renderFloatingScores(sf::RenderWindow &window, const sf::Font &font) {
        const auto now = Clock::now();

        for (const auto &effect : floatingScores_) {
            const float elapsed = std::chrono::duration<float>(now - effect.startedAt).count();
            const float progress = clamp01(elapsed / kFloatingScoreDuration);

            sf::Text scoreDeltaText("+" + std::to_string(effect.scoreDelta), font, 20);
            auto color = sf::Color(92, 163, 80);
            color.a = toAlpha(1.f - progress);
            scoreDeltaText.setFillColor(color);
            centerTextOrigin(scoreDeltaText);
            scoreDeltaText.setPosition(126.f, 20.f - 22.f * progress);
            window.draw(scoreDeltaText);
        }
    }

    sf::Text scoreText_;
    sf::Text bestText_;
    RoundedRectShape menuButton_;
    RoundedRectShape menuPanel_;
    RoundedRectShape menuNewGameButton_;
    RoundedRectShape menuSoundButton_;
    sf::Text menuNewGameText_;
    sf::Text menuSoundText_;
    bool menuOpen_{false};
    bool soundEnabled_{true};

    bool moveAnimationActive_{false};
    Clock::time_point moveAnimationStart_{};
    std::vector<MovingTileVisual> movingTiles_;
    std::vector<MergeCellVisual> mergeCells_;
    std::set<BoardCell> hiddenDuringSlide_;
    std::set<BoardCell> hiddenDuringSpawn_;
    std::optional<core2048::SpawnedTile> spawnedTile_;
    std::vector<FloatingScoreEffect> floatingScores_;
};

class GameOverScene {
  public:
    GameOverScene(const sf::Font &font, const float width, const float height)
        : overlay_({width, height}),
          box_({390.f, 270.f}, kButtonCornerRadius, kRoundedCornerPointCount),
          title_("Oyun Bitti", font, 40), scoreText_("", font, 26), bestText_("", font, 22),
          newGameText_(localizedText(font, "YENİ OYUN", "YENI OYUN"), font, 22),
          newGameButton_(createButtonForText(newGameText_, kPrimaryButtonColor)),
          quitText_(localizedText(font, "ÇIKIŞ", "CIKIS"), font, 22),
          quitButton_(createButtonForText(quitText_, kDangerButtonColor)), width_(width),
          height_(height) {
        overlay_.setFillColor(sf::Color(0, 0, 0, 150));

        box_.setFillColor(sf::Color::White);
        box_.setOutlineThickness(3.f);
        box_.setOutlineColor(sf::Color(200, 0, 0));
        box_.setOrigin(box_.getSize() / 2.f);

        title_.setFillColor(sf::Color::Red);
        title_.setStyle(sf::Text::Bold);
        centerTextOrigin(title_);

        scoreText_.setFillColor(sf::Color(30, 30, 30));
        scoreText_.setStyle(sf::Text::Bold);

        bestText_.setFillColor(sf::Color(35, 35, 35));
        bestText_.setStyle(sf::Text::Bold);

        newGameText_.setFillColor(sf::Color::White);
        centerTextOrigin(newGameText_);

        quitText_.setFillColor(sf::Color::White);
        centerTextOrigin(quitText_);

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

        if (isPrimaryMouseRelease(event)) {
            const sf::Vector2f clickPos = cursorPosition(window);
            if (containsWithPadding(newGameButton_.getGlobalBounds(), clickPos)) {
                return SceneCommand::RestartGame;
            }
            if (containsWithPadding(quitButton_.getGlobalBounds(), clickPos)) {
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

    void render(sf::RenderWindow &window, const int score, const int bestScore) {
        window.draw(overlay_);
        window.draw(box_);

        title_.setPosition(width_ / 2.f, height_ / 2.f - 92.f);
        window.draw(title_);

        scoreText_.setString("Son Skor: " + std::to_string(score));
        centerTextOrigin(scoreText_);
        scoreText_.setPosition(width_ / 2.f, height_ / 2.f - 48.f);
        window.draw(scoreText_);

        sf::String bestLabel = localizedText(*bestText_.getFont(), "En İyi: ", "En Iyi: ");
        bestLabel += sf::String(std::to_string(bestScore));
        bestText_.setString(bestLabel);
        centerTextOrigin(bestText_);
        bestText_.setPosition(width_ / 2.f, height_ / 2.f - 16.f);
        window.draw(bestText_);

        window.draw(newGameButton_);
        window.draw(newGameText_);
        window.draw(quitButton_);
        window.draw(quitText_);
    }

  private:
    void layout() {
        box_.setPosition(width_ / 2.f, height_ / 2.f - 4.f);

        newGameButton_.setPosition(width_ / 2.f - 98.f, height_ / 2.f + 74.f);
        newGameText_.setPosition(newGameButton_.getPosition());

        quitButton_.setPosition(width_ / 2.f + 98.f, height_ / 2.f + 74.f);
        quitText_.setPosition(quitButton_.getPosition());
    }

    sf::RectangleShape overlay_;
    RoundedRectShape box_;
    sf::Text title_;
    sf::Text scoreText_;
    sf::Text bestText_;
    sf::Text newGameText_;
    RoundedRectShape newGameButton_;
    sf::Text quitText_;
    RoundedRectShape quitButton_;
    float width_;
    float height_;
};

void applySceneCommand(const SceneCommand command, SceneId &scene, GameSession &session,
                       sf::RenderWindow &window) {
    switch (command) {
    case SceneCommand::None:
        return;
    case SceneCommand::StartGame:
    case SceneCommand::RestartGame:
        session.resetGame();
        scene = SceneId::Playing;
        return;
    case SceneCommand::ShowHighScores:
        scene = SceneId::HighScores;
        return;
    case SceneCommand::ShowSplash:
        scene = SceneId::Splash;
        return;
    case SceneCommand::ToggleSound:
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

    sf::ContextSettings windowSettings;
    windowSettings.antialiasingLevel = kWindowAntialiasingLevel;

    sf::RenderWindow window(sf::VideoMode(width, height), "2048",
                            sf::Style::Titlebar | sf::Style::Close, windowSettings);
    window.setVerticalSyncEnabled(config.vSyncEnabled);
    if (config.frameLimit.has_value()) {
        window.setFramerateLimit(*config.frameLimit);
    }
    window.requestFocus();

    const auto fontResolution = resolveAssetPath(kFontRelativePath);

    sf::Font font;
    if (!fontResolution.resolvedPath.has_value() ||
        !font.loadFromFile(fontResolution.resolvedPath->string())) {
        std::cerr << "Font yüklenemedi. Denenen yollar:\n";
        for (const auto &candidate : fontResolution.candidates) {
            std::cerr << "  - " << candidate.string() << "\n";
        }
        return 1;
    }

    const auto soundDirResolution = resolveAssetPath(kSoundsRelativePath);
    app::SoundManager soundManager(
        soundDirResolution.resolvedPath.value_or(std::filesystem::path(kSoundsRelativePath)),
        resolveSettingsFilePath());
    if (!soundManager.loadSettings()) {
        std::cerr << "Uyarı: ayar dosyası yüklenemedi: " << soundManager.settingsFilePath() << "\n";
    }
    if (!soundManager.loadSoundAssets()) {
        std::cerr << "Uyarı: bazı ses dosyaları yüklenemedi:\n";
        for (const auto &missing : soundManager.missingFiles()) {
            std::cerr << "  - " << missing.string() << "\n";
        }
    }

    core2048::ScoreManager scoreManager(resolveScoreFilePath());
    if (!scoreManager.load()) {
        std::cerr << "Uyarı: skor dosyası yüklenemedi: " << scoreManager.scoreFilePath() << "\n";
    }
    int bestScore = scoreManager.bestScore();
    bool finalScorePersisted = false;

    GameSession session;
    SplashScene splashScene(font, static_cast<float>(width), static_cast<float>(height));
    HighScoresScene highScoresScene(font, static_cast<float>(width), static_cast<float>(height));
    PlayingScene playingScene(font);
    playingScene.setSoundEnabled(soundManager.isEnabled());
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
                if (command == SceneCommand::StartGame) {
                    session.setPlayerName(splashScene.playerName());
                }
                break;
            case SceneId::HighScores:
                command = highScoresScene.handleEvent(event, window);
                break;
            case SceneId::Playing:
                command = playingScene.handleEvent(event, window, session, soundManager,
                                                   static_cast<float>(width));
                break;
            case SceneId::GameOver:
                command = gameOverScene.handleEvent(event, window);
                break;
            }

            applySceneCommand(command, scene, session, window);

            if (command == SceneCommand::StartGame || command == SceneCommand::RestartGame) {
                finalScorePersisted = false;
                playingScene.resetVisualEffects();
            }
            if (command == SceneCommand::ToggleSound) {
                soundManager.toggleEnabled();
                if (!soundManager.saveSettings()) {
                    std::cerr << "Uyarı: ayar dosyası kaydedilemedi: "
                              << soundManager.settingsFilePath() << "\n";
                }
                playingScene.setSoundEnabled(soundManager.isEnabled());
            }
            if (command == SceneCommand::ShowSplash) {
                playingScene.resetVisualEffects();
            }
        }

        if (!window.isOpen()) {
            break;
        }

        const sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        if (scene == SceneId::Splash) {
            splashScene.updateHover(mousePos);
        } else if (scene == SceneId::HighScores) {
            highScoresScene.updateHover(mousePos);
        } else if (scene == SceneId::Playing) {
            playingScene.updateHover(mousePos, static_cast<float>(width));
            playingScene.tickVisuals();
        } else if (scene == SceneId::GameOver) {
            gameOverScene.updateHover(mousePos);
        }

        if (scene == SceneId::Playing && session.game().isGameOver() &&
            !playingScene.hasActiveAnimations()) {
            if (!finalScorePersisted) {
                const bool isNewBest = session.game().getScore() > bestScore;
                scoreManager.addScore(session.game().getScore(), session.playerName());
                if (!scoreManager.save()) {
                    std::cerr << "Uyarı: skor dosyası kaydedilemedi: "
                              << scoreManager.scoreFilePath() << "\n";
                }
                bestScore = scoreManager.bestScore();
                finalScorePersisted = true;
                soundManager.play(app::SoundEffect::GameOver);
                if (isNewBest) {
                    soundManager.play(app::SoundEffect::HighScore);
                }
            }
            scene = SceneId::GameOver;
        }

        window.clear(kBoardBackgroundColor);

        if (scene == SceneId::Splash) {
            splashScene.render(window);
            window.display();
            continue;
        }

        if (scene == SceneId::HighScores) {
            highScoresScene.render(window, scoreManager.topScores());
            window.display();
            continue;
        }

        playingScene.render(window, font, session, static_cast<float>(width), bestScore);
        if (scene == SceneId::GameOver) {
            gameOverScene.render(window, session.game().getScore(), bestScore);
        }

        window.display();
    }

    return 0;
}

} // namespace app
