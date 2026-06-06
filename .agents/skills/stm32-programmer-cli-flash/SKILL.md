---
name: stm32-programmer-cli-flash
description: Flash STM32 firmware with STM32_Programmer_CLI.exe over SWD/JTAG. Use when Codex needs to locate STM32CubeProgrammer CLI, discover firmware artifacts, validate target connectivity, program internal flash from ELF/HEX/BIN files, program external flash with a .stldr loader, reset/start the target, or troubleshoot STM32 flashing failures.
---

# STM32 Programmer CLI Flashing

Use this skill to prepare and run STM32CubeProgrammer CLI commands for STM32 targets. Keep the workflow generic unless the current workspace clearly provides project-specific firmware paths, linker scripts, or external loaders.

## Workflow

1. Locate `STM32_Programmer_CLI.exe`:

   ```powershell
   Get-Command STM32_Programmer_CLI.exe -ErrorAction SilentlyContinue
   ```

   If it is not on `PATH`, check common install paths such as:

   ```powershell
   C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe
   ```

2. Discover candidate artifacts in the project:

   ```powershell
   rg --files | rg "\.(elf|hex|bin|stldr)$"
   ```

   Prefer project build outputs over compiler probe artifacts such as `CMakeDetermineCompilerABI_*.bin`.

3. Connect before programming:

   ```powershell
   STM32_Programmer_CLI.exe --connect port=swd
   ```

   Add connection parameters only when needed, for example `freq=4000`, `mode=UR`, `reset=HWrst`, or `sn=<STLINK_SERIAL>`.

4. Choose the programming flow:

   - Use ELF or HEX for internal flash when possible because addresses are embedded.
   - Use BIN only with an explicit address.
   - For external flash, use BIN plus an explicit external memory address and a matching `.stldr` loader.

5. Derive external download addresses from the active linker script or project configuration. Do not assume `0x90000000` or `0x70000000`; STM32 projects can map OSPI/XSPI/QSPI memories differently.

6. Start or reset after programming only when appropriate for the board and boot flow. Use `--start` for normal run-after-download; add `-rst` or `-hardRst` when the target needs a reset to remap or boot from external memory.

## Command Patterns

Program an ELF or HEX image whose addresses are embedded:

```powershell
STM32_Programmer_CLI.exe --connect port=swd --download "path\to\firmware.elf" --start
```

Program a raw BIN into internal flash:

```powershell
STM32_Programmer_CLI.exe --connect port=swd --download "path\to\firmware.bin" 0x08000000 --start
```

Program a raw BIN into external flash:

```powershell
STM32_Programmer_CLI.exe --connect port=swd --download "path\to\application.bin" 0x90000000 -el "path\to\external_loader.stldr" -hardRst -rst --start
```

Replace `0x90000000` with the external flash base from the active linker script or board documentation.

## External Loader Notes

- Confirm the `.stldr` matches the exact board memory wiring, peripheral instance, bus width, memory type, and timing.
- Some STM32CubeMX generated ExtMemLoader projects build an `.elf`; STM32CubeProgrammer external loader usage expects the same loader copied or installed with a `.stldr` extension.
- If a workspace has an ExtMemLoader project but no `.stldr`, inspect its post-build script or copy the built loader ELF to a `.stldr` file in a writable location for CLI use.
- If external programming fails while internal programming succeeds, check the loader path, download address, loader build configuration, external memory initialization, and whether the board needs hardware reset mode.

## Current Workspace Example

For this ART_PI2 workspace only:

- Boot ELF: `D:\workspaces\Embedded\Boards\ART_PI2\Boot\build\ART_PI2_Boot.elf`
- Application BIN: `D:\workspaces\Embedded\Boards\ART_PI2\Appli\build\ART_PI2_Appli.bin`
- Active application linker script: `Appli\stm32h7rsxx_RAMxspi1_ROMxspi2.ld`, with `__FLASH_BEGIN = 0x70000000`
- ExtMemLoader ELF: `D:\workspaces\Embedded\Boards\ART_PI2\ExtMemLoader\build\ART_PI2_ExtMemLoader.elf`

ART_PI2 Boot programming command:

```powershell
STM32_Programmer_CLI.exe --connect port=swd --download "D:\workspaces\Embedded\Boards\ART_PI2\Boot\build\ART_PI2_Boot.elf" --start
```

## Troubleshooting

- If connection fails, check board power, SWDIO/SWCLK/GND, reset wiring, ST-LINK firmware, and whether another debugger session owns the probe.
- If the target is locked or running code that disables debug, retry with `mode=UR reset=HWrst`.
- If programming reports address or sector errors, confirm the image type and address. ELF/HEX usually do not need a manual address; BIN always does.
- If verification fails, lower SWD frequency and retry, for example `--connect port=swd freq=1000`.
