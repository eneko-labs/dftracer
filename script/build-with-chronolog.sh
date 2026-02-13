#!/usr/bin/env bash
# Build DFTracer with Chronolog writer backend.
# Run from the dftracer repo root (e.g. /home/grc-iit/dftracer).
#
# ChronoLog is built with a Spack env (see ChronoLog/spack.yaml); its headers
# include spdlog, which lives in that env's view, not in ChronoLog's install tree.
# So you must build DFTracer with the same Spack env active:
#
#   export CHRONOLOG_SPACK_ENV=/path/to/ChronoLog   # or your Spack env name
#   bash script/build-with-chronolog.sh
#
# The script will source that env so CMAKE_PREFIX_PATH includes the view (spdlog).
# Or activate the env yourself first:  spack env activate /path/to/ChronoLog
# Set CHRONOLOG_INSTALL_DIR to ChronoLog's install prefix (e.g. .../chronolog-install/chronolog).

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

# spdlog is found from the ChronoLog Spack env view (if activated) or SPDLOG_INSTALL_DIR
if [[ -z "${SPDLOG_INSTALL_DIR:-}" ]] && command -v spack &>/dev/null; then
  SPDLOG_PREFIX=$(spack location -i spdlog 2>/dev/null || true)
  if [[ -n "$SPDLOG_PREFIX" && -f "$SPDLOG_PREFIX/include/spdlog/common.h" ]]; then
    export SPDLOG_INSTALL_DIR="$SPDLOG_PREFIX"
    echo "[DFTRACER] Using spdlog from Spack: $SPDLOG_INSTALL_DIR"
  fi
fi
if [[ -z "${SPDLOG_INSTALL_DIR:-}" ]]; then
  if [[ -n "${CHRONOLOG_SPACK_ENV:-}" ]]; then
    echo "[DFTRACER] Could not find spdlog under CHRONOLOG_SPACK_ENV=$CHRONOLOG_SPACK_ENV"
    echo "  Checked: ${CHRONOLOG_SPACK_ENV}/.spack-env/view and searched up to 6 levels deep."
  fi
  if [[ -z "${CMAKE_PREFIX_PATH:-}" ]]; then
    echo "[DFTRACER] Hint: ChronoLog's headers need spdlog from the same Spack env ChronoLog was built with."
    echo "  Set CHRONOLOG_SPACK_ENV=/path/to/ChronoLog (repo or env name) and re-run, or activate that env first."
  fi
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
  -DFTRACER_ENABLE_TESTS=ON \
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
echo "  5. export LD_LIBRARY_PATH=$CHRONOLOG_INSTALL_DIR/lib:\$LD_LIBRARY_PATH"
echo "  6. Run your application (e.g. ls -la /tmp)."
echo ""
echo "Quick Chronolog smoke test (env from above, then):"
echo "  mkdir -p /tmp/dftracer_test_data && $BUILD_DIR/bin/test_cpp /tmp/dftracer_test_data 1"
