# sfml-2048 | Enterprise C++ Game Showcase

[![CI](https://github.com/EnesGoker/sfml-2048/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/EnesGoker/sfml-2048/actions/workflows/ci.yml)
[![Release](https://github.com/EnesGoker/sfml-2048/actions/workflows/release.yml/badge.svg)](https://github.com/EnesGoker/sfml-2048/actions/workflows/release.yml)
[![License](https://img.shields.io/github/license/EnesGoker/sfml-2048)](LICENSE)
![C++](https://img.shields.io/badge/C%2B%2B-20-00599C)
![CMake](https://img.shields.io/badge/CMake-3.24%2B-064F8C)
![SFML](https://img.shields.io/badge/SFML-2.6.2-8CC445)

## TR

`sfml-2048`, klasik 2048 oyununu modern C++ yaklaşımıyla yeniden yorumlayan bir masaüstü proje vitrini.
Amaç sadece oyun geliştirmek değil; test edilebilir mimari, CI disiplini ve release otomasyonu ile profesyonel bir mühendislik standardı göstermek.

## EN

`sfml-2048` is a desktop showcase project that reimagines the classic 2048 game with modern C++ practices.
The goal is not only to build a game, but also to demonstrate professional engineering standards through testable architecture, CI discipline, and release automation.

## Preview

![Gameplay preview](docs/screenshots/gameplay.png)

## Highlights | Öne Çıkanlar

- Deterministic core engine (`seed` support) | Deterministik core motor (`seed` desteği)
- SFML-independent game logic for unit tests | Unit testler için SFML’den bağımsız oyun mantığı
- Multi-platform CI (Ubuntu, macOS, Windows) | Çok platformlu CI (Ubuntu, macOS, Windows)
- Automated release artifacts and versioning | Otomatik release artifact’ları ve sürümleme
- Clean project structure for portfolio presentation | Portfolyo için temiz ve okunabilir proje yapısı

## Tech Stack | Teknoloji Yığını

- Language: C++20
- UI/Rendering: SFML 2.6.2
- Build System: CMake + CMake Presets
- Dependency Management: vcpkg manifest mode
- Tests: Catch2 + CTest
- CI/CD: GitHub Actions

## Quick Start | Hızlı Başlangıç

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

## CLI Options | Çalıştırma Parametreleri

```bash
--seed <uint32>    Deterministic run | Deterministik oyun akışı
--fps <uint>       Frame limit       | FPS limiti
--vsync            Enable VSync      | VSync aç
--no-vsync         Disable VSync     | VSync kapat
--help             Show help         | Yardım göster
```

## Controls | Kontroller

- Arrow keys: move tiles | Yön tuşları: taşları hareket ettir
- `Esc` (in-game): back to splash | `Esc` (oyun içinde): ana ekrana dön
- `N` or `Enter` (game over): new game | `N` veya `Enter` (oyun bitince): yeni oyun
- `Q` or `Esc` (game over): quit | `Q` veya `Esc` (oyun bitince): çıkış

## Engineering Quality | Mühendislik Kalitesi

- Core rules are isolated in `src/core` and testable without SFML.
- CI gates include format checks, static analysis, cross-platform build/test, sanitizers, and coverage.
- Releases are versioned and distributed as downloadable artifacts.

## Documentation | Dokümantasyon

- Architecture: [docs/architecture.md](docs/architecture.md)
- Testing Strategy: [docs/testing.md](docs/testing.md)
- Maintenance Playbook: [docs/maintenance.md](docs/maintenance.md)
- Contributing: [CONTRIBUTING.md](CONTRIBUTING.md)
- Security: [SECURITY.md](SECURITY.md)
- Third-Party Notices: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)

## Roadmap | Yol Haritası

Current progress and upcoming improvements are tracked in [CHANGELOG.md](CHANGELOG.md).

## License

Licensed under the MIT License. See [LICENSE](LICENSE).
