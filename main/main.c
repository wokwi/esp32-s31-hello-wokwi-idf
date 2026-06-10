// SPDX-FileCopyrightText: 2026 Wokwi
// SPDX-License-Identifier: MIT
//
// ESP32-S31 hello-world demo for the Wokwi simulator.
// Drives a wokwi-ili9341 over SPI2 + GDMA via esp_lcd, renders via LVGL.

#include <stdio.h>

#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

static const char *TAG = "hello-s31";

// Pin map matches diagram.json
#define PIN_MOSI    35
#define PIN_SCK     36
#define PIN_CS      37
#define PIN_DC      38
#define PIN_RST     39

#define LCD_HOST            SPI2_HOST
#define LCD_PCLK_HZ         (40 * 1000 * 1000)
// Native portrait orientation of the ILI9341 panel.
#define LCD_WIDTH           240
#define LCD_HEIGHT          320

static lv_obj_t *tick_label;

static void build_ui(lv_display_t *disp) {
    lv_obj_t *scr = lv_display_get_screen_active(disp);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x06102A), LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Hello ESP32-S31");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFC83D), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 60);

    lv_obj_t *sub = lv_label_create(scr);
    lv_label_set_text(sub, "Welcome to Wokwi!");
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(sub, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 102);

    tick_label = lv_label_create(scr);
    lv_label_set_text(tick_label, "Tick: 0");
    lv_obj_set_style_text_font(tick_label, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(tick_label, lv_color_hex(0x32E6C3), 0);
    lv_obj_align(tick_label, LV_ALIGN_CENTER, 0, 60);
}

void app_main(void) {
    ESP_LOGI(TAG, "Hello ESP32-S31");
    ESP_LOGI(TAG, "Initialising SPI bus on host %d (MOSI=%d, SCK=%d)", LCD_HOST, PIN_MOSI, PIN_SCK);

    spi_bus_config_t buscfg = ILI9341_PANEL_BUS_SPI_CONFIG(PIN_SCK, PIN_MOSI,
                                                          LCD_HEIGHT * 40 * sizeof(uint16_t));
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Installing panel IO (CS=%d, DC=%d, pclk=%d Hz)", PIN_CS, PIN_DC, LCD_PCLK_HZ);
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_config = ILI9341_PANEL_IO_SPI_CONFIG(PIN_CS, PIN_DC, NULL, NULL);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io));

    ESP_LOGI(TAG, "Installing ILI9341 panel (RST=%d)", PIN_RST);
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io, &panel_config, &panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

    ESP_LOGI(TAG, "Starting LVGL port...");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = LCD_WIDTH * 40,
        .double_buffer = true,
        .hres = LCD_WIDTH,
        .vres = LCD_HEIGHT,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = true,
            .mirror_y = false,
        },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = true,
            .swap_bytes = true,
        },
    };
    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);

    lvgl_port_lock(0);
    build_ui(disp);
    lvgl_port_unlock();

    char buf[32];
    int tick = 0;
    while (1) {
        snprintf(buf, sizeof(buf), "Tick: %d", tick);
        ESP_LOGI(TAG, "%s", buf);
        if (lvgl_port_lock(100)) {
            lv_label_set_text(tick_label, buf);
            lvgl_port_unlock();
        }
        tick++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
