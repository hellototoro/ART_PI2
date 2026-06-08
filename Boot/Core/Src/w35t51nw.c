/**
  ******************************************************************************
  * W35T51NW Flash Memory Driver
  * @file    w35t51nw.c
  * @author  Huang Jian
  * @brief   This file provides a set of functions needed to drive the
  *          W35T51NW QSPI Flash memory mounted on ART_PI2 board.
  ******************************************************************************
  */

#include <stdio.h>
#include <string.h>
#include "w35t51nw.h"
#include "extmem_manager.h"
#include "sal/stm32_sal_xspi_api.h"

#define W35T51NW_MEM_ID                     EXTMEMORY_2
#define BOOT_EXTFLASH_BENCH_ADDRESS         (0x00000000UL)
#define BOOT_EXTFLASH_BENCH_SIZE            (64U * 1024U)
#define W35T51NW_VCR_BUS_ADDRESS            (0x00000000UL)
#define W35T51NW_VCR_DUMMY_ADDRESS          (0x00000001UL)
#define W35T51NW_VCR_ODDR_WITH_DS           (0xE7U)
#define W35T51NW_VCR_DUMMY_16               (0x10U)
#define W35T51NW_DDR_DUMMY_CYCLES           (1U)
#define W35T51NW_NEED_BENCH                 (W35T51NW_SELF_TEST || W35T51NW_DDR_TUNE)
#define W35T51NW_NEED_ODDR                  (W35T51NW_DDR_ENABLE || W35T51NW_DDR_TUNE)


#if W35T51NW_SELF_TEST
#define BOOT_EXTFLASH_TEST_ADDRESS          (0x03FFF000UL)
#define BOOT_EXTFLASH_TEST_SIZE             (256U)
#endif /* W35T51NW_SELF_TEST */

#if W35T51NW_NEED_BENCH
static uint8_t bench_buffer[BOOT_EXTFLASH_BENCH_SIZE] __attribute__((aligned(32)));
#endif /* W35T51NW_NEED_BENCH */

#if W35T51NW_DDR_TUNE
static uint8_t ddr_expected[256] __attribute__((aligned(32)));
static uint32_t ddr_expected_checksum;
#endif /* W35T51NW_DDR_TUNE */


static void W35T51NW_DumpBytes(const char *label, const uint8_t *data, uint32_t size)
{
  printf("[BOOT] %s", label);
  for (uint32_t i = 0; i < size; i++)
  {
    printf(" %02X", data[i]);
  }
  printf("\r\n");
}

static HAL_StatusTypeDef W35T51NW_XSPI_ReadCommand(uint8_t instruction,
                                               uint32_t address,
                                               uint32_t address_mode,
                                               uint8_t dummy_cycles,
                                               uint8_t *data,
                                               uint32_t size)
{
  XSPI_RegularCmdTypeDef command = {0};

  command.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
  command.IOSelect           = HAL_XSPI_SELECT_IO_7_0;
  command.Instruction        = instruction;
  command.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
  command.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
  command.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  command.Address            = address;
  command.AddressMode        = address_mode;
  command.AddressWidth       = HAL_XSPI_ADDRESS_24_BITS;
  command.AddressDTRMode     = HAL_XSPI_ADDRESS_DTR_DISABLE;
  command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  command.DataMode           = HAL_XSPI_DATA_1_LINE;
  command.DataLength         = size;
  command.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
  command.DummyCycles        = dummy_cycles;
  command.DQSMode            = HAL_XSPI_DQS_DISABLE;

  if (HAL_XSPI_Command(&hxspi2, &command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_XSPI_Receive(&hxspi2, data, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
}

static void W35T51NW_PrintDriverState(const char *tag)
{
  EXTMEM_DRIVER_NOR_SFDP_ObjectTypeDef *nor = &extmem_list_config[W35T51NW_MEM_ID].NorSfdpObject;

  printf("[BOOT] %s phy=%u read=0x%02X prog=0x%02X dummy=%lu dqs=%lu cmd_ext=%u sfdp_mask=0x%08lX access=0x%02X prescaler=%lu\r\n",
         tag,
         (unsigned int)nor->sfdp_private.DriverInfo.SpiPhyLink,
         nor->sfdp_private.DriverInfo.ReadInstruction,
         nor->sfdp_private.DriverInfo.PageProgramInstruction,
         nor->sfdp_private.SALObject.Commandbase.DummyCycles,
         nor->sfdp_private.SALObject.Commandbase.DQSMode,
         nor->sfdp_private.SALObject.CommandExtension,
         nor->sfdp_private.Sfdp_table_mask,
         nor->sfdp_private.Sfdp_AccessProtocol,
         hxspi2.Init.ClockPrescaler);
}

#if W35T51NW_SELF_TEST
static uint32_t W35T51NW_MinEraseSize(const EXTMEM_NOR_SFDP_FlashInfoTypeDef *info)
{
  uint32_t sizes[4] = {
    info->EraseType1Size,
    info->EraseType2Size,
    info->EraseType3Size,
    info->EraseType4Size
  };
  uint32_t min_size = 0U;

  for (uint32_t i = 0; i < 4U; i++)
  {
    if (sizes[i] != 0U)
    {
      if ((min_size == 0U) || (sizes[i] < min_size))
      {
        min_size = sizes[i];
      }
    }
  }

  return min_size;
}
#endif /* W35T51NW_SELF_TEST */

#if W35T51NW_NEED_BENCH

static void W35T51NW_DwtStart(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint32_t W35T51NW_DwtNow(void)
{
  return DWT->CYCCNT;
}

static uint32_t W35T51NW_Checksum(const uint8_t *data, uint32_t size)
{
  uint32_t sum = 2166136261UL;

  for (uint32_t i = 0U; i < size; i++)
  {
    sum ^= data[i];
    sum *= 16777619UL;
  }

  return sum;
}

static EXTMEM_StatusTypeDef W35T51NW_BenchmarkRead(const char *tag, uint8_t enable_mmap)
{
  EXTMEM_StatusTypeDef status;
  uint32_t map_address = 0U;
  uint32_t start;
  uint32_t cycles;
  uint32_t checksum;

  W35T51NW_DwtStart();
  memset(bench_buffer, 0, sizeof(bench_buffer));
  start = W35T51NW_DwtNow();
  status = EXTMEM_Read(W35T51NW_MEM_ID, BOOT_EXTFLASH_BENCH_ADDRESS,
                       bench_buffer, sizeof(bench_buffer));
  cycles = W35T51NW_DwtNow() - start;
  checksum = W35T51NW_Checksum(bench_buffer, sizeof(bench_buffer));
  printf("[BOOT] %s indirect read %lu bytes: status=%ld cycles=%lu checksum=0x%08lX\r\n",
         tag, (uint32_t)sizeof(bench_buffer), (long)status, cycles, checksum);
  if ((status != EXTMEM_OK) || (enable_mmap == 0U))
  {
    return status;
  }

  status = EXTMEM_MemoryMappedMode(W35T51NW_MEM_ID, EXTMEM_ENABLE);
  printf("[BOOT] %s mmap enable=%ld\r\n", tag, (long)status);
  if (status == EXTMEM_OK)
  {
    status = EXTMEM_GetMapAddress(W35T51NW_MEM_ID, &map_address);
    if (status == EXTMEM_OK)
    {
      const uint8_t *src = (const uint8_t *)(map_address + BOOT_EXTFLASH_BENCH_ADDRESS);

      memset(bench_buffer, 0, sizeof(bench_buffer));
      SCB_InvalidateDCache_by_Addr((uint32_t *)bench_buffer, sizeof(bench_buffer));
      start = W35T51NW_DwtNow();
      memcpy(bench_buffer, src, sizeof(bench_buffer));
      cycles = W35T51NW_DwtNow() - start;
      checksum = W35T51NW_Checksum(bench_buffer, sizeof(bench_buffer));
      printf("[BOOT] %s XIP memcpy %lu bytes: cycles=%lu checksum=0x%08lX\r\n",
             tag, (uint32_t)sizeof(bench_buffer), cycles, checksum);
    }
    else
    {
      printf("[BOOT] %s get map address=%ld\r\n", tag, (long)status);
    }

    status = EXTMEM_MemoryMappedMode(W35T51NW_MEM_ID, EXTMEM_DISABLE);
    printf("[BOOT] %s mmap disable=%ld\r\n", tag, (long)status);
  }

  return status;
}
#endif /* W35T51NW_NEED_BENCH */

#if W35T51NW_DDR_TUNE
static uint8_t W35T51NW_LoadDdrExpected(void)
{
  EXTMEM_StatusTypeDef status;

  status = EXTMEM_Read(W35T51NW_MEM_ID, BOOT_EXTFLASH_BENCH_ADDRESS,
                       ddr_expected, sizeof(ddr_expected));
  if (status != EXTMEM_OK)
  {
    printf("[BOOT] DDR expected read failed: status=%ld\r\n", (long)status);
    return 0U;
  }

  ddr_expected_checksum = W35T51NW_Checksum(ddr_expected, sizeof(ddr_expected));
  printf("[BOOT] DDR expected checksum=0x%08lX\r\n", ddr_expected_checksum);
  W35T51NW_DumpBytes("DDR expected:", ddr_expected, 32U);

  return 1U;
}

static uint32_t W35T51NW_FirstMismatch(const uint8_t *a, const uint8_t *b, uint32_t size)
{
  for (uint32_t i = 0U; i < size; i++)
  {
    if (a[i] != b[i])
    {
      return i;
    }
  }

  return size;
}

static HAL_StatusTypeDef W35T51NW_DDR_ReadManual(uint32_t address,
                                                 uint8_t dummy_cycles,
                                                 uint32_t dqs_mode,
                                                 uint8_t *data,
                                                 uint32_t size)
{
  XSPI_RegularCmdTypeDef command = {0};

  command.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
  command.IOSelect           = HAL_XSPI_SELECT_IO_7_0;
  command.Instruction        = 0xFDFDU;
  command.InstructionMode    = HAL_XSPI_INSTRUCTION_8_LINES;
  command.InstructionWidth   = HAL_XSPI_INSTRUCTION_16_BITS;
  command.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_ENABLE;
  command.Address            = address;
  command.AddressMode        = HAL_XSPI_ADDRESS_8_LINES;
  command.AddressWidth       = HAL_XSPI_ADDRESS_32_BITS;
  command.AddressDTRMode     = HAL_XSPI_ADDRESS_DTR_ENABLE;
  command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  command.DataMode           = HAL_XSPI_DATA_8_LINES;
  command.DataLength         = size;
  command.DataDTRMode        = HAL_XSPI_DATA_DTR_ENABLE;
  command.DummyCycles        = dummy_cycles;
  command.DQSMode            = dqs_mode;

  if (HAL_XSPI_Command(&hxspi2, &command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    (void)HAL_XSPI_Abort(&hxspi2);
    return HAL_ERROR;
  }

  if (HAL_XSPI_Receive(&hxspi2, data, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    (void)HAL_XSPI_Abort(&hxspi2);
    return HAL_ERROR;
  }

  return HAL_OK;
}
#endif /* W35T51NW_DDR_TUNE */

#if W35T51NW_NEED_ODDR
static HAL_StatusTypeDef W35T51NW_WriteVolatileRegisterSDR(uint32_t address, uint8_t value)
{
  XSPI_RegularCmdTypeDef command = {0};

  command.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
  command.IOSelect           = HAL_XSPI_SELECT_IO_7_0;
  command.Instruction        = 0x81U;
  command.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
  command.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
  command.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  command.Address            = address;
  command.AddressMode        = HAL_XSPI_ADDRESS_1_LINE;
  command.AddressWidth       = HAL_XSPI_ADDRESS_32_BITS;
  command.AddressDTRMode     = HAL_XSPI_ADDRESS_DTR_DISABLE;
  command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  command.DataMode           = HAL_XSPI_DATA_1_LINE;
  command.DataLength         = 1U;
  command.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
  command.DummyCycles        = 0U;
  command.DQSMode            = HAL_XSPI_DQS_DISABLE;

  if (HAL_XSPI_Command(&hxspi2, &command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_XSPI_Transmit(&hxspi2, &value, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
}

static HAL_StatusTypeDef W35T51NW_WriteEnableSDR(void)
{
  XSPI_RegularCmdTypeDef command = {0};

  command.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
  command.IOSelect           = HAL_XSPI_SELECT_IO_7_0;
  command.Instruction        = 0x06U;
  command.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
  command.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
  command.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  command.AddressMode        = HAL_XSPI_ADDRESS_NONE;
  command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  command.DataMode           = HAL_XSPI_DATA_NONE;
  command.DummyCycles        = 0U;
  command.DQSMode            = HAL_XSPI_DQS_DISABLE;

  return HAL_XSPI_Command(&hxspi2, &command, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
}

static HAL_StatusTypeDef W35T51NW_EnterODDR(void)
{
  EXTMEM_DRIVER_NOR_SFDP_ObjectTypeDef *nor = &extmem_list_config[W35T51NW_MEM_ID].NorSfdpObject;
  SAL_XSPI_PhysicalLinkTypeDef phy = PHY_LINK_8D8D8D;
  uint32_t dummy = W35T51NW_DDR_DUMMY_CYCLES;
  HAL_StatusTypeDef status;

  status = W35T51NW_WriteEnableSDR();
  printf("[BOOT] DDR WREN for dummy status=%d err=0x%08lX\r\n", (int)status, hxspi2.ErrorCode);
  if (status != HAL_OK)
  {
    (void)HAL_XSPI_Abort(&hxspi2);
    return status;
  }

  status = W35T51NW_WriteVolatileRegisterSDR(W35T51NW_VCR_DUMMY_ADDRESS, W35T51NW_VCR_DUMMY_16);
  printf("[BOOT] DDR set VCR dummy16 status=%d err=0x%08lX\r\n", (int)status, hxspi2.ErrorCode);
  if (status != HAL_OK)
  {
    (void)HAL_XSPI_Abort(&hxspi2);
    return status;
  }

  HAL_Delay(1U);
  status = W35T51NW_WriteEnableSDR();
  printf("[BOOT] DDR WREN for bus status=%d err=0x%08lX\r\n", (int)status, hxspi2.ErrorCode);
  if (status != HAL_OK)
  {
    (void)HAL_XSPI_Abort(&hxspi2);
    return status;
  }

  status = W35T51NW_WriteVolatileRegisterSDR(W35T51NW_VCR_BUS_ADDRESS, W35T51NW_VCR_ODDR_WITH_DS);
  printf("[BOOT] DDR set VCR ODDR+DS status=%d err=0x%08lX\r\n", (int)status, hxspi2.ErrorCode);
  if (status != HAL_OK)
  {
    (void)HAL_XSPI_Abort(&hxspi2);
    return status;
  }

  nor->sfdp_private.DriverInfo.SpiPhyLink = phy;
  nor->sfdp_private.DriverInfo.ReadInstruction = 0xFDU;
  nor->sfdp_private.DriverInfo.PageProgramInstruction = 0x12U;
  nor->sfdp_private.SALObject.CommandExtension = 0U;
  (void)SAL_XSPI_UpdateMemoryType(&nor->sfdp_private.SALObject, SAL_XSPI_ORDERINVERTED);
  if (SAL_XSPI_MemoryConfig(&nor->sfdp_private.SALObject, PARAM_PHY_LINK, &phy) != HAL_OK)
  {
    return HAL_ERROR;
  }
  if (SAL_XSPI_MemoryConfig(&nor->sfdp_private.SALObject, PARAM_DUMMY_CYCLES, &dummy) != HAL_OK)
  {
    return HAL_ERROR;
  }
  nor->sfdp_private.SALObject.Commandbase.DQSMode = HAL_XSPI_DQS_ENABLE;

  return HAL_OK;
}

#if W35T51NW_DDR_ENABLE

void W35T51NW_EnableDdrXipMode(void)
{
  HAL_StatusTypeDef status;

  printf("[BOOT] Enable W35T51NW ODDR 8D-8D-8D for XIP\r\n");
  status = W35T51NW_EnterODDR();
  printf("[BOOT] DDR enable status=%d\r\n", (int)status);
  W35T51NW_PrintDriverState("DDR");
}
#endif /* W35T51NW_DDR_ENABLE */
#endif /* W35T51NW_NEED_ODDR */

#if W35T51NW_DDR_TUNE
static void W35T51NW_DdrTune(void)
{
  HAL_StatusTypeDef status;
  uint8_t rx[sizeof(ddr_expected)] __attribute__((aligned(32)));
  uint8_t found = 0U;

  if (W35T51NW_LoadDdrExpected() == 0U)
  {
    return;
  }

  printf("[BOOT] DDR tune: enter W35T51NW ODDR 8D-8D-8D, volatile only\r\n");
  status = W35T51NW_EnterODDR();
  printf("[BOOT] DDR enter status=%d\r\n", (int)status);
  W35T51NW_PrintDriverState("DDR");
  if (status == HAL_OK)
  {
    for (uint8_t dummy = 1U; dummy <= 24U; dummy++)
    {
      for (uint8_t dqs = 0U; dqs <= 1U; dqs++)
      {
        memset(rx, 0, sizeof(rx));
        status = W35T51NW_DDR_ReadManual(BOOT_EXTFLASH_BENCH_ADDRESS, dummy,
                                         (dqs != 0U) ? HAL_XSPI_DQS_ENABLE : HAL_XSPI_DQS_DISABLE,
                                         rx, sizeof(rx));
        uint32_t checksum = W35T51NW_Checksum(rx, sizeof(rx));
        uint32_t mismatch = W35T51NW_FirstMismatch(rx, ddr_expected, sizeof(rx));
        printf("[BOOT] DDR scan dummy=%u dqs=%u status=%d checksum=0x%08lX %s\r\n",
               dummy, dqs, (int)status, checksum,
               ((status == HAL_OK) && (checksum == ddr_expected_checksum) &&
                (memcmp(rx, ddr_expected, sizeof(rx)) == 0)) ? "MATCH" : "MISS");
        if ((status == HAL_OK) && (mismatch != sizeof(rx)))
        {
          printf("[BOOT] DDR mismatch offset=%lu exp=0x%02X got=0x%02X\r\n",
                 mismatch, ddr_expected[mismatch], rx[mismatch]);
        }
        if ((dummy == 10U) || (dummy == 16U) || (dummy == 20U) || (dummy == 24U))
        {
          W35T51NW_DumpBytes("DDR sample:", rx, 16U);
        }
        if ((status == HAL_OK) && (checksum == ddr_expected_checksum) &&
            (memcmp(rx, ddr_expected, sizeof(rx)) == 0))
        {
          uint32_t tuned_dummy = dummy;
          EXTMEM_DRIVER_NOR_SFDP_ObjectTypeDef *nor = &extmem_list_config[W35T51NW_MEM_ID].NorSfdpObject;

          found = 1U;
          (void)SAL_XSPI_MemoryConfig(&nor->sfdp_private.SALObject, PARAM_DUMMY_CYCLES, &tuned_dummy);
          nor->sfdp_private.SALObject.Commandbase.DQSMode =
            (dqs != 0U) ? HAL_XSPI_DQS_ENABLE : HAL_XSPI_DQS_DISABLE;
          printf("[BOOT] DDR selected dummy=%u dqs=%u\r\n", dummy, dqs);
          W35T51NW_BenchmarkRead("DDR", 1U);
          break;
        }
      }
      if (found != 0U)
      {
        break;
      }
    }

    if (found == 0U)
    {
      printf("[BOOT] DDR scan found no valid read window; keep SDR as production mode\r\n");
    }
  }
}
#endif /* W35T51NW_DDR_TUNE */

void W35T51NW_LowLevelProbe(void)
{
  uint8_t id[6] = {0};
  uint8_t sfdp[16] = {0};

  if (W35T51NW_XSPI_ReadCommand(0x9FU, 0U, HAL_XSPI_ADDRESS_NONE, 0U, id, sizeof(id)) == HAL_OK)
  {
    W35T51NW_DumpBytes("JEDEC ID:", id, sizeof(id));
  }
  else
  {
    printf("[BOOT] JEDEC ID read failed, HAL error=0x%08lX\r\n", hxspi2.ErrorCode);
    (void)HAL_XSPI_Abort(&hxspi2);
  }

  if (W35T51NW_XSPI_ReadCommand(0x5AU, 0U, HAL_XSPI_ADDRESS_1_LINE, 8U, sfdp, sizeof(sfdp)) == HAL_OK)
  {
    W35T51NW_DumpBytes("SFDP[0..15]:", sfdp, sizeof(sfdp));
  }
  else
  {
    printf("[BOOT] SFDP read failed, HAL error=0x%08lX\r\n", hxspi2.ErrorCode);
    (void)HAL_XSPI_Abort(&hxspi2);
  }
}

#if W35T51NW_SELF_TEST
void W35T51NW_ExtFlashSelfTest(void)
{
  EXTMEM_NOR_SFDP_FlashInfoTypeDef info = {0};
  uint8_t tx[BOOT_EXTFLASH_TEST_SIZE];
  uint8_t rx[BOOT_EXTFLASH_TEST_SIZE];
  uint32_t map_address = 0U;
  uint32_t test_address = BOOT_EXTFLASH_TEST_ADDRESS;
  uint32_t erase_size;
  EXTMEM_StatusTypeDef status;

  printf("\r\n[BOOT] ART_PI2 W35T51NW XSPI2 bring-up\r\n");
  printf("[BOOT] UART4 115200 8N1\r\n");
  W35T51NW_LowLevelProbe();

  status = EXTMEM_GetInfo(W35T51NW_MEM_ID, &info);
  W35T51NW_PrintDriverState("SDR");
  printf("[BOOT] EXTMEM_GetInfo=%ld flash=2^%u page=%lu erase=%lu/%lu/%lu/%lu\r\n",
              (long)status,
              info.FlashSize,
              info.PageSize,
              info.EraseType1Size,
              info.EraseType2Size,
              info.EraseType3Size,
              info.EraseType4Size);
  if (status != EXTMEM_OK)
  {
    return;
  }

  erase_size = W35T51NW_MinEraseSize(&info);
  if ((erase_size != 0U) && (test_address % erase_size) != 0U)
  {
    test_address -= test_address % erase_size;
  }

  for (uint32_t i = 0; i < BOOT_EXTFLASH_TEST_SIZE; i++)
  {
    tx[i] = (uint8_t)(0xA5U ^ i);
    rx[i] = 0U;
  }

  printf("[BOOT] Erase 0x%08lX size=%lu\r\n", test_address, erase_size);
  status = EXTMEM_EraseSector(W35T51NW_MEM_ID, test_address, erase_size);
  printf("[BOOT] EXTMEM_EraseSector=%ld\r\n", (long)status);
  if (status != EXTMEM_OK)
  {
    return;
  }

  status = EXTMEM_Write(W35T51NW_MEM_ID, test_address, tx, sizeof(tx));
  printf("[BOOT] EXTMEM_Write=%ld\r\n", (long)status);
  if (status != EXTMEM_OK)
  {
    return;
  }

  status = EXTMEM_Read(W35T51NW_MEM_ID, test_address, rx, sizeof(rx));
  printf("[BOOT] EXTMEM_Read=%ld\r\n", (long)status);
  if (status != EXTMEM_OK)
  {
    return;
  }

  if (memcmp(tx, rx, sizeof(tx)) == 0)
  {
    printf("[BOOT] External flash indirect erase/write/read PASS\r\n");
  }
  else
  {
    W35T51NW_DumpBytes("Expected:", tx, 16U);
    W35T51NW_DumpBytes("Actual:  ", rx, 16U);
    printf("[BOOT] External flash compare FAIL\r\n");
    return;
  }

  W35T51NW_BenchmarkRead("SDR", 1U);

#if W35T51NW_DDR_TUNE
  W35T51NW_DdrTune();
#endif /* W35T51NW_DDR_TUNE */

#if W35T51NW_DDR_ENABLE
  W35T51NW_EnableDdrXipMode();
  W35T51NW_BenchmarkRead("DDR", 1U);
#endif /* W35T51NW_DDR_ENABLE */

  status = EXTMEM_MemoryMappedMode(W35T51NW_MEM_ID, EXTMEM_ENABLE);
  printf("[BOOT] EXTMEM_MemoryMappedMode(enable)=%ld\r\n", (long)status);
  if (status == EXTMEM_OK)
  {
    status = EXTMEM_GetMapAddress(W35T51NW_MEM_ID, &map_address);
    printf("[BOOT] EXTMEM_GetMapAddress=%ld base=0x%08lX\r\n", (long)status, map_address);
    if ((status == EXTMEM_OK) &&
        (memcmp((const void *)(map_address + test_address), tx, sizeof(tx)) == 0))
    {
      printf("[BOOT] Memory-mapped read PASS\r\n");
    }
    else
    {
      printf("[BOOT] Memory-mapped read FAIL\r\n");
    }

    status = EXTMEM_MemoryMappedMode(W35T51NW_MEM_ID, EXTMEM_DISABLE);
    printf("[BOOT] EXTMEM_MemoryMappedMode(disable)=%ld\r\n", (long)status);
  }
}
#endif /* W35T51NW_SELF_TEST */
