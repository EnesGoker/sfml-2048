# 2048 Project (C++ / SFML)

[![CI](https://github.com/EnesGoker/sfml-2048/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/EnesGoker/sfml-2048/actions/workflows/ci.yml)
[![Release](https://github.com/EnesGoker/sfml-2048/actions/workflows/release.yml/badge.svg)](https://github.com/EnesGoker/sfml-2048/actions/workflows/release.yml)
[![License](https://img.shields.io/github/license/EnesGoker/sfml-2048)](LICENSE)

A desktop implementation of 2048 written in C++. The goal of this repository is to evolve into an enterprise-grade portfolio project with reproducible builds, tests, CI, and release automation.

## Screenshot

![Gameplay preview](docs/screenshots/gameplay.png)

## Current Features

- 4x4 board with classic 2048 merge rules
- Score tracking
- Splash screen + game over overlay
- Keyboard controls (arrow keys)
- Spawn tile animation

## Controls

- `Up Arrow`: move up
- `Down Arrow`: move down
- `Left Arrow`: move left
- `Right Arrow`: move right
- `Esc` (during game): return to splash menu
- `N` or `Enter` (game over): start a new game
- `Q` or `Esc` (game over): quit

## Build

The project now uses CMake presets.

### Prerequisites

- CMake 3.24+
- C++20 compatible compiler

### Option A: System dependencies (quick local build)

macOS:

```bash
brew install sfml
cmake --preset debug
cmake --build --preset build-debug
./build/debug/sfml_2048
```

Linux (Debian/Ubuntu example):

```bash
sudo apt-get install libsfml-dev
cmake --preset debug
cmake --build --preset build-debug
./build/debug/sfml_2048
```

Windows (Visual Studio Developer Prompt example):

```bat
cmake --preset debug
cmake --build --preset build-debug
build\\debug\\sfml_2048.exe
```

### Option B: vcpkg manifest mode (recommended for reproducible setup)

macOS/Linux:

```bash
git clone https://github.com/microsoft/vcpkg.git "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh"
brew install pkg-config # macOS only
export VCPKG_ROOT="$HOME/vcpkg"
cmake --preset vcpkg-debug
cmake --build --preset build-vcpkg-debug
./build/vcpkg-debug/sfml_2048
```

Windows:

```bat
git clone https://github.com/microsoft/vcpkg.git C:\\vcpkg
C:\\vcpkg\\bootstrap-vcpkg.bat
set VCPKG_ROOT=C:\\vcpkg
cmake --preset vcpkg-debug
cmake --build --preset build-vcpkg-debug
build\\vcpkg-debug\\sfml_2048.exe
```

`vcpkg.json` is provided in the repository root and pins `sfml` to `2.6.2` via manifest versioning.

## CI Pipeline

GitHub Actions workflow: `.github/workflows/ci.yml`

- `format`: `clang-format` policy check
- `build-and-test`: matrix build + test on Ubuntu, macOS, Windows
- `sanitizers`: ASan/UBSan build + test on Ubuntu
- `coverage`: gcovr report for core module with line coverage threshold (`>= 80%`)

One-time branch protection setup (maintainer):

```bash
./scripts/apply_branch_protection.sh <owner/repo> single-maintainer
```

Team setup (enforced reviews + admin enforcement):

```bash
./scripts/apply_branch_protection.sh <owner/repo> team
```

## Engineering Docs

- Architecture: `docs/architecture.md`
- Testing strategy: `docs/testing.md`
- Maintenance playbook: `docs/maintenance.md`
- Contribution guide: `CONTRIBUTING.md`
- Security policy: `SECURITY.md`
- Third-party notices: `THIRD_PARTY_NOTICES.md`

## Packaging and Release

Local release package (ZIP):

```bash
cmake --preset release
cmake --build --preset build-release
ctest --test-dir build/release --output-on-failure
cpack --config build/release/CPackConfig.cmake
```

Optional install staging check:

```bash
cmake --install build/release --prefix build/release/install
```

Automated GitHub Release:

- Workflow: `.github/workflows/release.yml`
- Trigger: push a tag in `vX.Y.Z` format
- Guardrail: tag version must match `VERSION` file
- Outputs: cross-platform ZIP artifacts (Ubuntu, macOS, Windows) attached to the GitHub Release

Example:

```bash
git tag v0.1.0
git push origin v0.1.0
```

Source-only package (clean zip without build artifacts):

```bash
./scripts/package_source.sh
```

This script excludes `build/`, `vcpkg_installed/`, `_CPack_Packages/`, `*.zip`, and macOS metadata (`__MACOSX`).

### Deterministic Run (Seeded)

Use a fixed seed to reproduce the same random tile sequence:

```bash
./build/debug/sfml_2048 --seed 1234
```

Runtime tuning examples:

```bash
./build/debug/sfml_2048 --no-vsync --fps 120
./build/debug/sfml_2048 --vsync
```

### Core Test Target (SFML-independent)

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

List test names:

```bash
./build/core_unit_tests --list-tests
```

Deterministic snapshot check only:

```bash
./build/core_unit_tests "[golden]"
```

### Static Format Check

```bash
./scripts/check_format.sh
```

### Sanitizer Run (Linux local option)

```bash
cmake -S . -B build/sanitizers \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DSFML_2048_ENABLE_SANITIZERS=ON
cmake --build build/sanitizers
ctest --test-dir build/sanitizers --output-on-failure
```

## Project Structure (Current)

- `src/core/Game.hpp` + `src/core/Game.cpp`: SFML-independent game engine
- `src/app/App.hpp` + `src/app/App.cpp`: SFML app layer (window/input/render)
- `src/app/main.cpp`: executable entry point
- `assets/fonts/`: runtime font assets
- `CMakeLists.txt`: build configuration
- `CMakePresets.json`: standard Debug/Release and vcpkg presets

## Roadmap

See `CHANGELOG.md` and ongoing milestone plan in project discussions.
