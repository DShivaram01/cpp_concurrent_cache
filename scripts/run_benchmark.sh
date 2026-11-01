#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
RESULTS_DIR="${ROOT_DIR}/results"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build "${BUILD_DIR}" --target cache_benchmark -j

mkdir -p "${RESULTS_DIR}"
OUTPUT="${RESULTS_DIR}/benchmark_$(date +%Y%m%d_%H%M%S).txt"

"${BUILD_DIR}/cache_benchmark" "$@" | tee "${OUTPUT}"
echo "Saved results to ${OUTPUT}"
