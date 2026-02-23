#sfml - 2048

> A classic 2048 clone built with modern C++ - designed from the ground up for testability, cross-platform CI, and clean architecture.

[![CI](https://github.com/EnesGoker/sfml-2048/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/EnesGoker/sfml-2048/actions/workflows/ci.yml)
[![Release](https://github.com/EnesGoker/sfml-2048/actions/workflows/release.yml/badge.svg)](https://github.com/EnesGoker/sfml-2048/actions/workflows/release.yml)
[![License: MIT](https://img.shields.io/github/license/EnesGoker/sfml-2048)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-005999)](https://en.cppreference.com/w/cpp/20)
[![SFML 2.6.2](https://img.shields.io/badge/SFML-2.6.2-8CC445)](https://www.sfml-dev.org/)

<p align="center">
  <img src="docs/screenshots/gameplay.png" alt="Gameplay" width="720" />
</p>

---

## What is this?

This is not just a 2048 clone. It's a **portfolio-grade C++ project** that demonstrates:

- A game core that is **decoupled from SFML** and **unit-testable**
- A **multi-platform CI pipeline** (Ubuntu, macOS, Windows)
- **Deterministic core logic** with reproducible tests
- Automated versioning + release artifacts

If you're here to play the game, great. If you're here to understand how it's built, read on.

---

## Quick Start (Build & Run)

### Prerequisites

- **CMake 3.24+**
- **vcpkg** (manifest mode) and `VCPKG_ROOT` set
- A **C++20** compiler (GCC 11+, Clang 13+, MSVC 2022+)

> Tip: If you don't have vcpkg yet, install it once:
> - macOS/Linux: `git clone https://github.com/microsoft/vcpkg.git && ./vcpkg/bootstrap-vcpkg.sh`
> - Windows: `git clone https://github.com/microsoft/vcpkg.git && .\\vcpkg\\bootstrap-vcpkg.bat`

### macOS / Linux

```bash
cmake --preset vcpkg-debug
cmake --build --preset build-vcpkg-debug
./build/vcpkg-debug/sfml_2048
```

### Windows

```bat
cmake --preset vcpkg-debug
cmake --build --preset build-vcpkg-debug
build\vcpkg-debug\sfml_2048.exe
```

---

## CLI Options

Run `--help` to see the up-to-date list from the binary.

| Flag | Description |
|---|---|
| `--fps <uint>` | Frame-rate cap |
| `--vsync` | Enable vertical sync |
| `--no-vsync` | Disable vertical sync |
| `--help` | Show usage |

---

## How to Play

| Input | Action |
|---|---|
| Arrow keys | Move tiles |
| `Esc` (in-game) | Return to splash screen |
| `N` / `Enter` (game over) | Start a new game |
| `Q` / `Esc` (game over) | Quit |

---

## Architecture

The project is split into two clear layers:

```text
src/
├── core/     # Pure game logic (no SFML). Fully testable.
└── app/      # SFML rendering, input, window management
```

The core layer operates on plain data structures and can be exercised by unit tests without a display.
The app layer owns rendering and delegates state changes back to core.

This separation means:
- You can swap the renderer without touching game logic
- Unit tests are fast and dependency-free
- Rules are easier to reason about in isolation

Full details: [docs/architecture.md](docs/architecture.md)

---

## Testing

Tests are written with Catch2 and run via CTest:

```bash
ctest --test-dir build/vcpkg-debug --output-on-failure
```

CI runs tests across platforms and also executes sanitizer + coverage jobs.
Full strategy: [docs/testing.md](docs/testing.md)

---

## CI / CD

Every pull request goes through quality gates:

1. `clang-format` check
2. Static analysis
3. Cross-platform build (Ubuntu, macOS, Windows)
4. Unit tests + sanitizers (ASan/UBSan on Linux)
5. Coverage report

Releasing:
- Pushing a `vX.Y.Z` tag triggers an automated release
- Release artifacts are produced for all three platforms

---

## Tech Stack

| | |
|---|---|
| Language | C++20 |
| Graphics / Input | SFML 2.6.2 |
| Build | CMake + CMake Presets |
| Dependencies | vcpkg (manifest mode) |
| Testing | Catch2 + CTest |
| CI/CD | GitHub Actions |

---

## Documentation

- [Architecture](docs/architecture.md)
- [Testing Strategy](docs/testing.md)
- [Maintenance Playbook](docs/maintenance.md)
- [Changelog](CHANGELOG.md)
- [Contributing](CONTRIBUTING.md)
- [Security Policy](SECURITY.md)
- [Third-Party Notices](THIRD_PARTY_NOTICES.md)

---

## License

MIT - see [LICENSE](LICENSE) for details.
