# ART_PI2 W35T51NW XSPI 外部 Flash 调试记录

日期：2026-06-06

## 目标

本次调试目标是先调通 ART_PI2 开发板上的 W35T51NW 外部 Flash，然后验证完整启动链路：

- MCU 内部 Flash 存放 Boot 程序。
- 外部 Flash 存放 Appli 程序，映射地址为 `0x70000000`。
- Boot 初始化外部 Flash，进入 memory-mapped/XIP 模式，然后跳转到外部 Flash 中的 Appli。
- STM32CubeProgrammer 可以通过自定义外部加载器 `.stldr` 下载 Appli 到外部 Flash。

## 硬件与内存信息

- 开发板：ART_PI2
- MCU：STM32H7R7L8HxH，STM32H7RSxx 系列
- 外部 Flash：W35T51NW，512 Mbit / 64 MB
- 外部 Flash 接口：XSPI2，8-line NOR SFDP
- 外部 Flash 映射基地址：`0x70000000`
- Appli 链接脚本：`Appli/stm32h7rsxx_RAMxspi1_ROMxspi2.ld`
- 外部加载器配置：
  - 设备起始地址：`0x70000000`
  - 设备容量：`0x04000000`
  - 页大小：`256`
  - 扇区大小：`4096`
  - 扇区数量：`16384`

## 最终可用配置

ST 的 EXTMEM SFDP 驱动可以正确识别 W35T51NW，并解析出如下信息：

```text
JEDEC ID: EF 5B 1A 02 00 00
SFDP signature: 53 46 44 50
Flash size: 2^26 bytes
Page size: 256 bytes
Erase sizes: 4096 / 32768 / 65536
PHY: 1S-8S-8S
Read command: 0xCC
Program command: 0x8E
Mapped base: 0x70000000
```

但是，SFDP 初始化后读命令 `0xCC` 的 dummy cycle 被配置为 `8`，实测会导致读回数据整体延迟 8 字节。最终可用的板级修正是将 dummy cycle 强制改为 `16`：

```c
extmem_list_config[0].NorSfdpObject.sfdp_public.MaxFreq = 100 * 1000000u;
extmem_list_config[0].NorSfdpObject.sfdp_public.DtrReadDummyCycle = 16u;

EXTMEM_Init(EXTMEMORY_1, HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_XSPI2));
extmem_list_config[0].NorSfdpObject.sfdp_private.SALObject.Commandbase.DummyCycles = 16u;
```

这个修正必须同时加在：

- `Boot/Core/Src/extmem_manager.c`
- `ExtMemLoader/Core/Src/extmem_manager.c`

原因是 Boot 运行时和 STM32CubeProgrammer 外部加载器都会各自初始化一次外部 Flash。只改 Boot 不改 ExtMemLoader，会出现运行时能读写，但外部下载器下载或校验异常的风险。

## 调试过程

1. 确认工程已经包含 `Boot`、`Appli`、`ExtMemLoader` 和 `STM32_ExtMem_Manager`。
2. 在 Boot 中增加 UART 日志和底层 XSPI 探测命令。
3. 先验证 XSPI2 的基本通信是否正常：

```text
[BOOT] JEDEC ID: EF 5B 1A 02 00 00
[BOOT] SFDP[0..15]: 53 46 44 50 0A 01 02 FF 00 08 01 17 80 00 00 FF
```

4. 验证 EXTMEM SFDP 初始化可以解析到正确容量和擦除粒度：

```text
[BOOT] EXTMEM_GetInfo=0 flash=2^26 page=256 erase=4096/32768/65536/0
```

5. 第一次间接擦写读测试失败，读回数据整体偏移 8 字节：

```text
Expected: A5 A4 A7 A6 A1 A0 A3 A2 AD AC AF AE A9 A8 AB AA
Actual:   00 00 00 00 00 00 00 00 A5 A4 A7 A6 A1 A0 A3 A2
```

6. 在运行时扫描 dummy cycle，发现 `16` 个 dummy cycle 可以通过读回比较：

```text
Dummy 8 first16: 00 00 00 00 00 00 00 00 A5 A4 A7 A6 A1 A0 A3 A2
Dummy scan PASS at 16 cycles
```

7. 将 `16` dummy cycle 作为固定板级修正，分别加入 Boot 和 ExtMemLoader。
8. 生成外部加载器 `.stldr`：

```powershell
Copy-Item -LiteralPath 'ExtMemLoader\build\ART_PI2_ExtMemLoader.elf' `
  -Destination 'ExtMemLoader\build\ART_PI2_ExtMemLoader.stldr' -Force
```

9. 使用 STM32CubeProgrammer 通过 `.stldr` 下载 Appli 到外部 Flash，并 verify 通过。
10. 恢复 Boot 跳转 Appli，最终确认外部 Flash XIP 应用正常运行。

## 已验证命令

构建 Boot：

```powershell
cmake --build .\build
```

执行目录：

```powershell
D:\workspaces\Embedded\Boards\ART_PI2\Boot
```

构建 ExtMemLoader：

```powershell
cmake --build .\build
```

执行目录：

```powershell
D:\workspaces\Embedded\Boards\ART_PI2\ExtMemLoader
```

生成 `.stldr`：

```powershell
Copy-Item -LiteralPath 'ExtMemLoader\build\ART_PI2_ExtMemLoader.elf' `
  -Destination 'ExtMemLoader\build\ART_PI2_ExtMemLoader.stldr' -Force
```

构建 Appli：

```powershell
cmake --build .\build
```

执行目录：

```powershell
D:\workspaces\Embedded\Boards\ART_PI2\Appli
```

注意：当前 Appli 工程构建后不会自动重新生成 `ART_PI2_Appli.bin`。修改 Appli 后，需要手动从 ELF 生成 BIN：

```powershell
arm-none-eabi-objcopy -O binary Appli\build\ART_PI2_Appli.elf Appli\build\ART_PI2_Appli.bin
```

下载 Appli 到外部 Flash：

```powershell
STM32_Programmer_CLI.exe --connect port=swd mode=UR reset=HWrst `
  -el "D:\workspaces\Embedded\Boards\ART_PI2\ExtMemLoader\build\ART_PI2_ExtMemLoader.stldr" `
  --download "D:\workspaces\Embedded\Boards\ART_PI2\Appli\build\ART_PI2_Appli.bin" 0x70000000 `
  --verify
```

烧录 Boot 到内部 Flash：

```powershell
STM32_Programmer_CLI.exe --connect port=swd mode=UR reset=HWrst `
  --download "D:\workspaces\Embedded\Boards\ART_PI2\Boot\build\ART_PI2_Boot.elf" `
  --start
```

串口监控：

```powershell
.\.venv\Scripts\python.exe .\.agents\skills\monitor-serial-data\scripts\monitor_serial.py --port COM4 --baud 115200
```

## 最终验证日志

```text
[BOOT] ART_PI2 W35T51NW XSPI2 bring-up
[BOOT] UART4 115200 8N1
[BOOT] MX_XSPI2_Init done
[BOOT] JEDEC ID: EF 5B 1A 02 00 00
[BOOT] SFDP[0..15]: 53 46 44 50 0A 01 02 FF 00 08 01 17 80 00 00 FF
[BOOT] MX_XSPI1_Init done
[BOOT] MX_EXTMEM_MANAGER_Init begin
[BOOT] MX_EXTMEM_MANAGER_Init done
[BOOT] Driver phy=7 read=0xCC prog=0x8E dummy=16 dqs=0 prescaler=0
[BOOT] EXTMEM_GetInfo=0 flash=2^26 page=256 erase=4096/32768/65536/0
[BOOT] Erase 0x03FFF000 size=4096
[BOOT] EXTMEM_EraseSector=0
[BOOT] EXTMEM_Write=0
[BOOT] EXTMEM_Read=0
[BOOT] External flash indirect erase/write/read PASS
[BOOT] EXTMEM_MemoryMappedMode(enable)=0
[BOOT] EXTMEM_GetMapAddress=0 base=0x70000000
[BOOT] Memory-mapped read PASS
[BOOT] EXTMEM_MemoryMappedMode(disable)=0
[BOOT] Jump to application

[APPLI] ART_PI2 application running from external flash
[APPLI] VTOR=0x70000000
[APPLI] heartbeat
```

## 关键注意事项

### 1. Boot 自测后不要保持 memory-mapped mode

Boot 自测阶段会启用 memory-mapped mode 来验证 XIP 读路径。验证完成后必须先关闭 memory-mapped mode，再调用 `BOOT_Application()`。

推荐流程：

```c
status = EXTMEM_MemoryMappedMode(EXTMEMORY_1, EXTMEM_ENABLE);
/* 验证 memory-mapped 读 */
status = EXTMEM_MemoryMappedMode(EXTMEMORY_1, EXTMEM_DISABLE);

BOOT_Application();
```

原因是 `BOOT_Application()` 内部还会调用 `MapMemory()`，再次进入 memory-mapped mode。如果 Boot 自测已经保持映射状态，后续跳转链路可能异常。

### 2. Boot 和 ExtMemLoader 的 Flash 参数必须一致

Boot 和 ExtMemLoader 都会初始化 W35T51NW。`DummyCycles=16` 这个修正必须两边都加：

- Boot 负责运行时 XIP。
- ExtMemLoader 负责 STM32CubeProgrammer 外部下载和校验。

### 3. 修改 Appli 后要重新生成 BIN

当前 Appli 的 CMake 构建会更新 ELF，但不会自动更新 BIN。曾经出现过 ELF 已更新、BIN 仍是旧文件，导致下载到外部 Flash 的还是旧应用。

修改 Appli 后务必执行：

```powershell
arm-none-eabi-objcopy -O binary Appli\build\ART_PI2_Appli.elf Appli\build\ART_PI2_Appli.bin
```

### 4. 自测擦写地址不要覆盖应用区

Boot 自测使用的是外部 Flash 最后一个 4 KB 扇区：

```c
#define BOOT_EXTFLASH_TEST_ADDRESS  (0x03FFF000UL)
```

这样可以避免擦除 `0x70000000` 起始处的 Appli 镜像。后续增加破坏性擦写测试时，也要避开应用镜像所在区域。`

不同电脑、USB 口或 ST-LINK 枚举顺序下，COM 号可能不同。

## 当前产物

- Boot ELF：`Boot/build/ART_PI2_Boot.elf`
- Appli ELF：`Appli/build/ART_PI2_Appli.elf`
- Appli BIN：`Appli/build/ART_PI2_Appli.bin`
- ExtMemLoader ELF：`ExtMemLoader/build/ART_PI2_ExtMemLoader.elf`
- ExtMemLoader ST-LINK 加载器：`ExtMemLoader/build/ART_PI2_ExtMemLoader.stldr`

## 后续建议

- 给 ExtMemLoader 增加 Windows 下可用的 post-build，自动生成或复制 `.stldr`。
- 生产版 Boot 建议保留最小流程：初始化 XSPI2、进入 memory-mapped mode、跳转 Appli。
