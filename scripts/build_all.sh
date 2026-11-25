#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "=== [1/3] Building RP2040 composite firmware ==="
"${REPO_ROOT}/firmware/compile.sh"

echo "=== [2/3] Building DSpico-TTY (.nds) ==="
if [[ -n "${DEVKITARM:-}" ]] && command -v arm-none-eabi-gcc >/dev/null 2>&1; then
  make -C "${REPO_ROOT}/apps/dspico-tty" clean && make -C "${REPO_ROOT}/apps/dspico-tty"
else
  docker run --rm --platform linux/amd64 \
    -v "${REPO_ROOT}:/workspace" \
    -w /workspace/apps/dspico-tty \
    devkitpro/devkitarm:20241104 \
    bash -c 'make clean && make'
fi

echo "=== [3/3] Building DSpico-HID-Deck (.nds) ==="
if [[ -n "${DEVKITARM:-}" ]] && command -v arm-none-eabi-gcc >/dev/null 2>&1; then
  make -C "${REPO_ROOT}/apps/dspico-hid-deck" clean && make -C "${REPO_ROOT}/apps/dspico-hid-deck"
else
  docker run --rm --platform linux/amd64 \
    -v "${REPO_ROOT}:/workspace" \
    -w /workspace/apps/dspico-hid-deck \
    devkitpro/devkitarm:20241104 \
    bash -c 'make clean && make'
fi

echo "=== All artifacts built ==="
ls -lh "${REPO_ROOT}/firmware/build/dspico_linux_suite.uf2" \
       "${REPO_ROOT}/apps/dspico-tty/dspico-tty.nds" \
       "${REPO_ROOT}/apps/dspico-hid-deck/dspico-hid-deck.nds"
