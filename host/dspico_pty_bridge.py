#!/usr/bin/env python3
"""
DSpico-TTY Host PTY Bridge
Bridges the DSpico USB CDC ACM serial port (/dev/dspico-tty or /dev/ttyACM0)
to an interactive Linux shell inside a 24x32 PTY (matching the DS top screen).
"""

import argparse
import errno
import fcntl
import os
import pty
import select
import signal
import struct
import sys
import termios
import time

BAUD_MAP = {
    9600: termios.B9600,
    19200: termios.B19200,
    38400: termios.B38400,
    57600: termios.B57600,
    115200: termios.B115200,
    230400: termios.B230400,
}


def configure_serial(fd: int, baud: int) -> None:
    attrs = termios.tcgetattr(fd)
    # Raw mode: disable canonical input, echo, signals, and CR/NL translation
    attrs[0] &= ~(termios.IGNBRK | termios.BRKINT | termios.PARMRK |
                  termios.ISTRIP | termios.INLCR | termios.IGNCR |
                  termios.ICRNL | termios.IXON | termios.IXOFF | termios.IXANY)
    attrs[1] &= ~termios.OPOST
    attrs[2] &= ~(termios.CSIZE | termios.PARENB | termios.CSTOPB)
    attrs[2] |= termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[3] &= ~(termios.ECHO | termios.ECHONL | termios.ICANON |
                  termios.ISIG | termios.IEXTEN)
    attrs[6][termios.VMIN] = 1
    attrs[6][termios.VTIME] = 0

    speed = BAUD_MAP.get(baud, termios.B115200)
    attrs[4] = speed
    attrs[5] = speed
    termios.tcsetattr(fd, termios.TCSANOW, attrs)


def set_pty_winsize(fd: int, rows: int, cols: int) -> None:
    winsz = struct.pack("HHHH", rows, cols, 0, 0)
    fcntl.ioctl(fd, termios.TIOCSWINSZ, winsz)


def run_session(device: str, baud: int, shell: str, rows: int, cols: int) -> int:
    ser_fd = os.open(device, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        configure_serial(ser_fd, baud)
    except termios.error as exc:
        os.close(ser_fd)
        raise OSError(f"Failed to configure serial port {device}: {exc}") from exc

    # Clear O_NONBLOCK after configuring; we multiplex with select()
    flags = fcntl.fcntl(ser_fd, fcntl.F_GETFL)
    fcntl.fcntl(ser_fd, fcntl.F_SETFL, flags & ~os.O_NONBLOCK)

    pid, pty_fd = pty.fork()
    if pid == 0:
        os.environ["TERM"] = "vt100"
        os.environ["COLUMNS"] = str(cols)
        os.environ["LINES"] = str(rows)
        os.execvp(shell, [shell, "-i"])
        os._exit(127)

    set_pty_winsize(pty_fd, rows, cols)

    banner = (
        f"\r\n\x1b[1;32m[DSpico-TTY connected: {cols}x{rows} @ {baud} baud]\x1b[0m\r\n"
    ).encode("utf-8")
    try:
        os.write(ser_fd, banner)
    except OSError:
        pass

    running = True

    def stop_handler(_signum: int, _frame: object) -> None:
        nonlocal running
        running = False

    prev_int = signal.signal(signal.SIGINT, stop_handler)
    prev_term = signal.signal(signal.SIGTERM, stop_handler)

    try:
        while running:
            try:
                rlist, _, _ = select.select([ser_fd, pty_fd], [], [], 0.25)
            except InterruptedError:
                continue

            if ser_fd in rlist:
                try:
                    data = os.read(ser_fd, 512)
                    if not data:
                        break
                    os.write(pty_fd, data)
                except OSError as exc:
                    if exc.errno in (errno.EAGAIN, errno.EINTR):
                        continue
                    break

            if pty_fd in rlist:
                try:
                    data = os.read(pty_fd, 1024)
                    if not data:
                        break
                    view = memoryview(data)
                    while view:
                        written = os.write(ser_fd, view)
                        view = view[written:]
                except OSError as exc:
                    if exc.errno in (errno.EAGAIN, errno.EINTR):
                        continue
                    break
    finally:
        signal.signal(signal.SIGINT, prev_int)
        signal.signal(signal.SIGTERM, prev_term)
        try:
            os.close(pty_fd)
        except OSError:
            pass
        try:
            os.close(ser_fd)
        except OSError:
            pass
        try:
            os.kill(pid, signal.SIGTERM)
        except OSError:
            pass
        try:
            os.waitpid(pid, 0)
        except OSError:
            pass

    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Bridge DSpico USB CDC serial device to a Linux PTY shell."
    )
    parser.add_argument(
        "-d", "--device",
        default="/dev/dspico-tty" if os.path.exists("/dev/dspico-tty") else "/dev/ttyACM0",
        help="Serial device path (default: /dev/dspico-tty or /dev/ttyACM0)",
    )
    parser.add_argument("-b", "--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument(
        "-s", "--shell",
        default=os.environ.get("SHELL", "/bin/bash"),
        help="Shell executable to spawn (default: $SHELL or /bin/bash)",
    )
    parser.add_argument("--rows", type=int, default=24, help="Terminal rows (default: 24)")
    parser.add_argument("--cols", type=int, default=32, help="Terminal columns (default: 32)")
    parser.add_argument(
        "--reconnect",
        action="store_true",
        help="Wait and automatically reconnect if the DSpico disconnects",
    )
    args = parser.parse_args()

    while True:
        if not os.path.exists(args.device):
            if not args.reconnect:
                print(f"Error: device '{args.device}' not found.", file=sys.stderr)
                return 1
            print(f"Waiting for {args.device}...", file=sys.stderr)
            while not os.path.exists(args.device):
                time.sleep(0.5)

        print(f"Bridging {args.device} -> {args.shell} ({args.cols}x{args.rows})", file=sys.stderr)
        try:
            run_session(args.device, args.baud, args.shell, args.rows, args.cols)
        except OSError as exc:
            print(f"Serial error: {exc}", file=sys.stderr)
            if not args.reconnect:
                return 1

        if not args.reconnect:
            break
        time.sleep(0.5)

    return 0


if __name__ == "__main__":
    sys.exit(main())
