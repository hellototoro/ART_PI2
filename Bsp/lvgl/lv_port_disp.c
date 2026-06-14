#include "lv_port_disp.h"

#include "ili9481.h"
#include "lvgl.h"
#include "main.h"
#include <stdint.h>
#include <string.h>

#define LV_PORT_DISP_HOR_RES 320U
#define LV_PORT_DISP_VER_RES 480U
#define LV_PORT_DISP_FB_SIZE (LV_PORT_DISP_HOR_RES * LV_PORT_DISP_VER_RES)

extern LTDC_HandleTypeDef hltdc;

__attribute__((section(".extram_bss"), aligned(32)))
static uint16_t framebuffer[LV_PORT_DISP_FB_SIZE];

static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);
static void clean_framebuffer_cache(void);

void lv_port_disp_init(void)
{
    ili9481_init();

    memset(framebuffer, 0x00, sizeof(framebuffer));
    clean_framebuffer_cache();

    if(HAL_LTDC_SetAddress(&hltdc, (uint32_t)framebuffer, 0U) != HAL_OK) {
        Error_Handler();
    }

    lv_display_t * disp = lv_display_create((int32_t)LV_PORT_DISP_HOR_RES, (int32_t)LV_PORT_DISP_VER_RES);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, disp_flush);
    lv_display_set_buffers(disp, framebuffer, NULL, sizeof(framebuffer), LV_DISPLAY_RENDER_MODE_DIRECT);
}

static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    (void)disp;
    (void)area;
    (void)px_map;

    clean_framebuffer_cache();
    lv_display_flush_ready(disp);
}

static void clean_framebuffer_cache(void)
{
    SCB_CleanDCache_by_Addr((uint32_t *)framebuffer, (int32_t)sizeof(framebuffer));
}
