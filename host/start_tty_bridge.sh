#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UDEV_SRC="${SCRIPT_DIR}/udev/99-dspico.rules"
UDEV_DST="/etc/udev/rules.d/99-dspico.rules"

DEVICE=""
BAUD="115200"
MODE="pty"
RECONNECT=0

usage() {
  cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Bridge the DSpico USB CDC serial console (/dev/ttyACM0 or /dev/dspico-tty)
to an interactive Linux shell.

Options:
  -d, --device PATH    Serial device path (auto-detects /dev/dspico-tty or /dev/ttyACM0)
  -b, --baud RATE      Baud rate (default: 115200)
  -r, --reconnect      Auto-wait and reconnect on USB unplug/replug (PTY mode)
      --agetty         Launch agetty login prompt on the device (requires root)
      --install-udev   Install 99-dspico.rules to /etc/udev/rules.d/ and reload
  -h, --help           Show this help message
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -d|--device)
      DEVICE="${2:-}"
      shift 2
      ;;
    -b|--baud)
      BAUD="${2:-115200}"
      shift 2
      ;;
    -r|--reconnect)
      RECONNECT=1
      shift
      ;;
    --agetty)
      MODE="agetty"
      shift
      ;;
    --install-udev)
      MODE="install-udev"
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

if [[ "${MODE}" == "install-udev" ]]; then
  if [[ "${EUID}" -ne 0 ]]; then
    echo "Installing udev rules with sudo..."
    exec sudo "$0" --install-udev
  fi
  install -m 0644 "${UDEV_SRC}" "${UDEV_DST}"
  udevadm control --reload-rules
  udevadm trigger
  echo "Installed ${UDEV_DST} and reloaded udev."
  exit 0
fi

if [[ -z "${DEVICE}" ]]; then
  if [[ -e /dev/dspico-tty ]]; then
    DEVICE="/dev/dspico-tty"
  elif [[ -e /dev/ttyACM0 ]]; then
    DEVICE="/dev/ttyACM0"
  else
    DEVICE="/dev/ttyACM0"
  fi
fi

if [[ "${MODE}" == "agetty" ]]; then
  if [[ "${EUID}" -ne 0 ]]; then
    echo "Error: --agetty requires root privileges (run with sudo)." >&2
    exit 1
  fi
  if [[ ! -e "${DEVICE}" ]]; then
    echo "Error: serial device '${DEVICE}' not found." >&2
    exit 1
  fi
  echo "Starting agetty on ${DEVICE} at ${BAUD} baud (TERM=vt100)..."
  exec agetty --noclear -L "${BAUD}" "${DEVICE}" vt100
fi

ARGS=(--device "${DEVICE}" --baud "${BAUD}")
if [[ "${RECONNECT}" -eq 1 ]]; then
  ARGS+=(--reconnect)
fi

exec python3 "${SCRIPT_DIR}/dspico_pty_bridge.py" "${ARGS[@]}"
