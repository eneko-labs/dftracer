#!/usr/bin/env bash
# Run a quick ChronoLog write + read test (e.g. inside Docker with ChronoVisor already running).
#
# Usage:
#   export CHRONOLOG_INSTALL_DIR=/path/to/chronolog/install   # ChronoLog client install
#   export DFTracer is built with CHRONOLOG and installed to INSTALL_PREFIX (or run from build dir)
#   bash script/run-chronolog-write-read-test.sh [install_prefix]
#
# Optional env (defaults shown):
#   DFTRACER_CHRONOLOG_HOST=127.0.0.1
#   DFTRACER_CHRONOLOG_PORT=5555
#   DFTRACER_CHRONOLOG_CHRONICLE_NAME=dftracer_chronicle
#   DFTRACER_CHRONOLOG_STORY_NAME=dftracer_story
#
# If ChronoVisor runs in another container, set DFTRACER_CHRONOLOG_HOST to that container's hostname.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DFTRACER_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
INSTALL_PREFIX="${1:-${DFTRACER_INSTALL_PREFIX:-$DFTRACER_ROOT/install}}"
BUILD_DIR="${DFTRACER_BUILD_DIR:-$DFTRACER_ROOT/build}"
CHRONOLOG_INSTALL_DIR="${CHRONOLOG_INSTALL_DIR:-/home/grc-iit/chronolog-install/chronolog}"

# Where to dump reader output
OUTPUT_FILE="${OUTPUT_FILE:-/tmp/dftracer_chronolog_events.pfw}"

export DFTRACER_CHRONOLOG_HOST="${DFTRACER_CHRONOLOG_HOST:-127.0.0.1}"
export DFTRACER_CHRONOLOG_PORT="${DFTRACER_CHRONOLOG_PORT:-5555}"
# Use a dedicated chronicle/story for this smoke test
export DFTRACER_CHRONOLOG_CHRONICLE_NAME="${DFTRACER_CHRONOLOG_CHRONICLE_NAME:-dftracer_smoke}"
export DFTRACER_CHRONOLOG_STORY_NAME="${DFTRACER_CHRONOLOG_STORY_NAME:-dftracer_smoke_story}"

# Prefer installed binary, then build tree
if [[ -x "$INSTALL_PREFIX/bin/dftracer_chronolog_reader" ]]; then
  READER="$INSTALL_PREFIX/bin/dftracer_chronolog_reader"
elif [[ -x "$BUILD_DIR/dftracer_chronolog_reader" ]]; then
  READER="$BUILD_DIR/dftracer_chronolog_reader"
else
  echo "ERROR: dftracer_chronolog_reader not found. Build with CHRONOLOG and install or set INSTALL_PREFIX/BUILD_DIR."
  exit 1
fi

if [[ -x "$INSTALL_PREFIX/bin/test_cpp" ]]; then
  TEST_CPP="$INSTALL_PREFIX/bin/test_cpp"
elif [[ -x "$BUILD_DIR/bin/test_cpp" ]]; then
  TEST_CPP="$BUILD_DIR/bin/test_cpp"
else
  echo "ERROR: test_cpp not found. Build with CHRONOLOG and install or set INSTALL_PREFIX/BUILD_DIR."
  exit 1
fi

export LD_LIBRARY_PATH="${INSTALL_PREFIX}/lib:${CHRONOLOG_INSTALL_DIR}/lib:${LD_LIBRARY_PATH}"
export DFTRACER_ENABLE=1
export DFTRACER_INIT=PRELOAD
export LD_PRELOAD="${INSTALL_PREFIX}/lib/libdftracer_preload.so"
export DFTRACER_LOG_FILE=/tmp/dftracer_chronolog_test

echo "[DFTRACER] ChronoLog write+read test"
echo "  ChronoVisor: $DFTRACER_CHRONOLOG_HOST:$DFTRACER_CHRONOLOG_PORT"
echo "  Chronicle:   $DFTRACER_CHRONOLOG_CHRONICLE_NAME / $DFTRACER_CHRONOLOG_STORY_NAME"
echo "  Reader:      $READER"
echo "  Output:      $OUTPUT_FILE"
echo ""

# 1) Run tracer (writes to ChronoLog)
echo "[1/3] Running tracer (test_cpp)..."
mkdir -p /tmp/dftracer_test_data
"$TEST_CPP" /tmp/dftracer_test_data 1
echo "  Done."
echo ""

# 2) Drain events with reader (one-shot)
echo "[2/3] Reading events with dftracer_chronolog_reader --once..."
rm -f "$OUTPUT_FILE"
"$READER" --once --output "$OUTPUT_FILE"
echo "  Done."
echo ""

# 3) Show result
echo "[3/3] Event count and first 3 lines:"
COUNT=$(wc -l < "$OUTPUT_FILE" 2>/dev/null || echo 0)
echo "  Lines (events): $COUNT"
if [[ "$COUNT" -gt 0 ]]; then
  echo "  First 3 lines:"
  head -n 3 "$OUTPUT_FILE" | sed 's/^/    /'
fi
echo ""
echo "Success. Full trace: $OUTPUT_FILE"
