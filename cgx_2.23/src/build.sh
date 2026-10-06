#!/bin/sh
# Configure + build + check the cgx CMake build from wherever this tree lives.
#
#   ./build.sh [extra cmake configure args...]
#   ./build.sh -DCGX_QT_TESTHOOKS=ON -DCGX_BUILD_CHECK=OFF
#
# Environment:
#   CGX_BUILD_DIR    build directory   (default: <src>/build)
#   CGX_JOBS         parallel jobs     (default: nproc)
#   CGX_SKIP_CHECK   =1 to skip the post-build check
#
# Renaming or moving the tree (cgx_2.23 -> cgx_2.24, /opt -> $HOME, ...) keeps
# the build directory, but a CMake cache records the absolute path it was
# configured with. CMake then refuses to continue ("... does not match the
# source ... used to generate cache"), which shows up as a stuck/failed
# configure. This wrapper detects that case and recreates the build directory,
# so the build keeps working after any rename.
set -e

src=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build=${CGX_BUILD_DIR:-$src/build}
jobs=${CGX_JOBS:-$(nproc 2>/dev/null || echo 2)}

if [ ! -f "$src/CMakeLists.txt" ]; then
  echo "build.sh: no CMakeLists.txt in $src" >&2
  exit 1
fi

if [ -f "$build/CMakeCache.txt" ]; then
  home=$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$build/CMakeCache.txt")
  if [ -n "$home" ] && [ "$home" != "$src" ]; then
    echo "build.sh: $build was configured for"
    echo "build.sh:   $home"
    echo "build.sh: but the tree now lives at"
    echo "build.sh:   $src"
    echo "build.sh: (renamed/moved) -> recreating the build directory"
    rm -rf "$build"
  fi
fi

cmake -B "$build" -S "$src" "$@"
cmake --build "$build" -j"$jobs"

if [ "${CGX_SKIP_CHECK:-0}" = 1 ]; then
  echo "build.sh: check skipped (CGX_SKIP_CHECK=1)"
else
  # The check is also a POST_BUILD hook, but that only fires when something
  # relinked; run it explicitly so every ./build.sh ends with a verified binary.
  cmake --build "$build" --target check
fi

echo "build.sh: OK -> $build/cgx"
