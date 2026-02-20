# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Repository baseline metadata for enterprise-style project hygiene.
- CMake-based build configuration (`CMakeLists.txt`) with C++20 and warning flags.
- CMake preset workflow (`CMakePresets.json`) for Debug/Release and vcpkg builds.
- `vcpkg.json` manifest for automatic SFML dependency resolution.
- `src/core` + `src/app` architecture split with `game_core` library target.
- New SFML-independent core API: `reset(seed)`, `applyMove(Direction)`, `getGrid()`, `getScore()`, `isGameOver()`.
- `--seed <uint32>` CLI option to launch deterministic game sessions.
- Catch2-based `core_unit_tests` suite with merge, no-op, game-over, score, and golden snapshot coverage.
- GitHub Actions CI pipeline with multi-OS matrix, format check, and sanitizer job.
- `scripts/check_format.sh` for local/CI clang-format enforcement.
- CMake install rules for runtime binary, assets, metadata files, and runtime dependencies.
- CPack ZIP packaging configuration for distributable release artifacts.
- Tag-driven release workflow (`.github/workflows/release.yml`) to build/package on Ubuntu, macOS, and Windows and publish GitHub Releases.
- vcpkg manifest version pinning (`builtin-baseline` + `overrides`) to keep SFML on `2.6.2` across platforms.
- Executable-based asset path resolver with fallback candidates for robust font loading across run locations.
- Game over overlay now provides clear `NEW GAME` and `QUIT` actions (mouse + keyboard shortcuts).
- Engineering documentation set: `docs/architecture.md` and `docs/testing.md`.
- Contribution and governance docs: `CONTRIBUTING.md` and `SECURITY.md`.
- Source-only archive script (`scripts/package_source.sh`) using metadata-clean ZIP output.
- CI/release caching improvements for vcpkg archives and compiler cache (`ccache` on Linux/macOS).
- Branch protection bootstrap script (`scripts/apply_branch_protection.sh`) for required checks and review gate.

### Changed
- Standardized runtime font asset path to `assets/fonts/Geneva.ttf`.
- README build instructions migrated to CMake + vcpkg flow.
- Documented `pkg-config` prerequisite for macOS vcpkg bootstrap flow.
- App layer now consumes `MoveResult` from core instead of mutating game state directly.
- Test harness migrated from ad-hoc binaries to a single Catch2 unit test executable.
- CMake now supports opt-in ASan/UBSan via `SFML_2048_ENABLE_SANITIZERS`.
- CMake project version now reads from the repository `VERSION` file (SemVer validated).
- Text centering now uses `bounds.left/top`-aware origins to prevent per-font alignment drift.
- Spawn animations are explicitly cleared on reset/menu transitions and when entering game-over state.
- Removed `std::char_traits<unsigned int>` workaround header from app build path.
- CI and release workflows now pin `vcpkg` checkout to `vcpkg.json` `builtin-baseline`.
- Added CI coverage gate (`gcovr`) with minimum line coverage threshold for `src/core`.

### Removed
- Committed build artifacts (`*.o`, local binaries, `.DS_Store`).
- Legacy Makefile with machine-specific Homebrew include/library paths.

## [0.1.0] - 2026-02-20

### Added
- Initial playable SFML-based 2048 implementation.
