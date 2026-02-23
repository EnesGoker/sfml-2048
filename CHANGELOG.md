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
- PR template and structured issue forms for bug/feature intake.
- Dependabot configuration for weekly GitHub Actions dependency updates.
- Release artifacts now include `SHA256SUMS.txt` and SPDX SBOM files.
- Property-style core tests for invariant preservation and left/right mirror symmetry.
- Repository ownership and compliance inventory files: `.github/CODEOWNERS` and `THIRD_PARTY_NOTICES.md`.
- Persistent top-5 high score module (`ScoreManager`) with JSON storage (`scores.json`) including timestamp and optional seed metadata.
- Splash scene now includes optional seed input for deterministic runs without CLI flags.
- Sound effects module (`SoundManager`) with persisted on/off preference (`settings.json`).

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
- CI now runs a dedicated clang-tidy static analysis job (`bugprone`, `performance`, `portability`).
- CI/release now provision CMake `3.29.6` explicitly to satisfy preset minimum requirements on all runners.
- Linux CI/release jobs now install SFML system prerequisites required by vcpkg builds.
- Windows CI/release now run multi-config aware build/test/package commands (`--config` / `ctest -C`).
- Core tile spawn RNG now uses an implementation-independent bounded sampler for cross-platform deterministic seed behavior.
- CI coverage filter paths now use workspace-relative regexes to avoid platform/path encoding mismatches.
- CI static-analysis now reports clang-tidy warnings without hard-failing on warning-level findings.
- GitHub Actions workflow actions were upgraded to latest major versions (`checkout@v6`, `cache@v5`, `download-artifact@v7`, `upload-artifact@v6`).
- vcpkg baseline pin was updated to `66c0373dc7fca549e5803087b9487edfe3aca0a1`.
- Branch protection setup script now supports `single-maintainer` (safe default) and `team` profiles.
- Release workflow now generates SPDX SBOM files from extracted package contents per platform artifact.
- README now includes CI/release/license badges and clearer screenshot/branch-protection guidance.
- README screenshot now points to a concrete gameplay image (`docs/screenshots/gameplay.png`) instead of placeholder art.
- README was redesigned as a bilingual (TR/EN), showcase-focused portfolio landing page with simplified quick-start guidance.
- Windows release packaging now uses explicit PowerShell ZIP staging (exe + assets + DLLs) to avoid CPack failures on `windows-latest`.
- App CLI now supports runtime performance flags (`--fps`, `--vsync`, `--no-vsync`) in addition to `--seed`.
- App asset path discovery was extracted from `App.cpp` into a dedicated `AssetResolver` module to reduce UI-layer coupling.
- App UI now shows `Best: <score>` in the top panel and game-over overlay, backed by persisted high-score data.
- Playing scene now includes a top-panel `NEW GAME` button and score-delta floating text feedback.
- Tile visuals were modernized with rounded custom shapes and move-timeline effects (slide lerp, merge pop, spawn fade).
- Added interactive sound toggle button plus action-based sound cues (slide, merge, spawn, game over, high score).
- Playing top panel layout now stacks `Best` under `Score` on the left and moves actions into a right-side hamburger menu.

### Removed
- Committed build artifacts (`*.o`, local binaries, `.DS_Store`).
- Legacy Makefile with machine-specific Homebrew include/library paths.

## [0.2.1] - 2026-02-23

### Changed
- CMake post-build asset copy command now uses explicit quoting and `VERBATIM` for safer cross-platform path handling.
- Manifest and project version metadata are aligned at `0.2.1` (`VERSION` and `vcpkg.json`).
- README rendering quality was improved (`# sfml-2048` header fix, normalized LF policy via `.gitattributes`, and explicit coverage gate note).

### Legal
- Replaced `assets/fonts/Geneva.ttf` with OFL-licensed `Inter-Variable.ttf`.
- Added `assets/fonts/LICENSE-Inter.txt` and updated `THIRD_PARTY_NOTICES.md` accordingly.

## [0.1.0] - 2026-02-20

### Added
- Initial playable SFML-based 2048 implementation.
