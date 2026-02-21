#include "app/AssetResolver.hpp"

#include <array>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace {

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
buildCandidatePaths(const std::filesystem::path &relativeAssetPath) {
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

} // namespace

namespace app {

AssetResolution resolveAssetPath(const std::filesystem::path &relativeAssetPath) {
    AssetResolution resolution;
    resolution.candidates = buildCandidatePaths(relativeAssetPath);
    resolution.resolvedPath = findFirstExistingPath(resolution.candidates);
    return resolution;
}

} // namespace app
