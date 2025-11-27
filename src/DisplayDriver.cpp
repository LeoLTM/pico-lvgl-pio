#include "DisplayDriver.h"
#include "Config.h"
#include <Arduino_GFX_Library.h>

static Arduino_DataBus *bus = new Arduino_RPiPicoSPI(
    Pins::LCD_DC, Pins::LCD_CS, Pins::LCD_SCK, Pins::LCD_MOSI, Pins::LCD_MISO
);

static Arduino_GFX *gfx = new Arduino_ST7789(
    bus, Pins::LCD_RST, 0, true
);

static lv_display_t * disp;
static lv_color_t * buf1;
static lv_color_t * buf2;

static uint32_t my_tick_cb(void) {
    return millis();
}

static void my_flush_cb(lv_display_t * display, const lv_area_t * area, uint8_t * px_map) {
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
    lv_display_flush_ready(display);
}

void Display_Init() {
    if (!gfx->begin()) {
        Serial.println("gfx->begin() failed!");
    }
    gfx->fillScreen(RGB565_BLACK);

    pinMode(Pins::LCD_BL, OUTPUT);
    digitalWrite(Pins::LCD_BL, HIGH);

    lv_init();
    lv_tick_set_cb(my_tick_cb);

    disp = lv_display_create(TFT_HOR_RES, TFT_VER_RES);
    uint32_t buf_size = TFT_HOR_RES * TFT_VER_RES / 10;
    buf1 = (lv_color_t *)malloc(buf_size * sizeof(lv_color_t));
    buf2 = (lv_color_t *)malloc(buf_size * sizeof(lv_color_t));

    if(buf1 == NULL || buf2 == NULL) {
        Serial.println("LVGL buffer allocation failed!");
        while(1);
    }

    lv_display_set_buffers(disp, buf1, buf2, buf_size * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, my_flush_cb);
}

void Display_Loop() {
    lv_timer_handler();
}
