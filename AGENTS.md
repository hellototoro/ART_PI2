# ART Pi2 AGENTS.md file

## Build
- Configure and build with:
  `cmake --preset Debug`
  `cmake --build --preset Debug`
- Main artifacts:
  - Boot ELF: `Boot/build/ART_PI2_Boot.elf`
  - App BIN: `Appli/build/ART_PI2_Appli.bin`
  - External loader ELF: `ExtMemLoader/build/ART_PI2_ExtMemLoader.stldr`

## STM32 Flash
- Project target board: ART PI2, MCU STM32H7R7L8HxH.
- Use `STM32_Programmer_CLI.exe` for flashing when ST-LINK is connected.
- Use `-c port=swd -d .\Boot\build\ART_PI2_Boot.elf -hardRst -rst --start` parameter for flashing boot.
- Use `-c port=swd -d .\Appli\build\ART_PI2_Appli.bin 0x70000000 -el .\ExtMemLoader\build\ART_PI2_ExtMemLoader.stldr -hardRst -rst --start` parameter for flashing application.
