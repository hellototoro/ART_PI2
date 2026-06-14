/*
 * ili9481.c
 *
 * ILI9481 RGB panel initialization over the board's GPIO SPI control lines.
 */

#include "ili9481.h"

static void SPI_WriteByte(uint8_t byte)
{
    uint8_t n;

    for(n = 0; n < 8U; n++)
    {
        if((byte & 0x80U) != 0U) {
            HAL_GPIO_WritePin(LCD_SPI_MOSI_GPIO_Port, LCD_SPI_MOSI_Pin, GPIO_PIN_SET);
        }
        else {
            HAL_GPIO_WritePin(LCD_SPI_MOSI_GPIO_Port, LCD_SPI_MOSI_Pin, GPIO_PIN_RESET);
        }

        byte <<= 1;
        HAL_GPIO_WritePin(LCD_SPI_SCK_GPIO_Port, LCD_SPI_SCK_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LCD_SPI_SCK_GPIO_Port, LCD_SPI_SCK_Pin, GPIO_PIN_SET);
    }
}

static void SPI_WriteComm(uint8_t cmd)
{
    HAL_GPIO_WritePin(LCD_SPI_CS_GPIO_Port, LCD_SPI_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_SPI_MOSI_GPIO_Port, LCD_SPI_MOSI_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_SPI_SCK_GPIO_Port, LCD_SPI_SCK_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_SPI_SCK_GPIO_Port, LCD_SPI_SCK_Pin, GPIO_PIN_SET);
    SPI_WriteByte(cmd);
    HAL_GPIO_WritePin(LCD_SPI_CS_GPIO_Port, LCD_SPI_CS_Pin, GPIO_PIN_SET);
}

static void SPI_WriteData(uint8_t data)
{
    HAL_GPIO_WritePin(LCD_SPI_CS_GPIO_Port, LCD_SPI_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_SPI_MOSI_GPIO_Port, LCD_SPI_MOSI_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_SPI_SCK_GPIO_Port, LCD_SPI_SCK_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_SPI_SCK_GPIO_Port, LCD_SPI_SCK_Pin, GPIO_PIN_SET);
    SPI_WriteByte(data);
    HAL_GPIO_WritePin(LCD_SPI_CS_GPIO_Port, LCD_SPI_CS_Pin, GPIO_PIN_SET);
}

void ili9481_init(void)
{
    HAL_GPIO_WritePin(TP_RST_GPIO_Port, TP_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(20U);
    HAL_GPIO_WritePin(TP_RST_GPIO_Port, TP_RST_Pin, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LCD_SPI_CS_GPIO_Port, LCD_SPI_CS_Pin, GPIO_PIN_SET);
    HAL_Delay(20U);
    HAL_GPIO_WritePin(LCD_SPI_CS_GPIO_Port, LCD_SPI_CS_Pin, GPIO_PIN_RESET);

    SPI_WriteComm(0x11U);
    HAL_Delay(20U);

    SPI_WriteComm(0xD0U);
    SPI_WriteData(0x07U);
    SPI_WriteData(0x42U);
    SPI_WriteData(0x19U);

    SPI_WriteComm(0xD1U);
    SPI_WriteData(0x00U);
    SPI_WriteData(0x14U);
    SPI_WriteData(0x1BU);

    SPI_WriteComm(0xD2U);
    SPI_WriteData(0x01U);
    SPI_WriteData(0x12U);

    SPI_WriteComm(0xC0U);
    SPI_WriteData(0x00U);
    SPI_WriteData(0x3BU);
    SPI_WriteData(0x00U);
    SPI_WriteData(0x02U);
    SPI_WriteData(0x01U);

    SPI_WriteComm(0xC5U);
    SPI_WriteData(0x00U);

    SPI_WriteComm(0xC8U);
    SPI_WriteData(0x00U);
    SPI_WriteData(0x46U);
    SPI_WriteData(0x44U);
    SPI_WriteData(0x50U);
    SPI_WriteData(0x04U);
    SPI_WriteData(0x16U);
    SPI_WriteData(0x33U);
    SPI_WriteData(0x13U);
    SPI_WriteData(0x77U);
    SPI_WriteData(0x05U);
    SPI_WriteData(0x0FU);
    SPI_WriteData(0x00U);

    SPI_WriteComm(0xB4U);
    SPI_WriteData(0x10U);

    SPI_WriteComm(0x36U);
    SPI_WriteData(0x88U);

    SPI_WriteComm(0x3AU);
    SPI_WriteData(0x66U);

    SPI_WriteComm(0x2AU);
    SPI_WriteData(0x00U);
    SPI_WriteData(0x00U);
    SPI_WriteData(0x01U);
    SPI_WriteData(0x3FU);

    SPI_WriteComm(0x2BU);
    SPI_WriteData(0x00U);
    SPI_WriteData(0x00U);
    SPI_WriteData(0x01U);
    SPI_WriteData(0xDFU);

    HAL_Delay(120U);
    SPI_WriteComm(0x29U);
    SPI_WriteComm(0x2CU);

    HAL_Delay(10U);
}
