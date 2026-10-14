#include "app/ProfileMigration.hpp"
#include "app/detail/SettingsValidation.hpp"
#include "core/detail/ProfileValidation.hpp"

#include <stdexcept>
#include <utility>

namespace app {
namespace {

enum class DestinationState { Missing, Present, IoError };

DestinationState inspectDestination(const std::filesystem::path &path, std::error_code &error) {
    const auto status = std::filesystem::symlink_status(path, error);
    if (status.type() == std::filesystem::file_type::not_found &&
        (!error || error == std::errc::no_such_file_or_directory)) {
        error.clear();
        return DestinationState::Missing;
    }
    if (error) {
        return DestinationState::IoError;
    }
    // This includes corrupt/unknown files, directories and dangling symlinks. The
    // individual canonical loader owns their validation and write protection.
    return DestinationState::Present;
}

ProfileImportStatus validateSnapshot(const std::string &bytes, bool scores) {
    if (scores) {
        const auto status = core2048::detail::validateScoreProfile(bytes);
        if (status == core2048::ScoreLoadStatus::LoadedV2 ||
            status == core2048::ScoreLoadStatus::LegacyMigrationPending) {
            return ProfileImportStatus::Imported;
        }
        if (status == core2048::ScoreLoadStatus::UnsupportedVersion) {
            return ProfileImportStatus::UnsupportedVersion;
        }
    } else {
        const auto status = detail::validateSettingsProfile(bytes);
        if (status == SettingsLoadStatus::LoadedV1 ||
            status == SettingsLoadStatus::LegacyMigrationPending) {
            return ProfileImportStatus::Imported;
        }
        if (status == SettingsLoadStatus::UnsupportedVersion) {
            return ProfileImportStatus::UnsupportedVersion;
        }
    }
    return ProfileImportStatus::InvalidSource;
}

ProfileImportResult importFile(const std::filesystem::path &destination,
                               const LegacyDataLocation &legacy, bool scores,
                               core2048::FileStorage &storage, const LegacySnapshotReader &reader) {
    ProfileImportResult result{ProfileImportStatus::DestinationIoError, {}, destination, {}, {}};
    if (legacy.directory) {
        result.source = *legacy.directory / (scores ? "scores.json" : "settings.json");
    }
    const auto target = inspectDestination(destination, result.systemError);
    if (target == DestinationState::Present) {
        result.status = ProfileImportStatus::DestinationPresent;
        return result;
    }
    if (target == DestinationState::IoError) {
        return result;
    }

    result.status = ProfileImportStatus::InvalidLegacyLocation;
    if (legacy.error) {
        result.systemError = legacy.error;
        return result;
    }
    if (!legacy.directory) {
        return result;
    }
    PathOptions options;
    options.userDataOverride = legacy.directory;
    const auto location = resolveUserDataPaths({}, options);
    if (location.error) {
        result.systemError = location.error->systemError;
        return result;
    }

    result.status = ProfileImportStatus::SourceIoError;
    if (!reader) {
        return result;
    }
    core2048::detail::FileReadResult source;
    try {
        source = reader(result.source);
    } catch (const std::runtime_error &) {
        return result;
    }
    using core2048::detail::FileReadStatus;
    if (source.status == FileReadStatus::Missing) {
        result.status = ProfileImportStatus::SourceMissing;
        return result;
    }
    if (source.status == FileReadStatus::Invalid) {
        result.status = ProfileImportStatus::InvalidSource;
        return result;
    }
    if (source.status != FileReadStatus::Read) {
        return result;
    }
    result.status = validateSnapshot(source.bytes, scores);
    if (result.status != ProfileImportStatus::Imported) {
        return result;
    }

    // Validate the exact captured bytes, then detect observed changes before copy.
    // The legacy file remains live: this is not cross-process locking or a promise
    // to capture changes made after this final observation.
    core2048::detail::FileReadResult current;
    try {
        current = reader(result.source);
    } catch (const std::runtime_error &) {
        result.status = ProfileImportStatus::SourceIoError;
        return result;
    }
    if (current.status == FileReadStatus::IoError) {
        result.status = ProfileImportStatus::SourceIoError;
        return result;
    }
    if (current.status != FileReadStatus::Read || current.bytes != source.bytes) {
        result.status = ProfileImportStatus::SourceChanged;
        return result;
    }

    result.status = ProfileImportStatus::PublicationFailed;
    try {
        result.saveStatus = storage.save(destination, source.bytes, std::nullopt, false);
    } catch (const std::runtime_error &) {
        result.saveStatus = core2048::FileSaveStatus::IoError;
    }
    if (result.saveStatus == core2048::FileSaveStatus::Saved) {
        result.status = ProfileImportStatus::Imported;
        return result;
    }
    // Another writer may have populated the target after the initial check. Never
    // merge/retry over it: let the canonical loader validate that authoritative file.
    std::error_code error;
    if (inspectDestination(destination, error) == DestinationState::Present) {
        result.status = ProfileImportStatus::DestinationPresent;
    }
    return result;
}

class ImportWriteBarrier final : public core2048::FileStorage {
  public:
    core2048::FileSaveStatus save(const std::filesystem::path &, const std::string &,
                                  const std::optional<std::string> &, bool) override {
        return core2048::FileSaveStatus::WriteBlocked;
    }
};

} // namespace

LegacyDataLocation captureLegacyDataLocation() {
    std::error_code error;
    auto directory = std::filesystem::current_path(error);
    if (error) {
        return {std::nullopt, error};
    }
    return {std::move(directory), {}};
}

bool ProfileImportResult::allowsProfileWrites() const noexcept {
    return status == ProfileImportStatus::DestinationPresent ||
           status == ProfileImportStatus::SourceMissing || status == ProfileImportStatus::Imported;
}

ProfileMigrationResult migrateLegacyProfile(const UserDataPaths &paths,
                                            const LegacyDataLocation &legacy,
                                            std::shared_ptr<core2048::FileStorage> storage,
                                            const LegacySnapshotReader &reader) {
    if (!storage) {
        storage = core2048::makeFileStorage();
    }
    // Each file has its own decision and commit point. No global completion marker
    // can cause a successful settings import to suppress a failed score retry.
    auto scores = importFile(paths.scoresFile(), legacy, true, *storage, reader);
    auto settings = importFile(paths.settingsFile(), legacy, false, *storage, reader);
    return {std::move(scores), std::move(settings)};
}

std::shared_ptr<core2048::FileStorage>
storageAfterProfileImport(const ProfileImportResult &result) {
    if (result.allowsProfileWrites()) {
        return core2048::makeFileStorage();
    }
    return std::make_shared<ImportWriteBarrier>();
}

} // namespace app
