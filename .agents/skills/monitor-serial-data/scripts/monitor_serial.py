#!/usr/bin/env python
"""Monitor serial data with pyserial."""

from __future__ import annotations

import argparse
import sys
from datetime import datetime
from pathlib import Path

try:
    import serial
    from serial import SerialException
except ImportError:
    print(
        "pyserial is not installed. Run "
        ".\\.agents\\skills\\monitor-serial-data\\scripts\\ensure_serial_env.ps1 first.",
        file=sys.stderr,
    )
    sys.exit(2)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Monitor a serial port and print incoming data.",
    )
    parser.add_argument("--port", required=True, help="Serial port name, for example COM3.")
    parser.add_argument(
        "--baud",
        type=int,
        default=115200,
        help="Serial baud rate. Default: 115200.",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=1.0,
        help="Read timeout in seconds. Default: 1.0.",
    )
    parser.add_argument(
        "--log",
        type=Path,
        help="Optional file path to append received serial data.",
    )
    return parser.parse_args()


def decode_chunk(chunk: bytes) -> str:
    return chunk.decode("utf-8", errors="replace")


def append_log(log_path: Path, text: str) -> None:
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with log_path.open("a", encoding="utf-8", newline="") as handle:
        handle.write(text)
        handle.flush()


def main() -> int:
    args = parse_args()

    try:
        with serial.Serial(args.port, args.baud, timeout=args.timeout) as ser:
            started = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            print(f"[{started}] Monitoring {args.port} at {args.baud} baud. Press Ctrl+C to stop.")
            if args.log:
                print(f"Appending received data to {args.log}")

            while True:
                chunk = ser.read(ser.in_waiting or 1)
                if not chunk:
                    continue

                text = decode_chunk(chunk)
                print(text, end="", flush=True)
                if args.log:
                    append_log(args.log, text)

    except KeyboardInterrupt:
        print("\nStopped.")
        return 0
    except SerialException as exc:
        print(f"Serial error: {exc}", file=sys.stderr)
        print("Check the port name, cable, board power, and whether another tool is using the port.", file=sys.stderr)
        return 1
    except OSError as exc:
        print(f"I/O error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
