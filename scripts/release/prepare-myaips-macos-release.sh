#!/usr/bin/env bash
# Bump the MyAIPs version, rebuild the macOS app, and create the DMG.
# This is the release entry point for the 0.99 -> 0.100 -> 0.101 sequence.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT"

next_version=$(scripts/release/bump-myaips-version.sh)
printf 'Preparing MyAIPs %s\n' "$next_version"

CMAKE="${CMAKE:-}"
if [[ -z "$CMAKE" ]]; then
  if [[ -x "$ROOT/.deps/CMake.app/Contents/bin/cmake" ]]; then
    CMAKE="$ROOT/.deps/CMake.app/Contents/bin/cmake"
  else
    CMAKE=cmake
  fi
fi

"$CMAKE" --preset mac-release
"$CMAKE" --build build/mac-release -j 6
bash packaging/macos/make-dmg.sh

printf 'MyAIPs %s is ready in %s\n' "$next_version" "$ROOT/build/package/MyAIPs-$next_version.dmg"
