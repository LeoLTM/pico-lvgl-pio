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
static const int8_t BTN_A = 12;       // GP12 -> Button A (Start/Pause)
static const int8_t BTN_B = 13;       // GP13 -> Button B (Reset)
static const int8_t BTN_X = 14;       // GP14 -> Button X (Short Break)
static const int8_t BTN_Y = 15;       // GP15 -> Button Y (Long Break)

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

// Pomodoro Timer Settings (in seconds for testing, change to minutes * 60 for production)
enum TimerMode {
    MODE_WORK = 0,
    MODE_SHORT_BREAK = 1,
    MODE_LONG_BREAK = 2
};

static const uint32_t WORK_DURATION = 25 * 60;        // 25 minutes
static const uint32_t SHORT_BREAK_DURATION = 5 * 60;  // 5 minutes
static const uint32_t LONG_BREAK_DURATION = 15 * 60;  // 15 minutes

// Timer state
static TimerMode current_mode = MODE_WORK;
static uint32_t time_remaining = WORK_DURATION;
static uint32_t last_tick_time = 0;
static bool timer_running = false;

// Button debouncing
static bool btn_a_last = false;
static bool btn_b_last = false;
static bool btn_x_last = false;
static bool btn_y_last = false;
static uint32_t last_button_time = 0;
static const uint32_t DEBOUNCE_DELAY = 200; // ms

// UI objects
static lv_obj_t * timer_label;
static lv_obj_t * mode_label;
static lv_obj_t * status_label;
static lv_obj_t * progress_arc;

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

// Format time in MM:SS format
static void format_time(uint32_t seconds, char* buf, size_t buf_size)
{
    uint32_t minutes = seconds / 60;
    uint32_t secs = seconds % 60;
    snprintf(buf, buf_size, "%02lu:%02lu", minutes, secs);
}

// Get mode name
static const char* get_mode_name(TimerMode mode)
{
    switch(mode) {
        case MODE_WORK: return "WORK TIME";
        case MODE_SHORT_BREAK: return "SHORT BREAK";
        case MODE_LONG_BREAK: return "LONG BREAK";
        default: return "UNKNOWN";
    }
}

// Get mode duration
static uint32_t get_mode_duration(TimerMode mode)
{
    switch(mode) {
        case MODE_WORK: return WORK_DURATION;
        case MODE_SHORT_BREAK: return SHORT_BREAK_DURATION;
        case MODE_LONG_BREAK: return LONG_BREAK_DURATION;
        default: return WORK_DURATION;
    }
}

// Update UI with current timer state
static void update_ui()
{
    // Update timer display
    char time_buf[16];
    format_time(time_remaining, time_buf, sizeof(time_buf));
    lv_label_set_text(timer_label, time_buf);
    
    // Update mode label
    lv_label_set_text(mode_label, get_mode_name(current_mode));
    
    // Update status label
    lv_label_set_text(status_label, timer_running ? "RUNNING" : "PAUSED");
    
    // Update progress arc
    uint32_t total_duration = get_mode_duration(current_mode);
    int32_t progress = (int32_t)((total_duration - time_remaining) * 100 / total_duration);
    lv_arc_set_value(progress_arc, progress);
    
    // Change color based on mode
    if (current_mode == MODE_WORK) {
        lv_obj_set_style_arc_color(progress_arc, lv_color_hex(0xFF6B6B), LV_PART_INDICATOR);
    } else if (current_mode == MODE_SHORT_BREAK) {
        lv_obj_set_style_arc_color(progress_arc, lv_color_hex(0x4ECDC4), LV_PART_INDICATOR);
    } else {
        lv_obj_set_style_arc_color(progress_arc, lv_color_hex(0x95E1D3), LV_PART_INDICATOR);
    }
}

// Start/pause timer (Button A)
static void start_pause_timer()
{
    timer_running = !timer_running;
    if (timer_running) {
        last_tick_time = millis();
        Serial.println("Timer started");
    } else {
        Serial.println("Timer paused");
    }
    update_ui();
}

// Reset timer (Button B)
static void reset_timer()
{
    timer_running = false;
    time_remaining = get_mode_duration(current_mode);
    Serial.println("Timer reset");
    update_ui();
}

// Switch to short break (Button X)
static void set_short_break()
{
    current_mode = MODE_SHORT_BREAK;
    timer_running = false;
    time_remaining = SHORT_BREAK_DURATION;
    Serial.println("Switched to short break");
    update_ui();
}

// Switch to long break (Button Y)
static void set_long_break()
{
    current_mode = MODE_LONG_BREAK;
    timer_running = false;
    time_remaining = LONG_BREAK_DURATION;
    Serial.println("Switched to long break");
    update_ui();
}

// Handle button inputs with debouncing
static void handle_buttons()
{
    uint32_t current_time = millis();
    
    // Only check buttons if enough time has passed since last press
    if (current_time - last_button_time < DEBOUNCE_DELAY) {
        return;
    }
    
    // Read button states (active LOW with pull-up)
    bool btn_a = !digitalRead(BTN_A);
    bool btn_b = !digitalRead(BTN_B);
    bool btn_x = !digitalRead(BTN_X);
    bool btn_y = !digitalRead(BTN_Y);
    
    // Detect button presses (rising edge)
    if (btn_a && !btn_a_last) {
        start_pause_timer();
        last_button_time = current_time;
    }
    
    if (btn_b && !btn_b_last) {
        reset_timer();
        last_button_time = current_time;
    }
    
    if (btn_x && !btn_x_last) {
        set_short_break();
        last_button_time = current_time;
    }
    
    if (btn_y && !btn_y_last) {
        set_long_break();
        last_button_time = current_time;
    }
    
    // Update button states
    btn_a_last = btn_a;
    btn_b_last = btn_b;
    btn_x_last = btn_x;
    btn_y_last = btn_y;
}

// Update timer countdown
static void update_timer()
{
    if (!timer_running) {
        return;
    }
    
    uint32_t current_time = millis();
    uint32_t elapsed = (current_time - last_tick_time) / 1000; // Convert to seconds
    
    if (elapsed >= 1) {
        last_tick_time = current_time;
        
        if (time_remaining > 0) {
            time_remaining--;
            update_ui();
        } else {
            // Timer finished
            timer_running = false;
            Serial.println("Timer finished!");
            
            // Auto-switch mode
            if (current_mode == MODE_WORK) {
                current_mode = MODE_SHORT_BREAK;
                time_remaining = SHORT_BREAK_DURATION;
                Serial.println("Switching to short break");
            } else {
                current_mode = MODE_WORK;
                time_remaining = WORK_DURATION;
                Serial.println("Switching to work time");
            }
            update_ui();
        }
    }
}

void setup(void)
{
#ifdef DEV_DEVICE_INIT
  DEV_DEVICE_INIT();
#endif

  Serial.begin(115200);
  // Serial.setDebugOutput(true);
  while(!Serial);
  Serial.println("Pomodoro Timer with LVGL");

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

  // Create Pomodoro Timer UI
  // Mode label at top
  mode_label = lv_label_create(lv_screen_active());
  lv_label_set_text(mode_label, get_mode_name(current_mode));
  lv_obj_set_style_text_font(mode_label, &lv_font_montserrat_14, 0);
  lv_obj_align(mode_label, LV_ALIGN_TOP_MID, 0, 10);
  
  // Create circular progress arc first (so it appears behind the timer)
  progress_arc = lv_arc_create(lv_screen_active());
  lv_obj_set_size(progress_arc, 180, 180);
  lv_obj_align(progress_arc, LV_ALIGN_CENTER, 0, 0);
  lv_arc_set_range(progress_arc, 0, 100);
  lv_arc_set_value(progress_arc, 0);
  lv_arc_set_bg_angles(progress_arc, 0, 360);
  lv_obj_remove_style(progress_arc, NULL, LV_PART_KNOB);  // Remove the knob
  lv_obj_remove_flag(progress_arc, LV_OBJ_FLAG_CLICKABLE); // Make it non-interactive
  
  // Style the arc with thicker stroke
  lv_obj_set_style_arc_width(progress_arc, 12, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(progress_arc, 12, LV_PART_MAIN);
  lv_obj_set_style_arc_color(progress_arc, lv_color_hex(0xFF6B6B), LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(progress_arc, lv_color_hex(0x333333), LV_PART_MAIN);
  
  // Large timer display in center (on top of arc)
  timer_label = lv_label_create(lv_screen_active());
  char time_buf[16];
  format_time(time_remaining, time_buf, sizeof(time_buf));
  lv_label_set_text(timer_label, time_buf);
  // Use larger font and scale for better visibility
  lv_obj_set_style_text_font(timer_label, &lv_font_montserrat_48, 0);
  lv_obj_align(timer_label, LV_ALIGN_CENTER, 0, -10);
  
  // Status label
  status_label = lv_label_create(lv_screen_active());
  lv_label_set_text(status_label, "PAUSED");
  lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
  lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 50);
  
  // Button instructions
  lv_obj_t * instr_label = lv_label_create(lv_screen_active());
  lv_label_set_text(instr_label, 
    "A: Start/Pause | B: Reset\n"
    "X: Short Break | Y: Long Break");
  lv_obj_set_style_text_font(instr_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_align(instr_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(instr_label, LV_ALIGN_BOTTOM_MID, 0, -5);
  
  // Initialize timer
  last_tick_time = millis();
  update_ui();

  Serial.println("Pomodoro Timer initialized!");
  Serial.println("Controls:");
  Serial.println("  Button A: Start/Pause");
  Serial.println("  Button B: Reset");
  Serial.println("  Button X: Short Break (5 min)");
  Serial.println("  Button Y: Long Break (15 min)");
}

void loop()
{
  // Handle button inputs
  handle_buttons();
  
  // Update timer countdown
  update_timer();
  
  // Handle LVGL tasks
  lv_timer_handler();
  delay(5); // Small delay to let the system breathe
}