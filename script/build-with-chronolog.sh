#!/usr/bin/env bash
# Build DFTracer with Chronolog writer backend.
# Run from the dftracer repo root (e.g. /home/grc-iit/dftracer).
#
# Requirements:
#   - CHRONOLOG_INSTALL_DIR: ChronoLog install prefix (e.g. .../chronolog-install/chronolog).
#     Recent ChronoLog installs include spdlog headers and libs, so this is usually enough.
#   - CMAKE_PREFIX_PATH: prefix(es) where DFTracer deps are installed (cpp-logger, brahma,
#     yaml-cpp, etc.). Example: export CMAKE_PREFIX_PATH=/path/to/dftracer-install
#     If missing, build with -DDFTRACER_INSTALL_DEPENDENCIES=ON once to install deps, then
#     reconfigure with CMAKE_PREFIX_PATH=<that-install-prefix>.
#   - patchelf (e.g. apt install patchelf) for core RPATH handling.
#
# Optional (for older ChronoLog installs that do not ship spdlog):
#   - CHRONOLOG_SPACK_ENV=/path/to/ChronoLog  so the script can use that env's view for spdlog.
#   - Or set SPDLOG_INSTALL_DIR to spdlog's prefix.
# If spdlog is still not found, CMake will fetch spdlog via FetchContent.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DFTRACER_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CHRONOLOG_INSTALL_DIR="${CHRONOLOG_INSTALL_DIR:-/home/grc-iit/chronolog-install/chronolog}"
BUILD_DIR="${DFTRACER_ROOT}/build"
INSTALL_PREFIX="${DFTRACER_INSTALL_PREFIX:-${DFTRACER_ROOT}/install}"

# If ChronoLog's Spack env is set, activate it and/or set view path so spdlog is found.
# We do not require 'spack' in PATH: try standard view path, then search under env dir.
if [[ -n "${CHRONOLOG_SPACK_ENV:-}" ]]; then
  ENV_ACTIVATE=""
  ENV_DIR=""
  if [[ -d "$CHRONOLOG_SPACK_ENV" ]]; then
    if [[ -f "$CHRONOLOG_SPACK_ENV/.spack-env/activate.sh" ]]; then
      ENV_ACTIVATE="$CHRONOLOG_SPACK_ENV/.spack-env/activate.sh"
      ENV_DIR="$CHRONOLOG_SPACK_ENV"
    elif command -v spack &>/dev/null; then
      ENV_DIR=$(spack location -e "$CHRONOLOG_SPACK_ENV" 2>/dev/null || true)
      [[ -n "$ENV_DIR" && -f "$ENV_DIR/activate.sh" ]] && ENV_ACTIVATE="$ENV_DIR/activate.sh"
    fi
    [[ -z "$ENV_DIR" ]] && ENV_DIR="$CHRONOLOG_SPACK_ENV"
  elif command -v spack &>/dev/null; then
    ENV_DIR=$(spack location -e "$CHRONOLOG_SPACK_ENV" 2>/dev/null || true)
    [[ -n "$ENV_DIR" && -f "$ENV_DIR/activate.sh" ]] && ENV_ACTIVATE="$ENV_DIR/activate.sh"
  fi
  if [[ -n "$ENV_ACTIVATE" && -f "$ENV_ACTIVATE" ]]; then
    echo "[DFTRACER] Activating ChronoLog Spack env: $CHRONOLOG_SPACK_ENV"
    set +e
    source "$ENV_ACTIVATE"
    set -e
  fi
  # Prefer standard Spack view path, then search for spdlog anywhere under the env dir
  VIEW_FOUND=""
  for VIEW_DIR in "${ENV_DIR}/.spack-env/view" "${CHRONOLOG_SPACK_ENV}/.spack-env/view"; do
    if [[ -n "$VIEW_DIR" && -d "$VIEW_DIR" && -f "$VIEW_DIR/include/spdlog/common.h" ]]; then
      export CMAKE_PREFIX_PATH="${VIEW_DIR}:${CMAKE_PREFIX_PATH}"
      export SPDLOG_INSTALL_DIR="$VIEW_DIR"
      echo "[DFTRACER] Using spdlog from Spack env view: $VIEW_DIR"
      VIEW_FOUND=1
      break
    fi
  done
  if [[ -z "$VIEW_FOUND" && -d "${CHRONOLOG_SPACK_ENV}" ]]; then
    SPDLOG_COMMON=$(find "$CHRONOLOG_SPACK_ENV" -maxdepth 6 -type f -path '*/include/spdlog/common.h' 2>/dev/null | head -1)
    if [[ -n "$SPDLOG_COMMON" ]]; then
      # e.g. .../view/include/spdlog/common.h -> prefix is .../view
      SPDPREFIX="$(cd "$(dirname "$(dirname "$(dirname "$SPDLOG_COMMON")")")" && pwd)"
      export CMAKE_PREFIX_PATH="${SPDPREFIX}:${CMAKE_PREFIX_PATH}"
      export SPDLOG_INSTALL_DIR="$SPDPREFIX"
      echo "[DFTRACER] Using spdlog from Spack env (found under CHRONOLOG_SPACK_ENV): $SPDPREFIX"
    fi
  fi
fi

# Chronolog build needs patchelf to fix core RPATH so dftracer_service links (see CMakeLists.txt).
if ! command -v patchelf &>/dev/null; then
  echo "[DFTRACER] ERROR: patchelf is required for Chronolog build (e.g. apt install patchelf)."
  exit 1
fi

echo "[DFTRACER] Building with Chronolog backend"
echo "  DFTracer root:      $DFTRACER_ROOT"
echo "  ChronoLog install:  $CHRONOLOG_INSTALL_DIR"
echo "  Build dir:          $BUILD_DIR"
echo "  Install prefix:    $INSTALL_PREFIX"
echo "  CMAKE_PREFIX_PATH:  ${CMAKE_PREFIX_PATH:-<not set>}"

if [[ ! -d "$CHRONOLOG_INSTALL_DIR" ]]; then
  echo "ERROR: ChronoLog install dir not found: $CHRONOLOG_INSTALL_DIR"
  exit 1
fi
if [[ ! -d "$CHRONOLOG_INSTALL_DIR/include" || ! -d "$CHRONOLOG_INSTALL_DIR/lib" ]]; then
  echo "ERROR: ChronoLog install dir must contain include/ and lib/: $CHRONOLOG_INSTALL_DIR"
  exit 1
fi
if [[ -f "$CHRONOLOG_INSTALL_DIR/include/spdlog/common.h" ]]; then
  echo "[DFTRACER] ChronoLog install includes spdlog headers."
fi

# Optional: spdlog from Spack (if not already set and spack available)
if [[ -z "${SPDLOG_INSTALL_DIR:-}" ]] && command -v spack &>/dev/null; then
  SPDLOG_PREFIX=$(spack location -i spdlog 2>/dev/null || true)
  if [[ -n "$SPDLOG_PREFIX" && -f "$SPDLOG_PREFIX/include/spdlog/common.h" ]]; then
    export SPDLOG_INSTALL_DIR="$SPDLOG_PREFIX"
    echo "[DFTRACER] Using spdlog from Spack: $SPDLOG_INSTALL_DIR"
  fi
fi
# Hint only when user set CHRONOLOG_SPACK_ENV but we didn't find spdlog (older ChronoLog)
if [[ -z "${SPDLOG_INSTALL_DIR:-}" && -n "${CHRONOLOG_SPACK_ENV:-}" ]]; then
  echo "[DFTRACER] Spdlog not found under CHRONOLOG_SPACK_ENV; CMake will use ChronoLog install or FetchContent."
fi

cd "$DFTRACER_ROOT"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Pass CMAKE_PREFIX_PATH through so Spack-provided deps (e.g. spdlog) are found
CMAKE_EXTRA=()
if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
  CMAKE_EXTRA+=(-DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH")
fi
if [[ -n "${SPDLOG_INSTALL_DIR:-}" ]]; then
  CMAKE_EXTRA+=(-DSPDLOG_INSTALL_DIR="$SPDLOG_INSTALL_DIR")
fi

cmake .. \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
  -DDFTRACER_WRITER_TYPE=CHRONOLOG \
  -DCHRONOLOG_INSTALL_DIR="$CHRONOLOG_INSTALL_DIR" \
  -DDFTRACER_ENABLE_TESTS=ON \
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
echo "  3. export DFTRACER_INIT=PRELOAD"
echo "  4. export LD_PRELOAD=$INSTALL_PREFIX/lib/libdftracer_preload.so"
echo "  5. export LD_LIBRARY_PATH=$INSTALL_PREFIX/lib:$CHRONOLOG_INSTALL_DIR/lib:\$LD_LIBRARY_PATH"
echo "  6. Run your application (e.g. ls -la /tmp)."
echo ""
echo "Quick Chronolog smoke test (env from above, then):"
echo "  mkdir -p /tmp/dftracer_test_data && $BUILD_DIR/bin/test_cpp /tmp/dftracer_test_data 1"
echo ""
echo "If the test aborts with 'free(): corrupted unsorted chunks', run under GDB for a backtrace:"
echo "  sudo apt-get install -y gdb"
echo "  gdb --args $BUILD_DIR/bin/test_cpp /tmp/dftracer_test_data 1"
echo "  Then in GDB: set env DFTRACER_ENABLE=1  (and PRELOAD, LD_LIBRARY_PATH) then run"
