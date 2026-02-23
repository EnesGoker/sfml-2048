#include "app/SoundManager.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <system_error>

namespace app {

namespace {

using Json = nlohmann::json;

} // namespace

SoundManager::SoundManager(std::filesystem::path soundsDirectory,
                           std::filesystem::path settingsFilePath)
    : soundsDirectory_(std::move(soundsDirectory)), settingsFilePath_(std::move(settingsFilePath)) {
}

bool SoundManager::loadSettings() {
    std::error_code ec;
    if (!std::filesystem::exists(settingsFilePath_, ec)) {
        return !ec;
    }
    if (ec) {
        return false;
    }

    std::ifstream in(settingsFilePath_);
    if (!in.is_open()) {
        return false;
    }

    Json root;
    try {
        in >> root;
    } catch (const Json::parse_error &) {
        return false;
    }

    if (!root.is_object()) {
        return false;
    }

    if (root.contains("sound_enabled")) {
        if (!root["sound_enabled"].is_boolean()) {
            return false;
        }
        enabled_ = root["sound_enabled"].get<bool>();
    }

    return true;
}

bool SoundManager::saveSettings() const {
    const auto parent = settingsFilePath_.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            return false;
        }
    }

    Json root;
    root["sound_enabled"] = enabled_;

    std::ofstream out(settingsFilePath_);
    if (!out.is_open()) {
        return false;
    }

    out << root.dump(2) << '\n';
    return static_cast<bool>(out);
}

bool SoundManager::loadSoundAssets() {
    missingFiles_.clear();

    for (const SoundEffect effect : {SoundEffect::TileSlide, SoundEffect::Merge, SoundEffect::Spawn,
                                     SoundEffect::GameOver, SoundEffect::HighScore}) {
        auto &audio = audioForEffect(effect);
        audio.loaded = false;

        const auto path = soundsDirectory_ / effectFileName(effect);
        if (!audio.buffer.loadFromFile(path.string())) {
            missingFiles_.push_back(path);
            continue;
        }

        audio.sound.setBuffer(audio.buffer);
        audio.sound.setVolume(effectVolume(effect));
        audio.loaded = true;
    }

    return missingFiles_.empty();
}

void SoundManager::play(const SoundEffect effect) {
    if (!enabled_) {
        return;
    }

    auto &audio = audioForEffect(effect);
    if (!audio.loaded) {
        return;
    }

    audio.sound.stop();
    audio.sound.play();
}

bool SoundManager::isEnabled() const noexcept {
    return enabled_;
}

void SoundManager::setEnabled(const bool enabled) noexcept {
    enabled_ = enabled;
}

void SoundManager::toggleEnabled() noexcept {
    enabled_ = !enabled_;
}

const std::filesystem::path &SoundManager::settingsFilePath() const noexcept {
    return settingsFilePath_;
}

const std::vector<std::filesystem::path> &SoundManager::missingFiles() const noexcept {
    return missingFiles_;
}

const char *SoundManager::effectFileName(const SoundEffect effect) {
    switch (effect) {
    case SoundEffect::TileSlide:
        return "slide.wav";
    case SoundEffect::Merge:
        return "merge.wav";
    case SoundEffect::Spawn:
        return "spawn.wav";
    case SoundEffect::GameOver:
        return "game_over.wav";
    case SoundEffect::HighScore:
        return "high_score.wav";
    }

    return "unknown.wav";
}

float SoundManager::effectVolume(const SoundEffect effect) {
    switch (effect) {
    case SoundEffect::TileSlide:
        return 35.f;
    case SoundEffect::Merge:
        return 50.f;
    case SoundEffect::Spawn:
        return 38.f;
    case SoundEffect::GameOver:
        return 58.f;
    case SoundEffect::HighScore:
        return 62.f;
    }

    return 45.f;
}

SoundManager::EffectAudio &SoundManager::audioForEffect(const SoundEffect effect) {
    switch (effect) {
    case SoundEffect::TileSlide:
        return tileSlide_;
    case SoundEffect::Merge:
        return merge_;
    case SoundEffect::Spawn:
        return spawn_;
    case SoundEffect::GameOver:
        return gameOver_;
    case SoundEffect::HighScore:
        return highScore_;
    }

    return tileSlide_;
}

const SoundManager::EffectAudio &SoundManager::audioForEffect(const SoundEffect effect) const {
    switch (effect) {
    case SoundEffect::TileSlide:
        return tileSlide_;
    case SoundEffect::Merge:
        return merge_;
    case SoundEffect::Spawn:
        return spawn_;
    case SoundEffect::GameOver:
        return gameOver_;
    case SoundEffect::HighScore:
        return highScore_;
    }

    return tileSlide_;
}

} // namespace app
