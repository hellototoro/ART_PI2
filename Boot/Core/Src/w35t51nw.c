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

#define BOOT_EXTFLASH_TEST_ADDRESS          (0x03FFF000UL)
#define BOOT_EXTFLASH_TEST_SIZE             (256U)

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

void W35T51NW_ExtFlashSelfTest(void)
{
  EXTMEM_NOR_SFDP_FlashInfoTypeDef info = {0};
  EXTMEM_DRIVER_NOR_SFDP_ObjectTypeDef *nor = &extmem_list_config[EXTMEMORY_1].NorSfdpObject;
  uint8_t tx[BOOT_EXTFLASH_TEST_SIZE];
  uint8_t rx[BOOT_EXTFLASH_TEST_SIZE];
  uint32_t map_address = 0U;
  uint32_t test_address = BOOT_EXTFLASH_TEST_ADDRESS;
  uint32_t erase_size;
  EXTMEM_StatusTypeDef status;

  printf("\r\n[BOOT] ART_PI2 W35T51NW XSPI2 bring-up\r\n");
  printf("[BOOT] UART4 115200 8N1\r\n");
  W35T51NW_LowLevelProbe();

  status = EXTMEM_GetInfo(EXTMEMORY_1, &info);
  printf("[BOOT] Driver phy=%u read=0x%02X prog=0x%02X dummy=%lu dqs=%lu prescaler=%lu\r\n",
              (unsigned int)nor->sfdp_private.DriverInfo.SpiPhyLink,
              nor->sfdp_private.DriverInfo.ReadInstruction,
              nor->sfdp_private.DriverInfo.PageProgramInstruction,
              nor->sfdp_private.SALObject.Commandbase.DummyCycles,
              nor->sfdp_private.SALObject.Commandbase.DQSMode,
              hxspi2.Init.ClockPrescaler);
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
  status = EXTMEM_EraseSector(EXTMEMORY_1, test_address, erase_size);
  printf("[BOOT] EXTMEM_EraseSector=%ld\r\n", (long)status);
  if (status != EXTMEM_OK)
  {
    return;
  }

  status = EXTMEM_Write(EXTMEMORY_1, test_address, tx, sizeof(tx));
  printf("[BOOT] EXTMEM_Write=%ld\r\n", (long)status);
  if (status != EXTMEM_OK)
  {
    return;
  }

  status = EXTMEM_Read(EXTMEMORY_1, test_address, rx, sizeof(rx));
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

  status = EXTMEM_MemoryMappedMode(EXTMEMORY_1, EXTMEM_ENABLE);
  printf("[BOOT] EXTMEM_MemoryMappedMode(enable)=%ld\r\n", (long)status);
  if (status == EXTMEM_OK)
  {
    status = EXTMEM_GetMapAddress(EXTMEMORY_1, &map_address);
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

    status = EXTMEM_MemoryMappedMode(EXTMEMORY_1, EXTMEM_DISABLE);
    printf("[BOOT] EXTMEM_MemoryMappedMode(disable)=%ld\r\n", (long)status);
  }
}
