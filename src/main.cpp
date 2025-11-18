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

// --- Pico Enviro+ Pack Button pinout ---
static const int8_t BTN_A = 12;       // GP12 -> Button A (Click)
static const int8_t BTN_B = 13;       // GP13 -> Button B
static const int8_t BTN_X = 14;       // GP14 -> Button X (Left/Right)
static const int8_t BTN_Y = 15;       // GP15 -> Button Y (Up/Down)

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

// LVGL input device
static lv_indev_t * indev;

// Virtual cursor position
static int16_t cursor_x = TFT_HOR_RES / 2;
static int16_t cursor_y = TFT_VER_RES / 2;
static const int16_t cursor_step = 5;  // Pixels to move per button press

// Button state tracking (only for button A click detection)
static bool btn_a_last = false;

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

// LVGL input device read callback for virtual cursor
static void my_input_read_cb(lv_indev_t * indev, lv_indev_data_t * data)
{
    // Read button states (buttons are active LOW with internal pull-up)
    bool btn_a = !digitalRead(BTN_A);  // Click
    bool btn_x = !digitalRead(BTN_X);  // Right
    bool btn_y = !digitalRead(BTN_Y);  // Down
    bool btn_b = !digitalRead(BTN_B);  // Extra button
    
    // Update cursor position based on X button (horizontal movement)
    // Holding X moves cursor right continuously
    if (btn_x) {
        cursor_x += cursor_step;
    }
    
    // Update cursor position based on Y button (vertical movement)
    // Holding Y moves cursor down continuously
    if (btn_y) {
        cursor_y += cursor_step;
    }
    
    // Wrap cursor around screen edges
    if (cursor_x < 0) cursor_x = TFT_HOR_RES - 1;
    if (cursor_x >= TFT_HOR_RES) cursor_x = 0;
    if (cursor_y < 0) cursor_y = TFT_VER_RES - 1;
    if (cursor_y >= TFT_VER_RES) cursor_y = 0;
    
    // Set cursor position
    data->point.x = cursor_x;
    data->point.y = cursor_y;
    
    // Set button state (Button A is click)
    data->state = btn_a ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    
    // Debug output on button A state change
    if (btn_a && !btn_a_last) {
        Serial.printf("Click at (%d, %d)\n", cursor_x, cursor_y);
    }
    btn_a_last = btn_a;
}

// Button click event handler
static void btn_event_cb(lv_event_t * e)
{
    lv_obj_t * btn = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t * label = (lv_obj_t *)lv_event_get_user_data(e);
    
    static int click_count = 0;
    click_count++;
    
    char buf[32];
    snprintf(buf, sizeof(buf), "Clicked %d times!", click_count);
    lv_label_set_text(label, buf);
    
    Serial.printf("Button clicked! Count: %d\n", click_count);
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

  // Init buttons with internal pull-ups (buttons are active LOW)
  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);
  pinMode(BTN_X, INPUT_PULLUP);
  pinMode(BTN_Y, INPUT_PULLUP);

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

  // Create input device for virtual cursor
  indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_input_read_cb);

  // Create a custom cursor (black circle)
  lv_obj_t * cursor_obj = lv_obj_create(lv_screen_active());
  lv_obj_set_size(cursor_obj, 12, 12);
  lv_obj_set_style_radius(cursor_obj, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(cursor_obj, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(cursor_obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(cursor_obj, 2, 0);
  lv_obj_set_style_border_color(cursor_obj, lv_color_white(), 0);
  lv_obj_set_style_border_opa(cursor_obj, LV_OPA_COVER, 0);
  lv_obj_remove_flag(cursor_obj, LV_OBJ_FLAG_CLICKABLE);
  
  // Set the custom cursor for the input device
  lv_indev_set_cursor(indev, cursor_obj);
  
  // Position cursor at center of screen initially
  lv_obj_set_pos(cursor_obj, cursor_x - 6, cursor_y - 6);  // -6 to center the 12px cursor

  // Create a simple UI
  // Create a label
  lv_obj_t * label = lv_label_create(lv_screen_active());
  lv_label_set_text(label, "Use X/Y to move\nButton A to click");
  lv_obj_align(label, LV_ALIGN_CENTER, 0, -40);

  // Create a button
  lv_obj_t * btn = lv_button_create(lv_screen_active());
  lv_obj_set_size(btn, 120, 50);
  lv_obj_align(btn, LV_ALIGN_CENTER, 0, 40);

  // Create a label on the button
  lv_obj_t * btn_label = lv_label_create(btn);
  lv_label_set_text(btn_label, "Click Me!");
  lv_obj_center(btn_label);
  
  // Add click event handler to button
  lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, label);

  Serial.println("LVGL initialized successfully!");
  Serial.println("Controls:");
  Serial.println("  Button X: Move cursor left/right");
  Serial.println("  Button Y: Move cursor up/down");
  Serial.println("  Button A: Click");
}

void loop()
{
  // Handle LVGL tasks
  lv_timer_handler();
  delay(5); // Small delay to let the system breathe
}