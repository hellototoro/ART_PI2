# ART Pi2 AGENTS.md file

## Build
- Configure and build with:
  `cmake --preset Debug`
  `cmake --build --preset Debug`
- Main artifacts:
  - Boot ELF: `Boot/build/ART_PI2_Boot.elf`
  - App BIN: `Appli/build/ART_PI2_Appli.bin`
  - External loader ELF: `ExtMemLoader/build/ART_PI2_ExtMemLoader.elf`
- For STM32CubeProgrammer external loader usage, copy:
  `ExtMemLoader/build/ART_PI2_ExtMemLoader.elf`
  to:
  `ExtMemLoader/build/ART_PI2_ExtMemLoader.stldr`

## STM32 Debug
- Project target board: ART PI2, MCU STM32H7R7L8HxH.
- Use `STM32_Programmer_CLI.exe` for flashing when ST-LINK is connected.
- Default SWD connection:
  `--connect port=swd mode=UR reset=HWrst freq=4000`
- UART log port depends on the local machine. Verify the COM port before monitoring. The commonly used baudrate in this project is `115200`.
- For serial monitoring, open the serial port before resetting the target when full boot logs are needed.
- Program the external application image before programming the internal boot image when validating the full XIP boot flow.
- The current application external flash base is `0x70000000`.
