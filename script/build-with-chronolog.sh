#!/usr/bin/env bash
# Build DFTracer with Chronolog writer backend.
# Run from the dftracer repo root (e.g. /home/grc-iit/dftracer).
#
# Dependencies (ChronoLog and spdlog) should be provided by Spack when possible:
#   spack load chronolog spdlog
#   # or use a Spack env and activate it, then run this script so CMAKE_PREFIX_PATH is set
# Set CHRONOLOG_INSTALL_DIR if ChronoLog is not in your path/Spack view.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DFTRACER_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CHRONOLOG_INSTALL_DIR="${CHRONOLOG_INSTALL_DIR:-/home/grc-iit/chronolog-install/chronolog}"
BUILD_DIR="${DFTRACER_ROOT}/build"
INSTALL_PREFIX="${DFTRACER_INSTALL_PREFIX:-${DFTRACER_ROOT}/install}"

echo "[DFTRACER] Building with Chronolog backend"
echo "  DFTracer root:      $DFTRACER_ROOT"
echo "  ChronoLog install:  $CHRONOLOG_INSTALL_DIR"
echo "  Build dir:          $BUILD_DIR"
echo "  Install prefix:     $INSTALL_PREFIX"
echo "  CMAKE_PREFIX_PATH:  ${CMAKE_PREFIX_PATH:-<not set>}"

if [[ ! -d "$CHRONOLOG_INSTALL_DIR" ]]; then
  echo "ERROR: ChronoLog install dir not found: $CHRONOLOG_INSTALL_DIR"
  exit 1
fi
if [[ ! -d "$CHRONOLOG_INSTALL_DIR/include" || ! -d "$CHRONOLOG_INSTALL_DIR/lib" ]]; then
  echo "ERROR: ChronoLog install dir must contain include/ and lib/: $CHRONOLOG_INSTALL_DIR"
  exit 1
fi

cd "$DFTRACER_ROOT"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Pass CMAKE_PREFIX_PATH through so Spack-provided deps (e.g. spdlog) are found
CMAKE_EXTRA=()
if [[ -n "$CMAKE_PREFIX_PATH" ]]; then
  CMAKE_EXTRA+=(-DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH")
fi

cmake .. \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
  -DDFTRACER_WRITER_TYPE=CHRONOLOG \
  -DCHRONOLOG_INSTALL_DIR="$CHRONOLOG_INSTALL_DIR" \
  "${CMAKE_EXTRA[@]}"

cmake --build . -j$(nproc 2>/dev/null || echo 4)
cmake --install .

echo ""
echo "[DFTRACER] Build and install finished."
echo "  Libraries: $INSTALL_PREFIX/lib"
echo ""
echo "To run an app with Chronolog:"
echo "  1. Start ChronoVisor (e.g. from $CHRONOLOG_INSTALL_DIR/bin or conf)."
echo "  2. export DFTRACER_ENABLE=1"
echo "  3. export LD_PRELOAD=$INSTALL_PREFIX/lib/libdftracer_preload.so"
echo "  4. Run your application."
