#!/usr/bin/env bash
set -euo pipefail

CONFIG="${1:-Release}"

if [[ "$(uname -s)" == "MINGW"* || "$(uname -s)" == "MSYS"* || "$(uname -s)" == "CYGWIN"* ]]; then
  cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE="${CONFIG}" -DCMAKE_CXX_COMPILER=clang++
  cmake --build build --target package_dist
  exit 0
fi

echo "Best-effort clang path is not available for native Linux builds for this project."
echo "Use Windows with VS 2022 x64 tools and run ./build.bat instead."
exit 1
