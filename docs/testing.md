# Testing Strategy

## Scope

Primary quality gate is unit testing of `src/core` because gameplay correctness lives there and should not depend on SFML.

Current suite: `tests/core_unit_tests.cpp` (Catch2).

## Test Categories

- Merge behavior:
  - `[2,2,2,2] -> [4,4,0,0]`
  - one-merge-per-tile rule (`[2,2,4,4] -> [4,8,0,0]`)
- Move behavior:
  - no-op move does not mutate grid
  - no spawn on no-op
- Game-over detection:
  - dead board vs board with legal merge
- Score behavior:
  - cumulative score across multiple moves
- Golden deterministic snapshot:
  - fixed seed + fixed move sequence -> expected final grid, score, and move flags

## Determinism Rules

- Core uses `std::mt19937`.
- `--seed <uint32>` can be passed to the app for reproducible runs.
- Golden tests use fixed seed (`1234`) and assert exact outcomes.

## How To Run

All tests (system dependency build):

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

With vcpkg presets:

```bash
cmake --preset vcpkg-debug
cmake --build --preset build-vcpkg-debug
ctest --test-dir build/vcpkg-debug --output-on-failure
```

Run only deterministic snapshot case:

```bash
./build/vcpkg-debug/core_unit_tests "[golden]"
```

## CI Enforcement

GitHub Actions (`.github/workflows/ci.yml`) runs:

- format check (`clang-format`)
- build + tests on Linux/macOS/Windows
- sanitizer job on Linux
- coverage job (`gcovr`) with `--fail-under-line 80` for `src/core`

Release workflow (`.github/workflows/release.yml`) also runs tests before packaging.
