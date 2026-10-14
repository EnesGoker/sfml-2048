#include "app/ProfileMigration.hpp"
#include "app/SettingsStore.hpp"
#include "core/ScoreManager.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;
using app::LegacyDataLocation;
using app::ProfileImportStatus;
using core2048::FileSaveStatus;
using core2048::detail::FileReadResult;
using core2048::detail::FileReadStatus;

class TempTree {
  public:
    TempTree() {
        static std::atomic<unsigned> sequence{0};
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 64; ++attempt) {
            root_ = fs::temp_directory_path() / ("sfml_2048_import_" + std::to_string(timestamp) +
                                                 "_" + std::to_string(sequence.fetch_add(1)));
            std::error_code error;
            if (fs::create_directory(root_, error)) {
                return;
            }
            if (error && error != std::errc::file_exists) {
                throw fs::filesystem_error("create temporary migration fixture", root_, error);
            }
        }
        throw std::runtime_error("could not claim a unique migration fixture");
    }

    ~TempTree() {
        std::error_code ignored;
        fs::remove_all(root_, ignored);
    }

    TempTree(const TempTree &) = delete;
    TempTree &operator=(const TempTree &) = delete;

    const fs::path &root() const noexcept {
        return root_;
    }

    fs::path directory(const fs::path &relative) const {
        const auto path = root_ / relative;
        fs::create_directories(path);
        return path;
    }

    fs::path file(const fs::path &relative, std::string_view contents) const {
        const auto path = root_ / relative;
        fs::create_directories(path.parent_path());
        write(path, contents);
        return path;
    }

    static void write(const fs::path &path, std::string_view contents) {
        std::ofstream output(path, std::ios::binary);
        output.exceptions(std::ios::failbit | std::ios::badbit);
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        output.close();
    }

  private:
    fs::path root_;
};

std::string readBytes(const fs::path &path) {
    std::ifstream input(path, std::ios::binary);
    input.exceptions(std::ios::badbit);
    if (!input) {
        throw std::runtime_error("could not read migration fixture");
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string utf8Bytes(std::u8string_view text) {
    return {reinterpret_cast<const char *>(text.data()), text.size()};
}

// Raw, noncanonical source bytes deliberately include UTF-8 and CRLF formatting.
const std::string kLegacyScores = utf8Bytes(
    u8"{\r\n  \"scores\": [\r\n"
    u8"    {\"score\":128,\"played_at\":\"eski tarih\",\"player_name\":\"İnci 🎮\",\"seed\":7},\r\n"
    u8"    {\"score\":512,\"played_at\":\"başka tarih\",\"player_name\":\"用户\"}\r\n"
    u8"  ]\r\n}\r\n \r\n");
const std::string kV2Scores =
    utf8Bytes(u8"{\r\n \"version\":2,\"best_score\":2048,\"completed_games\":[\r\n"
              u8" {\"game_id\":\"round-one\",\"score\":256,\"played_at\":\"2026-10-06T09:00:00Z\","
              u8"\"player_name\":\"İnci 🎮\"}\r\n ]\r\n}\r\n");
const std::string kLegacySettings = "{\r\n  \"sound_enabled\": false\r\n}\r\n \r\n";
const std::string kV1Settings = "{\r\n \"version\":1, \"sound_enabled\":false\r\n}\r\n";
const std::string kConcurrentScores =
    "{\"version\":2,\"best_score\":65536,\"completed_games\":[{\"game_id\":\"foreign-round\","
    "\"score\":1024,\"played_at\":\"2026-10-06T11:00:00Z\",\"player_name\":\"Other\"}]}\n";

app::UserDataPaths profilePaths(const TempTree &tree,
                                const fs::path &relative = fs::path("profile")) {
    app::PathOptions options;
    options.userDataOverride = tree.directory(relative);
    const auto result = app::resolveUserDataPaths({}, options);
    REQUIRE(result.paths.has_value());
    REQUIRE_FALSE(result.error.has_value());
    return *result.paths;
}

LegacyDataLocation legacyLocation(const TempTree &tree,
                                  const fs::path &relative = fs::path("legacy")) {
    return {tree.directory(relative), {}};
}

fs::path backupPath(const fs::path &path) {
    auto result = path;
    result += ".bak";
    return result;
}

fs::path embeddedNullPath(const TempTree &tree) {
    auto native = (tree.root() / "before-null").native();
    native.push_back(fs::path::value_type{});
    native += fs::path("after-null").native();
    return fs::path(native);
}

struct SnapshotEntry {
    fs::path path;
    fs::file_type type;
    fs::file_time_type modified;
    std::string bytes;

    bool operator==(const SnapshotEntry &) const = default;
};

std::vector<SnapshotEntry> snapshotTree(const fs::path &root) {
    std::vector<SnapshotEntry> snapshot;
    const auto capture = [&](const fs::path &path) {
        const auto status = fs::status(path);
        snapshot.push_back({path.lexically_relative(root), status.type(), fs::last_write_time(path),
                            fs::is_regular_file(status) ? readBytes(path) : std::string{}});
    };
    capture(root);
    for (const auto &entry : fs::recursive_directory_iterator(root)) {
        capture(entry.path());
    }
    std::sort(snapshot.begin(), snapshot.end(),
              [](const auto &left, const auto &right) { return left.path < right.path; });
    return snapshot;
}

void requireImport(const app::ProfileImportResult &result, ProfileImportStatus status,
                   const fs::path &source, const fs::path &destination, bool allowsWrites) {
    REQUIRE(result.status == status);
    REQUIRE(result.source == source);
    REQUIRE(result.destination == destination);
    REQUIRE(result.allowsProfileWrites() == allowsWrites);
}

struct SaveCall {
    fs::path path;
    std::string bytes;
    std::optional<std::string> expectedSource;
    bool backupLegacy;
};

class ControlledFileStorage final : public core2048::FileStorage {
  public:
    std::vector<SaveCall> calls;
    std::optional<fs::path> failedPath;
    std::optional<FileSaveStatus> failure;
    std::function<void(const SaveCall &)> beforeFirstSave;

    FileSaveStatus save(const fs::path &path, const std::string &serialized,
                        const std::optional<std::string> &expectedSource,
                        bool backupLegacy) override {
        calls.push_back({path, serialized, expectedSource, backupLegacy});
        if (beforeFirstSave) {
            auto callback = std::exchange(beforeFirstSave, std::function<void(const SaveCall &)>{});
            callback(calls.back());
        }
        if (failure && (!failedPath || *failedPath == path)) {
            return *failure;
        }
        return native_->save(path, serialized, expectedSource, backupLegacy);
    }

  private:
    std::shared_ptr<core2048::FileStorage> native_{core2048::makeFileStorage()};
};

void requireScoreWritesBlocked(const fs::path &destination,
                               const app::ProfileImportResult &import) {
    core2048::ScoreManager scores(destination, app::storageAfterProfileImport(import));
    REQUIRE(scores.load());
    REQUIRE(scores.updateBestScore(8192));
    REQUIRE(scores.recordCompletedGame("unsaved-current-round", 4096, "Oyuncu",
                                       "2026-10-06T10:00:00Z"));
    for (int attempt = 0; attempt < 2; ++attempt) {
        REQUIRE_FALSE(scores.save());
        REQUIRE(scores.lastSaveStatus() == FileSaveStatus::WriteBlocked);
        REQUIRE(scores.isDirty());
        REQUIRE(scores.bestScore() == 8192);
        REQUIRE(scores.completedGames().size() == 1);
    }
    REQUIRE_FALSE(fs::exists(destination));
}

void requireSettingsWritesBlocked(const fs::path &destination,
                                  const app::ProfileImportResult &import) {
    app::SettingsStore settings(destination, app::storageAfterProfileImport(import));
    REQUIRE(settings.load());
    REQUIRE(settings.setSoundEnabled(false));
    REQUIRE_FALSE(settings.save());
    REQUIRE(settings.lastSaveStatus() == FileSaveStatus::WriteBlocked);
    REQUIRE(settings.isDirty());
    REQUIRE_FALSE(settings.soundEnabled());
    REQUIRE_FALSE(fs::exists(destination));
}

#ifndef _WIN32
void fileSymlink(const fs::path &target, const fs::path &link) {
    std::error_code error;
    fs::create_symlink(target, link, error);
    if (error) {
        SKIP("File symlinks unavailable: " << error.message());
    }
}
#endif

} // namespace

TEST_CASE("migration copies all supported score and settings formats as exact raw bytes",
          "[profile-migration]") {
    const std::array<std::string, 2> scoreSources{kLegacyScores, kV2Scores};
    const std::array<std::string, 2> settingsSources{kLegacySettings, kV1Settings};
    for (const auto &scoreBytes : scoreSources) {
        for (const auto &settingBytes : settingsSources) {
            TempTree tree;
            const auto paths = profilePaths(tree, fs::path(u8"yeni kullanıcı/用户"));
            const auto legacy = legacyLocation(tree, fs::path(u8"eski çalışma/旧"));
            const auto scoreSource = *legacy.directory / "scores.json";
            const auto settingsSource = *legacy.directory / "settings.json";
            TempTree::write(scoreSource, scoreBytes);
            TempTree::write(settingsSource, settingBytes);
            const auto before = snapshotTree(*legacy.directory);
            auto storage = std::make_shared<ControlledFileStorage>();

            const auto imported = app::migrateLegacyProfile(paths, legacy, storage);
            requireImport(imported.scores, ProfileImportStatus::Imported, scoreSource,
                          paths.scoresFile(), true);
            requireImport(imported.settings, ProfileImportStatus::Imported, settingsSource,
                          paths.settingsFile(), true);
            REQUIRE(imported.scores.saveStatus == FileSaveStatus::Saved);
            REQUIRE(imported.settings.saveStatus == FileSaveStatus::Saved);
            REQUIRE(readBytes(paths.scoresFile()) == scoreBytes);
            REQUIRE(readBytes(paths.settingsFile()) == settingBytes);
            REQUIRE(snapshotTree(*legacy.directory) == before);
            REQUIRE_FALSE(fs::exists(backupPath(paths.scoresFile())));
            REQUIRE_FALSE(fs::exists(backupPath(paths.settingsFile())));
            REQUIRE(storage->calls.size() == 2);
            for (const auto &call : storage->calls) {
                REQUIRE_FALSE(call.expectedSource.has_value());
                REQUIRE_FALSE(call.backupLegacy);
            }
        }
    }
}

TEST_CASE("ordinary managers upgrade copied legacy formats and back up only profile destinations",
          "[profile-migration][acceptance]") {
    TempTree tree;
    const auto paths = profilePaths(tree);
    const auto legacy = legacyLocation(tree);
    TempTree::write(*legacy.directory / "scores.json", kLegacyScores);
    TempTree::write(*legacy.directory / "settings.json", kLegacySettings);
    const auto before = snapshotTree(*legacy.directory);
    const auto imported = app::migrateLegacyProfile(paths, legacy);
    REQUIRE(imported.scores.status == ProfileImportStatus::Imported);
    REQUIRE(imported.settings.status == ProfileImportStatus::Imported);

    core2048::ScoreManager scores(paths.scoresFile(),
                                  app::storageAfterProfileImport(imported.scores));
    app::SettingsStore settings(paths.settingsFile(),
                                app::storageAfterProfileImport(imported.settings));
    REQUIRE(scores.load());
    REQUIRE(scores.needsMigration());
    REQUIRE(scores.completedGames().size() == 2);
    REQUIRE(scores.bestScore() == 512);
    REQUIRE(scores.completedGames()[0].playerName == utf8Bytes(u8"İnci 🎮"));
    REQUIRE(settings.load());
    REQUIRE(settings.needsMigration());
    REQUIRE_FALSE(settings.soundEnabled());
    REQUIRE(scores.save());
    REQUIRE(settings.save());
    REQUIRE_FALSE(scores.needsMigration());
    REQUIRE_FALSE(settings.needsMigration());
    REQUIRE(readBytes(backupPath(paths.scoresFile())) == kLegacyScores);
    REQUIRE(readBytes(backupPath(paths.settingsFile())) == kLegacySettings);
    REQUIRE(snapshotTree(*legacy.directory) == before);
    REQUIRE_FALSE(fs::exists(backupPath(*legacy.directory / "scores.json")));
    REQUIRE_FALSE(fs::exists(backupPath(*legacy.directory / "settings.json")));
    REQUIRE(nlohmann::json::parse(readBytes(paths.scoresFile()))["version"] == 2);
    REQUIRE(nlohmann::json::parse(readBytes(paths.settingsFile()))["version"] == 1);

    const auto rerun = app::migrateLegacyProfile(paths, legacy);
    REQUIRE(rerun.scores.status == ProfileImportStatus::DestinationPresent);
    REQUIRE(rerun.settings.status == ProfileImportStatus::DestinationPresent);
    core2048::ScoreManager reopened(paths.scoresFile());
    REQUIRE(reopened.load());
    REQUIRE(reopened.completedGames().size() == 2);
    REQUIRE(reopened.bestScore() == 512);
}

TEST_CASE("existing targets are authoritative even when legacy sources are better or invalid",
          "[profile-migration]") {
    const std::array<std::string, 3> targets{kV2Scores, "malformed target bytes",
                                             "{\"version\":1000}"};
    for (const auto &targetBytes : targets) {
        TempTree tree;
        const auto paths = profilePaths(tree);
        const auto legacy = legacyLocation(tree);
        TempTree::write(paths.scoresFile(), targetBytes);
        TempTree::write(paths.settingsFile(), "invalid authoritative settings");
        TempTree::write(*legacy.directory / "scores.json", kLegacyScores);
        TempTree::write(*legacy.directory / "settings.json", kLegacySettings);
        const auto before = snapshotTree(tree.root());
        auto storage = std::make_shared<ControlledFileStorage>();
        int readerCalls = 0;
        const app::LegacySnapshotReader reader = [&](const fs::path &) {
            ++readerCalls;
            return FileReadResult{FileReadStatus::IoError, {}};
        };

        const auto imported = app::migrateLegacyProfile(paths, legacy, storage, reader);
        REQUIRE(imported.scores.status == ProfileImportStatus::DestinationPresent);
        REQUIRE(imported.settings.status == ProfileImportStatus::DestinationPresent);
        REQUIRE(imported.scores.allowsProfileWrites());
        REQUIRE(imported.settings.allowsProfileWrites());
        REQUIRE(readerCalls == 0);
        REQUIRE(storage->calls.empty());
        REQUIRE(snapshotTree(tree.root()) == before);
    }
}

TEST_CASE("directory targets and corrupt targets are never replaced by legacy import",
          "[profile-migration]") {
    TempTree tree;
    const auto paths = profilePaths(tree);
    const auto legacy = legacyLocation(tree);
    fs::create_directory(paths.scoresFile());
    TempTree::write(paths.settingsFile(), "preserve corrupt settings");
    TempTree::write(*legacy.directory / "scores.json", kV2Scores);
    TempTree::write(*legacy.directory / "settings.json", kV1Settings);
    const auto before = snapshotTree(tree.root());

    const auto imported = app::migrateLegacyProfile(paths, legacy);
    REQUIRE(imported.scores.status == ProfileImportStatus::DestinationPresent);
    REQUIRE(imported.settings.status == ProfileImportStatus::DestinationPresent);
    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("existing targets win over an invalid or failed legacy location", "[profile-migration]") {
    TempTree tree;
    const auto paths = profilePaths(tree);
    TempTree::write(paths.scoresFile(), kV2Scores);
    TempTree::write(paths.settingsFile(), kV1Settings);
    const std::array<LegacyDataLocation, 5> invalidLocations{{
        {std::nullopt, {}},
        {fs::path{}, {}},
        {fs::path("relative-legacy-root"), {}},
        {embeddedNullPath(tree), {}},
        {tree.root() / "unavailable", std::make_error_code(std::errc::permission_denied)},
    }};
    const auto before = snapshotTree(tree.root());
    for (const auto &legacy : invalidLocations) {
        const auto imported = app::migrateLegacyProfile(paths, legacy);
        REQUIRE(imported.scores.status == ProfileImportStatus::DestinationPresent);
        REQUIRE(imported.settings.status == ProfileImportStatus::DestinationPresent);
        REQUIRE(imported.scores.allowsProfileWrites());
        REQUIRE(imported.settings.allowsProfileWrites());
    }
    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("invalid legacy roots block only missing profile targets without inventing another root",
          "[profile-migration]") {
    TempTree tree;
    const auto paths = profilePaths(tree);
    TempTree::write(paths.scoresFile(), kV2Scores);
    const std::array<LegacyDataLocation, 5> invalidLocations{{
        {std::nullopt, {}},
        {fs::path{}, {}},
        {fs::path("relative-legacy-root"), {}},
        {embeddedNullPath(tree), {}},
        {tree.root() / "unavailable", std::make_error_code(std::errc::permission_denied)},
    }};
    const auto before = snapshotTree(tree.root());
    for (const auto &legacy : invalidLocations) {
        const auto imported = app::migrateLegacyProfile(paths, legacy);
        REQUIRE(imported.scores.status == ProfileImportStatus::DestinationPresent);
        REQUIRE(imported.scores.allowsProfileWrites());
        REQUIRE(imported.settings.status == ProfileImportStatus::InvalidLegacyLocation);
        REQUIRE_FALSE(imported.settings.allowsProfileWrites());
        if (legacy.error) {
            REQUIRE(imported.settings.systemError == legacy.error);
        }
        requireSettingsWritesBlocked(paths.settingsFile(), imported.settings);
    }
    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("confirmed missing sources allow a fresh independently writable profile",
          "[profile-migration][acceptance]") {
    TempTree tree;
    const auto paths = profilePaths(tree);
    const auto legacy = legacyLocation(tree);
    const auto imported = app::migrateLegacyProfile(paths, legacy);
    requireImport(imported.scores, ProfileImportStatus::SourceMissing,
                  *legacy.directory / "scores.json", paths.scoresFile(), true);
    requireImport(imported.settings, ProfileImportStatus::SourceMissing,
                  *legacy.directory / "settings.json", paths.settingsFile(), true);
    REQUIRE_FALSE(fs::exists(paths.scoresFile()));
    REQUIRE_FALSE(fs::exists(paths.settingsFile()));

    core2048::ScoreManager scores(paths.scoresFile(),
                                  app::storageAfterProfileImport(imported.scores));
    app::SettingsStore settings(paths.settingsFile(),
                                app::storageAfterProfileImport(imported.settings));
    REQUIRE(scores.load());
    REQUIRE(scores.updateBestScore(64));
    REQUIRE(scores.save());
    REQUIRE(settings.load());
    REQUIRE(settings.setSoundEnabled(false));
    REQUIRE(settings.save());
    REQUIRE(fs::is_regular_file(paths.scoresFile()));
    REQUIRE(fs::is_regular_file(paths.settingsFile()));
    REQUIRE_FALSE(fs::exists(*legacy.directory / "scores.json"));
    REQUIRE_FALSE(fs::exists(*legacy.directory / "settings.json"));
}

TEST_CASE("a genuinely absent legacy directory is confirmed absence without being created",
          "[profile-migration]") {
    TempTree tree;
    const auto paths = profilePaths(tree);
    const LegacyDataLocation legacy{tree.root() / "missing" / "nested" / "legacy", {}};
    const auto before = snapshotTree(tree.root());
    const auto imported = app::migrateLegacyProfile(paths, legacy);
    REQUIRE(imported.scores.status == ProfileImportStatus::SourceMissing);
    REQUIRE(imported.settings.status == ProfileImportStatus::SourceMissing);
    REQUIRE(imported.scores.allowsProfileWrites());
    REQUIRE(imported.settings.allowsProfileWrites());
    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("file legacy roots and file ancestors are invalid locations rather than missing profiles",
          "[profile-migration]") {
    TempTree tree;
    const auto paths = profilePaths(tree);
    const auto blocker = tree.file("legacy-blocker", "preserve obstruction");
    const std::array<fs::path, 2> locations{blocker, blocker / "nested" / "legacy"};
    const auto before = snapshotTree(tree.root());
    for (const auto &location : locations) {
        const auto imported = app::migrateLegacyProfile(paths, {location, {}});
        REQUIRE(imported.scores.status == ProfileImportStatus::InvalidLegacyLocation);
        REQUIRE(imported.settings.status == ProfileImportStatus::InvalidLegacyLocation);
        requireScoreWritesBlocked(paths.scoresFile(), imported.scores);
        requireSettingsWritesBlocked(paths.settingsFile(), imported.settings);
    }
    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("malformed and unsupported legacy score files cannot be replaced by empty memory",
          "[profile-migration][acceptance]") {
    const std::array<std::pair<std::string, ProfileImportStatus>, 7> sources{{
        {"not JSON", ProfileImportStatus::InvalidSource},
        {"{\"scores\":\"wrong type\"}", ProfileImportStatus::InvalidSource},
        {"{\"scores\":[],\"scores\":[]}", ProfileImportStatus::InvalidSource},
        {"{\"scores\":[],\"foreign\":true}", ProfileImportStatus::InvalidSource},
        {kV2Scores + std::string(1, '\0') + "hidden trailing bytes",
         ProfileImportStatus::InvalidSource},
        {kV2Scores + "trailing non-JSON bytes", ProfileImportStatus::InvalidSource},
        {"{\"version\":777}", ProfileImportStatus::UnsupportedVersion},
    }};
    for (const auto &[bytes, status] : sources) {
        INFO("Expected import status: " << static_cast<int>(status));
        TempTree tree;
        const auto paths = profilePaths(tree);
        const auto legacy = legacyLocation(tree);
        const auto source = *legacy.directory / "scores.json";
        TempTree::write(source, bytes);
        const auto before = snapshotTree(tree.root());
        auto storage = std::make_shared<ControlledFileStorage>();
        const auto imported = app::migrateLegacyProfile(paths, legacy, storage);
        requireImport(imported.scores, status, source, paths.scoresFile(), false);
        REQUIRE(imported.settings.status == ProfileImportStatus::SourceMissing);
        REQUIRE(storage->calls.empty());
        requireScoreWritesBlocked(paths.scoresFile(), imported.scores);
        REQUIRE(snapshotTree(tree.root()) == before);
    }
}

TEST_CASE("malformed and unsupported legacy settings remain protected independently",
          "[profile-migration][acceptance]") {
    const std::array<std::pair<std::string, ProfileImportStatus>, 7> sources{{
        {"not JSON", ProfileImportStatus::InvalidSource},
        {"{\"sound_enabled\":\"wrong type\"}", ProfileImportStatus::InvalidSource},
        {"{\"sound_enabled\":true,\"sound_enabled\":false}", ProfileImportStatus::InvalidSource},
        {"{\"sound_enabled\":true,\"foreign\":true}", ProfileImportStatus::InvalidSource},
        {kV1Settings + std::string(1, '\0') + "hidden trailing bytes",
         ProfileImportStatus::InvalidSource},
        {kV1Settings + "trailing non-JSON bytes", ProfileImportStatus::InvalidSource},
        {"{\"version\":777}", ProfileImportStatus::UnsupportedVersion},
    }};
    for (const auto &[bytes, status] : sources) {
        TempTree tree;
        const auto paths = profilePaths(tree);
        const auto legacy = legacyLocation(tree);
        const auto source = *legacy.directory / "settings.json";
        TempTree::write(source, bytes);
        const auto before = snapshotTree(tree.root());
        const auto imported = app::migrateLegacyProfile(paths, legacy);
        requireImport(imported.settings, status, source, paths.settingsFile(), false);
        REQUIRE(imported.scores.status == ProfileImportStatus::SourceMissing);
        requireSettingsWritesBlocked(paths.settingsFile(), imported.settings);
        REQUIRE(snapshotTree(tree.root()) == before);
    }
}

TEST_CASE("a failed score import does not block a successful settings import",
          "[profile-migration]") {
    TempTree tree;
    const auto paths = profilePaths(tree);
    const auto legacy = legacyLocation(tree);
    TempTree::write(*legacy.directory / "scores.json", "broken legacy score bytes");
    TempTree::write(*legacy.directory / "settings.json", kV1Settings);
    const auto before = snapshotTree(*legacy.directory);
    const auto imported = app::migrateLegacyProfile(paths, legacy);
    REQUIRE(imported.scores.status == ProfileImportStatus::InvalidSource);
    REQUIRE_FALSE(imported.scores.allowsProfileWrites());
    REQUIRE(imported.settings.status == ProfileImportStatus::Imported);
    REQUIRE(imported.settings.allowsProfileWrites());
    requireScoreWritesBlocked(paths.scoresFile(), imported.scores);

    app::SettingsStore settings(paths.settingsFile(),
                                app::storageAfterProfileImport(imported.settings));
    REQUIRE(settings.load());
    REQUIRE_FALSE(settings.soundEnabled());
    REQUIRE(settings.setSoundEnabled(true));
    REQUIRE(settings.save());
    REQUIRE(snapshotTree(*legacy.directory) == before);
}

TEST_CASE("a failed settings import does not block a successful score import",
          "[profile-migration]") {
    TempTree tree;
    const auto paths = profilePaths(tree);
    const auto legacy = legacyLocation(tree);
    TempTree::write(*legacy.directory / "scores.json", kV2Scores);
    TempTree::write(*legacy.directory / "settings.json", "broken legacy setting bytes");
    const auto imported = app::migrateLegacyProfile(paths, legacy);
    REQUIRE(imported.scores.status == ProfileImportStatus::Imported);
    REQUIRE(imported.scores.allowsProfileWrites());
    REQUIRE(imported.settings.status == ProfileImportStatus::InvalidSource);
    REQUIRE_FALSE(imported.settings.allowsProfileWrites());
    requireSettingsWritesBlocked(paths.settingsFile(), imported.settings);

    core2048::ScoreManager scores(paths.scoresFile(),
                                  app::storageAfterProfileImport(imported.scores));
    REQUIRE(scores.load());
    REQUIRE(scores.updateBestScore(4096));
    REQUIRE(scores.save());
    REQUIRE(readBytes(*legacy.directory / "scores.json") == kV2Scores);
}

TEST_CASE("nonregular source leaves are rejected without creating any profile files",
          "[profile-migration]") {
    TempTree tree;
    const auto paths = profilePaths(tree);
    const auto legacy = legacyLocation(tree);
    fs::create_directory(*legacy.directory / "scores.json");
    fs::create_directory(*legacy.directory / "settings.json");
    const auto before = snapshotTree(tree.root());

    const auto imported = app::migrateLegacyProfile(paths, legacy);
    REQUIRE(imported.scores.status == ProfileImportStatus::InvalidSource);
    REQUIRE(imported.settings.status == ProfileImportStatus::InvalidSource);
    requireScoreWritesBlocked(paths.scoresFile(), imported.scores);
    requireSettingsWritesBlocked(paths.settingsFile(), imported.settings);
    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("an unreadable score snapshot remains protected while settings can import",
          "[profile-migration]") {
    TempTree tree;
    const auto paths = profilePaths(tree);
    const auto legacy = legacyLocation(tree);
    const auto scoreSource = *legacy.directory / "scores.json";
    TempTree::write(scoreSource, kV2Scores);
    TempTree::write(*legacy.directory / "settings.json", kV1Settings);
    auto storage = std::make_shared<ControlledFileStorage>();
    const app::LegacySnapshotReader reader = [&](const fs::path &source) {
        if (source == scoreSource) {
            return FileReadResult{FileReadStatus::IoError, {}};
        }
        return core2048::detail::readFileSnapshot(source);
    };

    const auto imported = app::migrateLegacyProfile(paths, legacy, storage, reader);
    REQUIRE(imported.scores.status == ProfileImportStatus::SourceIoError);
    REQUIRE_FALSE(imported.scores.allowsProfileWrites());
    REQUIRE(imported.settings.status == ProfileImportStatus::Imported);
    REQUIRE(storage->calls.size() == 1);
    REQUIRE(storage->calls[0].path == paths.settingsFile());
    requireScoreWritesBlocked(paths.scoresFile(), imported.scores);
    REQUIRE(readBytes(scoreSource) == kV2Scores);
    REQUIRE(readBytes(paths.settingsFile()) == kV1Settings);
}

TEST_CASE("publication failures keep targets absent and memory writes blocked for that process",
          "[profile-migration]") {
    constexpr std::array<FileSaveStatus, 4> failures{
        FileSaveStatus::IoError, FileSaveStatus::SerializationError, FileSaveStatus::WriteBlocked,
        FileSaveStatus::SourceChanged};
    for (const auto failure : failures) {
        TempTree tree;
        const auto paths = profilePaths(tree);
        const auto legacy = legacyLocation(tree);
        TempTree::write(*legacy.directory / "scores.json", kV2Scores);
        TempTree::write(*legacy.directory / "settings.json", kV1Settings);
        auto storage = std::make_shared<ControlledFileStorage>();
        storage->failedPath = paths.scoresFile();
        storage->failure = failure;

        const auto imported = app::migrateLegacyProfile(paths, legacy, storage);
        REQUIRE(imported.scores.status == ProfileImportStatus::PublicationFailed);
        REQUIRE(imported.scores.saveStatus == failure);
        REQUIRE_FALSE(imported.scores.allowsProfileWrites());
        REQUIRE(imported.settings.status == ProfileImportStatus::Imported);
        REQUIRE(readBytes(paths.settingsFile()) == kV1Settings);
        requireScoreWritesBlocked(paths.scoresFile(), imported.scores);
        REQUIRE(readBytes(*legacy.directory / "scores.json") == kV2Scores);
        REQUIRE(readBytes(*legacy.directory / "settings.json") == kV1Settings);
    }
}

TEST_CASE(
    "a later startup retries absent targets without reimporting successful files or duplicates",
    "[profile-migration][acceptance]") {
    TempTree tree;
    const auto paths = profilePaths(tree);
    const auto legacy = legacyLocation(tree);
    TempTree::write(*legacy.directory / "scores.json", kV2Scores);
    TempTree::write(*legacy.directory / "settings.json", kV1Settings);
    auto failing = std::make_shared<ControlledFileStorage>();
    failing->failedPath = paths.scoresFile();
    failing->failure = FileSaveStatus::IoError;
    const auto firstStartup = app::migrateLegacyProfile(paths, legacy, failing);
    REQUIRE(firstStartup.scores.status == ProfileImportStatus::PublicationFailed);
    REQUIRE(firstStartup.settings.status == ProfileImportStatus::Imported);
    requireScoreWritesBlocked(paths.scoresFile(), firstStartup.scores);
    const auto settingsBytes = readBytes(paths.settingsFile());
    const auto settingsModified = fs::last_write_time(paths.settingsFile());

    // The old score manager has been destroyed. This simulates startup, never a dirty-memory
    // reload.
    const auto nextStartup = app::migrateLegacyProfile(paths, legacy);
    REQUIRE(nextStartup.scores.status == ProfileImportStatus::Imported);
    REQUIRE(nextStartup.settings.status == ProfileImportStatus::DestinationPresent);
    REQUIRE(readBytes(paths.settingsFile()) == settingsBytes);
    REQUIRE((fs::last_write_time(paths.settingsFile()) == settingsModified));
    core2048::ScoreManager reopened(paths.scoresFile(),
                                    app::storageAfterProfileImport(nextStartup.scores));
    REQUIRE(reopened.load());
    REQUIRE(reopened.completedGames().size() == 1);
    REQUIRE(reopened.completedGames()[0].gameId == "round-one");
    REQUIRE(reopened.bestScore() == 2048);
    const auto before = snapshotTree(tree.root());
    const auto thirdStartup = app::migrateLegacyProfile(paths, legacy);
    REQUIRE(thirdStartup.scores.status == ProfileImportStatus::DestinationPresent);
    REQUIRE(thirdStartup.settings.status == ProfileImportStatus::DestinationPresent);
    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("a source changed before recheck is blocked before any publication",
          "[profile-migration]") {
    TempTree tree;
    const auto paths = profilePaths(tree);
    const auto legacy = legacyLocation(tree);
    const auto source = *legacy.directory / "scores.json";
    TempTree::write(source, kV2Scores);
    int scoreReads = 0;
    auto storage = std::make_shared<ControlledFileStorage>();
    const auto changedBytes = kV2Scores + " \r\n";
    const app::LegacySnapshotReader reader = [&](const fs::path &path) {
        if (path == source && ++scoreReads == 2) {
            TempTree::write(source, changedBytes);
        }
        return core2048::detail::readFileSnapshot(path);
    };

    const auto imported = app::migrateLegacyProfile(paths, legacy, storage, reader);
    REQUIRE(imported.scores.status == ProfileImportStatus::SourceChanged);
    REQUIRE_FALSE(imported.scores.allowsProfileWrites());
    REQUIRE(scoreReads == 2);
    REQUIRE(storage->calls.empty());
    REQUIRE(readBytes(source) == changedBytes);
    requireScoreWritesBlocked(paths.scoresFile(), imported.scores);
}

TEST_CASE("missing invalid or unreadable source rechecks never publish the first snapshot",
          "[profile-migration]") {
    constexpr std::array<FileReadStatus, 3> recheckStatuses{
        FileReadStatus::Missing, FileReadStatus::Invalid, FileReadStatus::IoError};
    for (const auto recheckStatus : recheckStatuses) {
        TempTree tree;
        const auto paths = profilePaths(tree);
        const auto legacy = legacyLocation(tree);
        const auto source = *legacy.directory / "scores.json";
        TempTree::write(source, kV2Scores);
        int scoreReads = 0;
        auto storage = std::make_shared<ControlledFileStorage>();
        const app::LegacySnapshotReader reader = [&](const fs::path &path) {
            if (path == source && ++scoreReads == 2) {
                return FileReadResult{recheckStatus, {}};
            }
            return core2048::detail::readFileSnapshot(path);
        };

        const auto imported = app::migrateLegacyProfile(paths, legacy, storage, reader);
        const auto expectedStatus = recheckStatus == FileReadStatus::IoError
                                        ? ProfileImportStatus::SourceIoError
                                        : ProfileImportStatus::SourceChanged;
        REQUIRE(imported.scores.status == expectedStatus);
        REQUIRE_FALSE(imported.scores.allowsProfileWrites());
        REQUIRE(scoreReads == 2);
        REQUIRE(storage->calls.empty());
        REQUIRE_FALSE(fs::exists(paths.scoresFile()));
        REQUIRE(readBytes(source) == kV2Scores);
    }
}

TEST_CASE("a missing snapshot reader blocks missing targets and never disturbs existing ones",
          "[profile-migration]") {
    TempTree tree;
    const auto paths = profilePaths(tree);
    const auto legacy = legacyLocation(tree);
    TempTree::write(paths.scoresFile(), kV2Scores);
    TempTree::write(*legacy.directory / "settings.json", kLegacySettings);
    const auto before = snapshotTree(tree.root());
    const auto imported =
        app::migrateLegacyProfile(paths, legacy, nullptr, app::LegacySnapshotReader{});
    REQUIRE(imported.scores.status == ProfileImportStatus::DestinationPresent);
    REQUIRE(imported.scores.allowsProfileWrites());
    REQUIRE(imported.settings.status == ProfileImportStatus::SourceIoError);
    REQUIRE_FALSE(imported.settings.allowsProfileWrites());
    requireSettingsWritesBlocked(paths.settingsFile(), imported.settings);
    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("source changes inside publication cannot alter the validated byte snapshot",
          "[profile-migration]") {
    TempTree tree;
    const auto paths = profilePaths(tree);
    const auto legacy = legacyLocation(tree);
    const auto source = *legacy.directory / "scores.json";
    TempTree::write(source, kV2Scores);
    const auto changedSource = kV2Scores + " \r\n";
    auto storage = std::make_shared<ControlledFileStorage>();
    storage->beforeFirstSave = [&](const SaveCall &) { TempTree::write(source, changedSource); };

    const auto imported = app::migrateLegacyProfile(paths, legacy, storage);
    REQUIRE(imported.scores.status == ProfileImportStatus::Imported);
    REQUIRE(readBytes(paths.scoresFile()) == kV2Scores);
    REQUIRE(readBytes(source) == changedSource);
    // Recheck happens before publication. Import copies that validated immutable snapshot;
    // it does not promise synchronization with later concurrent edits to the legacy source.
}

TEST_CASE("a concurrent target creation wins without importing or merging legacy scores",
          "[profile-migration][acceptance]") {
    const std::array<std::string, 3> concurrentTargets{
        kConcurrentScores, "corrupt concurrent bytes", "{\"version\":777}"};
    for (const auto &concurrentBytes : concurrentTargets) {
        TempTree tree;
        const auto paths = profilePaths(tree);
        const auto legacy = legacyLocation(tree);
        TempTree::write(*legacy.directory / "scores.json", kV2Scores);
        TempTree::write(*legacy.directory / "settings.json", kV1Settings);
        auto storage = std::make_shared<ControlledFileStorage>();
        storage->beforeFirstSave = [&](const SaveCall &call) {
            REQUIRE(call.path == paths.scoresFile());
            TempTree::write(call.path, concurrentBytes);
        };

        const auto imported = app::migrateLegacyProfile(paths, legacy, storage);
        REQUIRE(imported.scores.status == ProfileImportStatus::DestinationPresent);
        REQUIRE(imported.scores.allowsProfileWrites());
        REQUIRE(imported.scores.saveStatus.has_value());
        REQUIRE(*imported.scores.saveStatus != FileSaveStatus::Saved);
        REQUIRE(imported.settings.status == ProfileImportStatus::Imported);
        REQUIRE(readBytes(paths.scoresFile()) == concurrentBytes);
        REQUIRE(readBytes(paths.settingsFile()) == kV1Settings);
        REQUIRE(readBytes(*legacy.directory / "scores.json") == kV2Scores);
        REQUIRE_FALSE(fs::exists(backupPath(paths.scoresFile())));

        core2048::ScoreManager scores(paths.scoresFile(),
                                      app::storageAfterProfileImport(imported.scores));
        if (concurrentBytes == kConcurrentScores) {
            REQUIRE(scores.load());
            REQUIRE(scores.bestScore() == 65536);
            REQUIRE(scores.completedGames().size() == 1);
            REQUIRE(scores.completedGames()[0].gameId == "foreign-round");
            REQUIRE(scores.updateBestScore(131072));
            REQUIRE(scores.save());
            REQUIRE(scores.completedGames().size() == 1);
            REQUIRE(scores.completedGames()[0].gameId == "foreign-round");
        } else {
            REQUIRE_FALSE(scores.load());
            REQUIRE(scores.updateBestScore(4096));
            REQUIRE_FALSE(scores.save());
            REQUIRE(scores.lastSaveStatus() == FileSaveStatus::WriteBlocked);
            REQUIRE(readBytes(paths.scoresFile()) == concurrentBytes);
        }
    }
}

TEST_CASE("dependency exceptions become blocked profile imports without partial output",
          "[profile-migration]") {
    TempTree tree;
    const auto paths = profilePaths(tree);
    const auto legacy = legacyLocation(tree);
    TempTree::write(*legacy.directory / "scores.json", kV2Scores);
    auto storage = std::make_shared<ControlledFileStorage>();

    SECTION("snapshot reader throws") {
        const auto source = *legacy.directory / "scores.json";
        const app::LegacySnapshotReader reader = [source](const fs::path &path) {
            if (path == source) {
                throw std::runtime_error("injected source read failure");
            }
            return core2048::detail::readFileSnapshot(path);
        };
        const auto imported = app::migrateLegacyProfile(paths, legacy, storage, reader);
        REQUIRE(imported.scores.status == ProfileImportStatus::SourceIoError);
        REQUIRE(storage->calls.empty());
        requireScoreWritesBlocked(paths.scoresFile(), imported.scores);
    }

    SECTION("publication dependency throws") {
        storage->beforeFirstSave = [](const SaveCall &) {
            throw std::runtime_error("injected publication failure");
        };
        const auto imported = app::migrateLegacyProfile(paths, legacy, storage);
        REQUIRE(imported.scores.status == ProfileImportStatus::PublicationFailed);
        REQUIRE(imported.scores.saveStatus == FileSaveStatus::IoError);
        requireScoreWritesBlocked(paths.scoresFile(), imported.scores);
    }

    SECTION("snapshot reader throws during source recheck") {
        const auto source = *legacy.directory / "scores.json";
        int sourceReads = 0;
        const app::LegacySnapshotReader reader = [&](const fs::path &path) {
            if (path == source && ++sourceReads == 2) {
                throw std::runtime_error("injected source recheck failure");
            }
            return core2048::detail::readFileSnapshot(path);
        };
        const auto imported = app::migrateLegacyProfile(paths, legacy, storage, reader);
        REQUIRE(imported.scores.status == ProfileImportStatus::SourceIoError);
        REQUIRE(sourceReads == 2);
        REQUIRE(storage->calls.empty());
        requireScoreWritesBlocked(paths.scoresFile(), imported.scores);
    }

    REQUIRE(readBytes(*legacy.directory / "scores.json") == kV2Scores);
    REQUIRE_FALSE(fs::exists(paths.scoresFile()));
    REQUIRE_FALSE(fs::exists(paths.settingsFile()));
}

TEST_CASE("a same-directory legacy location is already authoritative and needs no import",
          "[profile-migration]") {
    TempTree tree;
    const auto paths = profilePaths(tree);
    TempTree::write(paths.scoresFile(), kV2Scores);
    TempTree::write(paths.settingsFile(), kV1Settings);
    const auto before = snapshotTree(tree.root());
    auto storage = std::make_shared<ControlledFileStorage>();
    const auto imported = app::migrateLegacyProfile(paths, {paths.directory(), {}}, storage);
    REQUIRE(imported.scores.status == ProfileImportStatus::DestinationPresent);
    REQUIRE(imported.settings.status == ProfileImportStatus::DestinationPresent);
    REQUIRE(storage->calls.empty());
    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("capturing the current legacy location reads the working directory without changing it",
          "[profile-migration][native]") {
    std::error_code error;
    const auto expected = fs::current_path(error);
    const auto captured = app::captureLegacyDataLocation();
    REQUIRE(captured.error == error);
    if (!error) {
        REQUIRE(captured.directory.has_value());
        REQUIRE(*captured.directory == expected);
        REQUIRE(captured.directory->is_absolute());
        REQUIRE(fs::current_path() == expected);
    }
}

#ifndef _WIN32
TEST_CASE("dangling legacy root and ancestor symlinks cannot be mistaken for missing profiles",
          "[profile-migration][symlink]") {
    TempTree tree;
    const auto paths = profilePaths(tree);
    const auto dangling = tree.root() / "dangling-legacy";
    const auto missingTarget = tree.root() / "missing-target";
    fileSymlink(missingTarget, dangling);
    const std::array<fs::path, 2> locations{dangling, dangling / "nested" / "legacy"};
    for (const auto &location : locations) {
        const auto imported = app::migrateLegacyProfile(paths, {location, {}});
        REQUIRE(imported.scores.status == ProfileImportStatus::InvalidLegacyLocation);
        REQUIRE(imported.settings.status == ProfileImportStatus::InvalidLegacyLocation);
        requireScoreWritesBlocked(paths.scoresFile(), imported.scores);
        requireSettingsWritesBlocked(paths.settingsFile(), imported.settings);
    }
    REQUIRE(fs::read_symlink(dangling) == missingTarget);
    REQUIRE_FALSE(fs::exists(missingTarget));
}

TEST_CASE("a symlink alias of the same profile does not duplicate or replace existing files",
          "[profile-migration][symlink]") {
    TempTree tree;
    const auto paths = profilePaths(tree);
    TempTree::write(paths.scoresFile(), kV2Scores);
    TempTree::write(paths.settingsFile(), kV1Settings);
    const auto alias = tree.root() / "profile-alias";
    fileSymlink(paths.directory(), alias);
    const auto scoreModified = fs::last_write_time(paths.scoresFile());
    const auto settingsModified = fs::last_write_time(paths.settingsFile());
    auto storage = std::make_shared<ControlledFileStorage>();
    const auto imported = app::migrateLegacyProfile(paths, {alias, {}}, storage);
    REQUIRE(imported.scores.status == ProfileImportStatus::DestinationPresent);
    REQUIRE(imported.settings.status == ProfileImportStatus::DestinationPresent);
    REQUIRE(storage->calls.empty());
    REQUIRE(readBytes(paths.scoresFile()) == kV2Scores);
    REQUIRE(readBytes(paths.settingsFile()) == kV1Settings);
    REQUIRE((fs::last_write_time(paths.scoresFile()) == scoreModified));
    REQUIRE((fs::last_write_time(paths.settingsFile()) == settingsModified));
}

TEST_CASE("a destination parent obstructed by a file fails before legacy publication",
          "[profile-migration]") {
    TempTree tree;
    const auto paths = profilePaths(tree);
    const auto legacy = legacyLocation(tree);
    TempTree::write(*legacy.directory / "scores.json", kV2Scores);
    TempTree::write(*legacy.directory / "settings.json", kV1Settings);
    fs::remove(paths.directory());
    TempTree::write(paths.directory(), "file obstruction");
    const auto before = snapshotTree(tree.root());
    auto storage = std::make_shared<ControlledFileStorage>();

    const auto imported = app::migrateLegacyProfile(paths, legacy, storage);
    REQUIRE(imported.scores.status == ProfileImportStatus::DestinationIoError);
    REQUIRE(imported.settings.status == ProfileImportStatus::DestinationIoError);
    REQUIRE_FALSE(imported.scores.allowsProfileWrites());
    REQUIRE_FALSE(imported.settings.allowsProfileWrites());
    REQUIRE(storage->calls.empty());
    REQUIRE(snapshotTree(tree.root()) == before);
}

TEST_CASE("legacy leaf symlinks are rejected even if their targets contain supported profiles",
          "[profile-migration][symlink]") {
    TempTree tree;
    const auto paths = profilePaths(tree);
    const auto legacy = legacyLocation(tree);
    const auto realScores = tree.file("other/scores.json", kV2Scores);
    const auto realSettings = tree.file("other/settings.json", kV1Settings);
    const auto scoreSource = *legacy.directory / "scores.json";
    const auto settingsSource = *legacy.directory / "settings.json";
    fileSymlink(realScores, scoreSource);
    fileSymlink(realSettings, settingsSource);

    const auto imported = app::migrateLegacyProfile(paths, legacy);
    REQUIRE(imported.scores.status == ProfileImportStatus::InvalidSource);
    REQUIRE(imported.settings.status == ProfileImportStatus::InvalidSource);
    requireScoreWritesBlocked(paths.scoresFile(), imported.scores);
    requireSettingsWritesBlocked(paths.settingsFile(), imported.settings);
    REQUIRE(fs::read_symlink(scoreSource) == realScores);
    REQUIRE(fs::read_symlink(settingsSource) == realSettings);
    REQUIRE(readBytes(realScores) == kV2Scores);
    REQUIRE(readBytes(realSettings) == kV1Settings);
}

TEST_CASE("dangling legacy leaf symlinks are unresolved sources and block empty-profile saves",
          "[profile-migration][symlink]") {
    TempTree tree;
    const auto paths = profilePaths(tree);
    const auto legacy = legacyLocation(tree);
    const auto missingScores = tree.root() / "missing-scores";
    const auto missingSettings = tree.root() / "missing-settings";
    fileSymlink(missingScores, *legacy.directory / "scores.json");
    fileSymlink(missingSettings, *legacy.directory / "settings.json");

    const auto imported = app::migrateLegacyProfile(paths, legacy);
    REQUIRE(imported.scores.status == ProfileImportStatus::InvalidSource);
    REQUIRE(imported.settings.status == ProfileImportStatus::InvalidSource);
    requireScoreWritesBlocked(paths.scoresFile(), imported.scores);
    requireSettingsWritesBlocked(paths.settingsFile(), imported.settings);
    REQUIRE_FALSE(fs::exists(missingScores));
    REQUIRE_FALSE(fs::exists(missingSettings));
}

TEST_CASE("existing destination leaf symlinks remain authoritative and untouched",
          "[profile-migration][symlink]") {
    TempTree tree;
    const auto paths = profilePaths(tree);
    const auto legacy = legacyLocation(tree);
    const auto scoreTarget = tree.file("other/scores.json", kV2Scores);
    const auto absentSettingsTarget = tree.root() / "missing-settings";
    fileSymlink(scoreTarget, paths.scoresFile());
    fileSymlink(absentSettingsTarget, paths.settingsFile());
    TempTree::write(*legacy.directory / "scores.json", kLegacyScores);
    TempTree::write(*legacy.directory / "settings.json", kLegacySettings);
    auto storage = std::make_shared<ControlledFileStorage>();

    const auto imported = app::migrateLegacyProfile(paths, legacy, storage);
    REQUIRE(imported.scores.status == ProfileImportStatus::DestinationPresent);
    REQUIRE(imported.settings.status == ProfileImportStatus::DestinationPresent);
    REQUIRE(storage->calls.empty());
    REQUIRE(fs::read_symlink(paths.scoresFile()) == scoreTarget);
    REQUIRE(fs::read_symlink(paths.settingsFile()) == absentSettingsTarget);
    REQUIRE(readBytes(scoreTarget) == kV2Scores);
    REQUIRE_FALSE(fs::exists(absentSettingsTarget));
}
#endif
