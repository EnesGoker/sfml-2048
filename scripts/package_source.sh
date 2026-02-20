#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "$0")/.." && pwd)"
version="$(tr -d '\r\n' < "$project_root/VERSION")"
output_name="${1:-sfml_2048-${version}-source.zip}"

for required_cmd in rsync zip; do
  if ! command -v "$required_cmd" >/dev/null 2>&1; then
    echo "Missing required command: $required_cmd"
    exit 1
  fi
done

cd "$project_root"

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

staging_dir="$tmp_dir/sfml_2048-${version}"
mkdir -p "$staging_dir"

rsync -a \
  --exclude ".git/" \
  --exclude ".github/workflows/*.tmp" \
  --exclude "build/" \
  --exclude "vcpkg_installed/" \
  --exclude ".vcpkg/" \
  --exclude "_CPack_Packages/" \
  --exclude "__MACOSX/" \
  --exclude "*.zip" \
  --exclude ".DS_Store" \
  ./ "$staging_dir/"

(
  cd "$tmp_dir"
  zip -X -r "$output_name" "sfml_2048-${version}" >/dev/null
  mv "$output_name" "$project_root/"
)

echo "Created source package: $project_root/$output_name"
