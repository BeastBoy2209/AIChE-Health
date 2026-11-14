#!/usr/bin/env bash
# Build and run AIChE Health on macOS (Apple Silicon or Intel).
set -euo pipefail
cd "$(dirname "$0")/.."

if ! xcode-select -p >/dev/null 2>&1; then
  echo ">> Installing Xcode Command Line Tools (a dialog will appear; re-run this script when it finishes)"
  xcode-select --install || true
  exit 1
fi
if ! command -v cmake >/dev/null; then
  if command -v brew >/dev/null; then brew install cmake
  else echo "Install CMake: https://cmake.org/download/ or Homebrew (https://brew.sh) then 'brew install cmake'"; exit 1; fi
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(sysctl -n hw.ncpu)"

echo
echo "Done. Starting the demo..."
cd build && ./ui/aiche-health --profile demo_expedition.json