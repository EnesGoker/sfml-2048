#pragma once

#include <cstdint>
#include <optional>
#include <filesystem>

namespace app {

struct RunConfig {
    bool vSyncEnabled{true};
    std::optional<unsigned int> frameLimit;
    // Programmatic isolation for acceptance fixtures; no native lookup when supplied.
    std::optional<std::filesystem::path> userDataOverride;
    // Final read-only assets directory; bypasses executable/bundle discovery.
    std::optional<std::filesystem::path> assetOverride;
    // Defaults to one startup CWD snapshot; isolates legacy sources in fixtures.
    std::optional<std::filesystem::path> legacyDataOverride;
};

int run(const RunConfig &config = {});

} // namespace app
