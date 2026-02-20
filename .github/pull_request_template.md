## Summary

- What changed?
- Why was this needed?

## Validation

- [ ] `./scripts/check_format.sh`
- [ ] `cmake --build --preset build-vcpkg-debug`
- [ ] `ctest --test-dir build/vcpkg-debug --output-on-failure`

## Checklist

- [ ] Scope is focused (no unrelated changes).
- [ ] Tests were added/updated for behavior changes.
- [ ] Docs/README/CHANGELOG updated when needed.
- [ ] CI is expected to pass (`format`, `static-analysis`, `build-and-test`, `sanitizers`, `coverage`).
