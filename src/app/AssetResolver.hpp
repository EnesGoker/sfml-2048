#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace app {

struct AssetResolution {
    std::optional<std::filesystem::path> resolvedPath;
    std::vector<std::filesystem::path> candidates;
};

AssetResolution resolveAssetPath(const std::filesystem::path &relativeAssetPath);

} // namespace app
