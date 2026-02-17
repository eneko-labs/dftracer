#!/usr/bin/env bash
# Run the ChronoLog smoke test under GDB to capture a backtrace on crash.
# Usage: bash script/run-chronolog-test-gdb.sh
# Requires: gdb (apt install gdb), ChronoVisor running, and build/install done.

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DFTRACER_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
INSTALL_PREFIX="${DFTRACER_INSTALL_PREFIX:-${DFTRACER_ROOT}/install}"
CHRONOLOG_INSTALL_DIR="${CHRONOLOG_INSTALL_DIR:-/home/grc-iit/chronolog-install/chronolog}"
BUILD_DIR="${DFTRACER_ROOT}/build"

export DFTRACER_ENABLE=1
export DFTRACER_INIT=PRELOAD
export LD_PRELOAD="${INSTALL_PREFIX}/lib/libdftracer_preload.so"
export LD_LIBRARY_PATH="${INSTALL_PREFIX}/lib:${CHRONOLOG_INSTALL_DIR}/lib:${LD_LIBRARY_PATH:-}"

mkdir -p /tmp/dftracer_test_data

if ! command -v gdb &>/dev/null; then
  echo "Install GDB first: sudo apt-get install -y gdb"
  exit 1
fi

echo "Running test under GDB. On abort, backtrace will be printed."
exec gdb -batch \
     -ex "set environment DFTRACER_ENABLE=1" \
     -ex "set environment DFTRACER_INIT=PRELOAD" \
     -ex "set environment LD_PRELOAD=${LD_PRELOAD}" \
     -ex "set environment LD_LIBRARY_PATH=${INSTALL_PREFIX}/lib:${CHRONOLOG_INSTALL_DIR}/lib" \
     -ex "run" \
     -ex "bt" \
     -ex "quit" \
     --args "${BUILD_DIR}/bin/test_cpp" /tmp/dftracer_test_data 1
