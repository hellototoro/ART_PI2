---
name: monitor-serial-data
description: Use when preparing a Python environment for serial-port work and monitoring UART/COM serial data with pyserial, especially in this STM32/embedded project. Handles checking or creating the project .venv, installing pyserial with uv when available or pip as fallback, and running a serial data monitor with port, baud rate, timeout, duration, and optional log output.
---

# Monitor Serial Data

Use this skill to prepare the project Python environment and monitor serial data from a COM/UART port.

For the current ART_PI2 workspace, verify the UART log port on the local machine before monitoring. The baud rate commonly used in this project is `115200`.

## Workflow

1. Work from the project root.
2. Check whether `.venv` exists.
3. If `.venv` does not exist, create it with `uv venv .venv` when `uv` is available.
4. If `uv` is not available, create it with `python -m venv .venv`.
5. Check whether `pyserial` is installed in `.venv`.
6. If `pyserial` is missing, install it with `uv pip install pyserial` when `uv` is available.
7. If `uv` is not available, install it with `.venv\Scripts\python.exe -m pip install pyserial`.
8. Run the serial monitor with `.venv\Scripts\python.exe .agents\skills\monitor-serial-data\scripts\monitor_serial.py`.
9. When capturing boot logs, open the serial port first, then reset or start the target so the early boot banner is not missed.

## Environment Setup

Use the bundled PowerShell helper from the project root:

```powershell
.\.agents\skills\monitor-serial-data\scripts\ensure_serial_env.ps1
```

The helper should be idempotent: it should not recreate an existing `.venv`, and it should not reinstall `pyserial` when it is already available.

If script execution policy blocks the helper, run:

```powershell
powershell -ExecutionPolicy Bypass -File .\.agents\skills\monitor-serial-data\scripts\ensure_serial_env.ps1
```

## Monitoring Serial Data

Run the monitor with an explicit port:

```powershell
.\.venv\Scripts\python.exe .\.agents\skills\monitor-serial-data\scripts\monitor_serial.py --port COM3 --baud 115200
```

Useful options:

- `--port`: Serial port, such as `COM3`.
- `--baud`: Baud rate. Default is `115200`.
- `--timeout`: Read timeout in seconds for each serial read. Default is `1.0`.
- `--duration`: Optional total capture duration in seconds. When omitted, the monitor runs until `Ctrl+C`.
- `--log`: Optional log file path. When set, received data is appended to the file.

Example with logging:

```powershell
.\.venv\Scripts\python.exe .\.agents\skills\monitor-serial-data\scripts\monitor_serial.py --port COM3 --baud 115200 --duration 8 --log serial.log
```

Stop monitoring with `Ctrl+C`.

Example for bounded ART_PI2 boot-log capture after opening the port:

```powershell
.\.venv\Scripts\python.exe .\.agents\skills\monitor-serial-data\scripts\monitor_serial.py --port COM3 --baud 115200 --duration 8
```

## Troubleshooting

- If the port cannot open, verify the COM port name in Windows Device Manager.
- If access is denied, close other serial terminals or debug tools using the same port.
- If output is garbled, verify baud rate, parity, data bits, stop bits, and firmware UART settings.
- If no data appears, confirm board power, firmware UART transmit path, cable orientation, and USB-UART driver status.
- The serial port number is host-dependent. Do not assume `COM3`, `COM4`, or any fixed port across machines.
- If the first boot lines are missing, open the serial port before issuing `--start`, hardware reset, or releasing reset from ST-LINK.
- `--timeout` is a read timeout, not the total monitor runtime. Use `--duration` for fixed-length captures.

## Bundled Scripts

- `scripts/ensure_serial_env.ps1`: Prepare `.venv` and install or confirm `pyserial`.
- `scripts/monitor_serial.py`: Read serial data, print it to the terminal, and optionally append it to a log file.
