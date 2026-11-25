#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
IMAGE_NAME="dspico-pico-builder:latest"

if [[ ! -f "${SCRIPT_DIR}/pico-sdk/pico_sdk_init.cmake" ]]; then
  echo "[>] Fetching pico-sdk + tinyusb..."
  rm -rf "${SCRIPT_DIR}/pico-sdk"
  git clone --depth 1 https://github.com/raspberrypi/pico-sdk.git "${SCRIPT_DIR}/pico-sdk"
  git -C "${SCRIPT_DIR}/pico-sdk" submodule update --init --depth 1 lib/tinyusb
fi

build_native() {
  if command -v nproc >/dev/null 2>&1; then
    NPROC="$(nproc)"
  else
    NPROC="$(getconf _NPROCESSORS_ONLN)"
  fi

  cd "${SCRIPT_DIR}"
  if [[ "${1:-}" == "--clean" ]]; then
    rm -rf build
  fi
  mkdir -p build

  if [[ ! -f build/build.ninja ]]; then
    PICO_SDK_PATH="${SCRIPT_DIR}/pico-sdk" \
    CMAKE_POLICY_VERSION_MINIMUM=3.5 \
      cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -B build .
  fi

  CMAKE_BUILD_PARALLEL_LEVEL="${NPROC}" cmake --build build
}

if command -v arm-none-eabi-gcc >/dev/null 2>&1 && command -v cmake >/dev/null 2>&1; then
  echo "[>] Building firmware natively..."
  build_native "${1:-}"
else
  echo "[>] Native arm-none-eabi-gcc not found; building via Docker (${IMAGE_NAME})..."
  if ! docker image inspect "${IMAGE_NAME}" >/dev/null 2>&1; then
    docker build -t "${IMAGE_NAME}" "${SCRIPT_DIR}"
  fi
  CLEAN_FLAG="${1:-}"
  docker run --rm \
    -v "${REPO_ROOT}:/workspace" \
    -w /workspace/firmware \
    "${IMAGE_NAME}" \
    bash -c "if [[ '${CLEAN_FLAG}' == '--clean' ]]; then rm -rf build; fi && mkdir -p build && if [[ ! -f build/build.ninja ]]; then PICO_SDK_PATH=/workspace/firmware/pico-sdk CMAKE_POLICY_VERSION_MINIMUM=3.5 cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -B build .; fi && cmake --build build"
fi

echo "[>] Build complete: ${SCRIPT_DIR}/build/dspico_linux_suite.uf2"
