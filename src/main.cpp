#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include "pico/stdlib.h"

#define GFX_BL DF_GFX_BL // default backlight pin, you may replace DF_GFX_BL to actual backlight pin


// --- Pico Enviro+ Pack LCD pinout (PIM635) ---
static const int8_t LCD_CS   = 17;    // GP17 -> LCD_CS
static const int8_t LCD_DC   = 16;    // GP16 -> LCD_DC
static const int8_t LCD_SCK  = 18;    // GP18 -> LCD_SCLK (SPI0 SCK)
static const int8_t LCD_MOSI = 19;    // GP19 -> LCD_MOSI (SPI0 TX)
static const int8_t LCD_MISO = -1;    // not connected
static const int8_t LCD_RST  = -1;    // LCD reset is tied to RUN on the Pico
static const int8_t LCD_BL   = 20;    // GP20 -> BACKLIGHT_EN

// Display dimensions
#define TFT_HOR_RES 240
#define TFT_VER_RES 240

/* More data bus class: https://github.com/moononournation/Arduino_GFX/wiki/Data-Bus-Class */
// Arduino_DataBus *bus = create_default_Arduino_DataBus();
Arduino_DataBus *bus = new Arduino_RPiPicoSPI(
    LCD_DC,      // DC
    LCD_CS,      // CS
    LCD_SCK,     // SCK
    LCD_MOSI,    // MOSI
    LCD_MISO     // MISO (unused)
);

/* More display class: https://github.com/moononournation/Arduino_GFX/wiki/Display-Class */
// Arduino_GFX *gfx = new Arduino_ILI9341(bus, DF_GFX_RST, 0 /* rotation */, false /* IPS */);
// ST7789 240x240 IPS panel
Arduino_GFX *gfx = new Arduino_ST7789(
    bus,
    LCD_RST,     // RST (-1 because it's tied to RUN)
    0,           // rotation (0,1,2,3) – try 0 for proper alignment
    true         // IPS
);

// LVGL display buffer
static lv_display_t * disp;
static lv_color_t * buf1;
static lv_color_t * buf2;

// LVGL tick callback
static uint32_t my_tick_cb(void)
{
    return millis();
}

// LVGL flush callback - send rendered content to display
static void my_flush_cb(lv_display_t * display, const lv_area_t * area, uint8_t * px_map)
{
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
    
    // Inform LVGL that flushing is complete
    lv_display_flush_ready(display);
}

void setup(void)
{
#ifdef DEV_DEVICE_INIT
  DEV_DEVICE_INIT();
#endif

  Serial.begin(115200);
  // Serial.setDebugOutput(true);
  // while(!Serial);
  Serial.println("LVGL with ArduinoGFX example");

  // Init Display
  if (!gfx->begin())
  {
    Serial.println("gfx->begin() failed!");
  }
  gfx->fillScreen(RGB565_BLACK);

  // turn on backlight
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  // Initialize LVGL
  lv_init();

  // Set tick source for LVGL
  lv_tick_set_cb(my_tick_cb);

  // Create LVGL display
  disp = lv_display_create(TFT_HOR_RES, TFT_VER_RES);

  // Allocate draw buffers
  // Using 1/10 screen size for each buffer (as recommended in LVGL docs)
  uint32_t buf_size = TFT_HOR_RES * TFT_VER_RES / 10;
  buf1 = (lv_color_t *)malloc(buf_size * sizeof(lv_color_t));
  buf2 = (lv_color_t *)malloc(buf_size * sizeof(lv_color_t));
  
  if(buf1 == NULL || buf2 == NULL) {
    Serial.println("LVGL buffer allocation failed!");
    while(1);
  }

  // Set the buffers for LVGL
  lv_display_set_buffers(disp, buf1, buf2, buf_size * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

  // Set the flush callback
  lv_display_set_flush_cb(disp, my_flush_cb);

  // Create a simple UI
  // Create a label
  lv_obj_t * label = lv_label_create(lv_screen_active());
  lv_label_set_text(label, "Hello LVGL!\nwith ArduinoGFX");
  lv_obj_align(label, LV_ALIGN_CENTER, 0, -40);

  // Create a button
  lv_obj_t * btn = lv_button_create(lv_screen_active());
  lv_obj_set_size(btn, 120, 50);
  lv_obj_align(btn, LV_ALIGN_CENTER, 0, 40);

  // Create a label on the button
  lv_obj_t * btn_label = lv_label_create(btn);
  lv_label_set_text(btn_label, "Click Me!");
  lv_obj_center(btn_label);

  Serial.println("LVGL initialized successfully!");
}

void loop()
{
  // Handle LVGL tasks
  lv_timer_handler();
  delay(5); // Small delay to let the system breathe
}