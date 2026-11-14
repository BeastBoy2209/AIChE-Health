#!/usr/bin/env bash
# Build AIChE Health on Linux (Ubuntu/Debian shown; everything else is vendored in third_party/).
set -euo pipefail
cd "$(dirname "$0")/.."

if command -v apt-get >/dev/null; then
  echo ">> installing build dependencies (needs sudo)"
  sudo apt-get update
  sudo apt-get install -y build-essential cmake pkg-config \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

echo
echo "Done. Run the demo (core + UI in one process):"
echo "  cd build && ./ui/aiche-health --profile demo_expedition.json"
echo "or run the daemon and UI separately:"
echo "  ./build/aiche-core --data build/data --profiles profiles &   ;   ./build/ui/aiche-health --connect"

