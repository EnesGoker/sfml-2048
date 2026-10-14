#pragma once

#include "app/AppPaths.hpp"
#include "core/FileStorage.hpp"
#include "core/detail/FileSnapshot.hpp"

#include <functional>
#include <memory>

namespace app {

// Capture the pre-upgrade working directory once at startup, outside path resolution.
// A test override uses the same structure and must be absolute/nonempty/no-NUL.
struct LegacyDataLocation {
    std::optional<std::filesystem::path> directory;
    std::error_code error;
};
LegacyDataLocation captureLegacyDataLocation();

enum class ProfileImportStatus {
    DestinationPresent,
    SourceMissing,
    Imported,
    InvalidLegacyLocation,
    DestinationIoError,
    SourceIoError,
    InvalidSource,
    UnsupportedVersion,
    SourceChanged,
    PublicationFailed,
};

struct ProfileImportResult {
    ProfileImportStatus status;
    std::filesystem::path source;
    std::filesystem::path destination;
    std::optional<core2048::FileSaveStatus> saveStatus;
    std::error_code systemError;

    // An unresolved import protects this profile for the current process. Successful
    // profiles and confirmed source absence remain independent and usable.
    bool allowsProfileWrites() const noexcept;
};

struct ProfileMigrationResult {
    ProfileImportResult scores;
    ProfileImportResult settings;
};

// Injection seam for deterministic source changes/read failures; production uses
// the native reader, which rejects symlinks and non-regular files.
using LegacySnapshotReader =
    std::function<core2048::detail::FileReadResult(const std::filesystem::path &)>;

// Import validated source bytes into confirmed-absent targets using atomic no-overwrite
// publication. Sources are never modified/deleted. Existing targets are authoritative,
// regardless of validity; score/settings format upgrades happen in their normal loaders.
ProfileMigrationResult
migrateLegacyProfile(const UserDataPaths &paths, const LegacyDataLocation &legacy,
                     std::shared_ptr<core2048::FileStorage> storage = nullptr,
                     const LegacySnapshotReader &reader = core2048::detail::readFileSnapshot);

// Ordinary storage for successful/unneeded imports; a memory-only write barrier after
// an unresolved import. No mid-round reload/import retry can erase dirty memory.
std::shared_ptr<core2048::FileStorage> storageAfterProfileImport(const ProfileImportResult &result);

} // namespace app
