#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/critical_section.h"
#include "pico/time.h"

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
static lv_obj_t * sim_layer;
static critical_section_t circle_lock;
static bool circle_lock_initialized = false;
static bool circle_worker_started = false;
static volatile uint32_t draw_request_flag = 0;
static constexpr uint32_t FRAME_INTERVAL_MS = 16;
static constexpr uint32_t LVGL_TIMER_POLL_MS = 5;
static uint32_t fallback_last_draw_ms = 0;

static void circle_core_entry();

static inline uint32_t now_ms()
{
  return to_ms_since_boot(get_absolute_time());
}

static inline void signal_frame_request()
{
  draw_request_flag = 1;
}

static inline void request_frame_if_due(uint32_t & last_tick, uint32_t now_tick)
{
  if ((uint32_t)(now_tick - last_tick) >= FRAME_INTERVAL_MS)
  {
    last_tick = now_tick;
    signal_frame_request();
  }
}

constexpr int BUTTON_A_PIN = 12; // top-right
constexpr int BUTTON_B_PIN = 13; // top-left
constexpr int BUTTON_X_PIN = 14; // bottom-right
constexpr int BUTTON_Y_PIN = 15; // bottom-left

namespace circle_sim
{
constexpr size_t MAX_CIRCLES = 32;
constexpr size_t ACTIVE_CIRCLES = 32;
constexpr float MIN_RADIUS = 4.5f;
constexpr float MAX_RADIUS = 15.0f;
constexpr float DT = 0.016f;
constexpr float DRAG = 0.995f;
constexpr float EDGE_BOUNCE = 0.80f;
constexpr float RESTITUTION = 0.82f;
constexpr float GRAVITY_FORCE = 20.0f;
constexpr float GRAVITY_RESPONSE = 32.0f;

struct Circle
{
  float x;
  float y;
  float vx;
  float vy;
  float radius;
  lv_color_t color;
};

struct GravityState
{
  float target_x;
  float target_y;
  float current_x;
  float current_y;
};

static std::array<Circle, MAX_CIRCLES> circles{};
static size_t circle_count = 0;
static GravityState gravity{0.0f, 0.0f, 0.0f, 0.0f};

static constexpr uint32_t palette_hex[] = {
  0xFF8300,
  0xdddddd,
  0x777777,
  0xFF8300,
  0xdddddd,
  0x777777,
  0xffd45f,
  0x63ed8c, // green
  // 0xEE7F3B, // orange
/*   0xD7B44F, // gold
  0x8D96A3  // gre */
};
inline lv_color_t palette_color(size_t idx)
{
  constexpr size_t palette_count = sizeof(palette_hex) / sizeof(palette_hex[0]);
  return lv_color_hex(palette_hex[idx % palette_count]);
}

constexpr int BROAD_COLS = 6;
constexpr int BROAD_ROWS = 6;
constexpr float CELL_W = static_cast<float>(TFT_HOR_RES) / BROAD_COLS;
constexpr float CELL_H = static_cast<float>(TFT_VER_RES) / BROAD_ROWS;

static std::array<uint8_t, MAX_CIRCLES> bucket_x{};
static std::array<uint8_t, MAX_CIRCLES> bucket_y{};

static float pseudo_random(int seed)
{
  uint32_t x = static_cast<uint32_t>(seed) * 747796405u + 2891336453u;
  x = (x >> ((x >> 28) + 4)) ^ x;
  x *= 277803737u;
  x ^= x >> 22;
  return (x & 0xFFFFu) / 65535.0f; // 0..1
}

static bool read_button(int pin)
{
  return digitalRead(pin) == LOW;
}

void update_gravity()
{
  constexpr float diag = 0.70710678f;
  float tx = 0.0f;
  float ty = 0.0f;

  if (read_button(BUTTON_B_PIN)) // top-left
  {
    tx -= diag;
    ty -= diag;
  }
  if (read_button(BUTTON_A_PIN)) // top-right
  {
    tx += diag;
    ty -= diag;
  }
  if (read_button(BUTTON_Y_PIN)) // bottom-left
  {
    tx -= diag;
    ty += diag;
  }
  if (read_button(BUTTON_X_PIN)) // bottom-right
  {
    tx += diag;
    ty += diag;
  }

  float len = std::sqrt(tx * tx + ty * ty);
  if (len > 1.0f)
  {
    tx /= len;
    ty /= len;
  }

  gravity.target_x = tx * GRAVITY_FORCE;
  gravity.target_y = ty * GRAVITY_FORCE;

  const float smoothing = std::min(1.0f, GRAVITY_RESPONSE * DT);
  gravity.current_x += (gravity.target_x - gravity.current_x) * smoothing;
  gravity.current_y += (gravity.target_y - gravity.current_y) * smoothing;
}

void resolve_pair(Circle & a, Circle & b)
{
  float dx = b.x - a.x;
  float dy = b.y - a.y;
  const float min_dist = a.radius + b.radius;
  float dist_sq = dx * dx + dy * dy;
  const float min_dist_sq = min_dist * min_dist;
  if (dist_sq >= min_dist_sq || dist_sq < 1e-6f)
  {
    return;
  }

  float dist = std::sqrt(dist_sq);
  float overlap = min_dist - dist;
  float nx = dx / dist;
  float ny = dy / dist;

  a.x -= nx * (overlap * 0.5f);
  a.y -= ny * (overlap * 0.5f);
  b.x += nx * (overlap * 0.5f);
  b.y += ny * (overlap * 0.5f);

  float va = a.vx * nx + a.vy * ny;
  float vb = b.vx * nx + b.vy * ny;
  float impulse = (-(1.0f + RESTITUTION) * (va - vb)) * 0.5f;
  a.vx += impulse * nx;
  a.vy += impulse * ny;
  b.vx -= impulse * nx;
  b.vy -= impulse * ny;
}

void integrate()
{
  update_gravity();
  for (size_t i = 0; i < circle_count; ++i)
  {
    auto & c = circles[i];
    c.vx += gravity.current_x * DT;
    c.vy += gravity.current_y * DT;
    c.x += c.vx * DT;
    c.y += c.vy * DT;
    c.vx *= DRAG;
    c.vy *= DRAG;

    if (c.x - c.radius < 0.0f)
    {
      c.x = c.radius;
      c.vx = std::fabs(c.vx) * EDGE_BOUNCE;
    }
    else if (c.x + c.radius > TFT_HOR_RES - 1)
    {
      c.x = static_cast<float>(TFT_HOR_RES - 1) - c.radius;
      c.vx = -std::fabs(c.vx) * EDGE_BOUNCE;
    }

    if (c.y - c.radius < 0.0f)
    {
      c.y = c.radius;
      c.vy = std::fabs(c.vy) * EDGE_BOUNCE;
    }
    else if (c.y + c.radius > TFT_VER_RES - 1)
    {
      c.y = static_cast<float>(TFT_VER_RES - 1) - c.radius;
      c.vy = -std::fabs(c.vy) * EDGE_BOUNCE;
    }

    int cell_x = static_cast<int>(c.x / CELL_W);
    int cell_y = static_cast<int>(c.y / CELL_H);
    bucket_x[i] = static_cast<uint8_t>(std::clamp(cell_x, 0, BROAD_COLS - 1));
    bucket_y[i] = static_cast<uint8_t>(std::clamp(cell_y, 0, BROAD_ROWS - 1));
  }

  for (size_t i = 0; i < circle_count; ++i)
  {
    int cx_i = bucket_x[i];
    int cy_i = bucket_y[i];
    for (size_t j = i + 1; j < circle_count; ++j)
    {
      int dx_bucket = cx_i - bucket_x[j];
      int dy_bucket = cy_i - bucket_y[j];
      if (dx_bucket < -1 || dx_bucket > 1 || dy_bucket < -1 || dy_bucket > 1)
      {
        continue;
      }
      resolve_pair(circles[i], circles[j]);
    }
  }
}

void init_circles()
{
  circle_count = std::min(ACTIVE_CIRCLES, MAX_CIRCLES);
  const float center_x = (TFT_HOR_RES - 1) * 0.5f;
  const float center_y = (TFT_VER_RES - 1) * 0.5f;
  for (size_t i = 0; i < circle_count; ++i)
  {
    float radius = MIN_RADIUS + (MAX_RADIUS - MIN_RADIUS) * pseudo_random(static_cast<int>(i * 17));
    float angle = pseudo_random(static_cast<int>(i * 31)) * 6.28318f;
    float radial = 8.0f + 35.0f * pseudo_random(static_cast<int>(i * 43));
    float px = center_x + std::cos(angle) * radial;
    float py = center_y + std::sin(angle) * radial;
    float jitter_vx = (pseudo_random(static_cast<int>(i * 59)) - 0.5f) * 25.0f;
    float jitter_vy = (pseudo_random(static_cast<int>(i * 67)) - 0.5f) * 25.0f;
    circles[i] = {px, py, jitter_vx, jitter_vy, radius, palette_color(i)};
  }
  gravity = {0.0f, 0.0f, 0.0f, 0.0f};
}

void simulate_step()
{
  integrate();
}

const std::array<Circle, MAX_CIRCLES> & data()
{
  return circles;
}

size_t count()
{
  return circle_count;
}

void init()
{
  init_circles();
}
} // namespace circle_sim

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

static void circle_draw_event_cb(lv_event_t * e)
{
  lv_layer_t * layer = lv_event_get_layer(e);
  if (layer == nullptr)
  {
    return;
  }

  lv_draw_rect_dsc_t bg_dsc;
  lv_draw_rect_dsc_init(&bg_dsc);
  bg_dsc.bg_color = lv_color_hex(0x060b12);
  bg_dsc.bg_opa = LV_OPA_COVER;
  bg_dsc.border_opa = LV_OPA_TRANSP;
  lv_area_t full = {0, 0, static_cast<lv_coord_t>(TFT_HOR_RES - 1), static_cast<lv_coord_t>(TFT_VER_RES - 1)};
  lv_draw_rect(layer, &bg_dsc, &full);

  lv_draw_rect_dsc_t circle_dsc;
  lv_draw_rect_dsc_init(&circle_dsc);
  circle_dsc.bg_opa = LV_OPA_COVER;
  circle_dsc.border_opa = LV_OPA_TRANSP;
  circle_dsc.radius = LV_RADIUS_CIRCLE;
  circle_dsc.border_width = 0;

  std::array<circle_sim::Circle, circle_sim::MAX_CIRCLES> snapshot;
  size_t count = 0;
  if (circle_worker_started)
  {
    critical_section_enter_blocking(&circle_lock);
  }
  const auto & circles = circle_sim::data();
  count = circle_sim::count();
  for (size_t i = 0; i < count; ++i)
  {
    snapshot[i] = circles[i];
  }
  if (circle_worker_started)
  {
    critical_section_exit(&circle_lock);
  }

  for (size_t i = 0; i < count; ++i)
  {
    const auto & c = snapshot[i];
    circle_dsc.bg_color = c.color;
    const lv_coord_t left = static_cast<lv_coord_t>(std::clamp(c.x - c.radius, 0.0f, static_cast<float>(TFT_HOR_RES - 1)));
    const lv_coord_t top = static_cast<lv_coord_t>(std::clamp(c.y - c.radius, 0.0f, static_cast<float>(TFT_VER_RES - 1)));
    const lv_coord_t right = static_cast<lv_coord_t>(std::clamp(c.x + c.radius, 0.0f, static_cast<float>(TFT_HOR_RES - 1)));
    const lv_coord_t bottom = static_cast<lv_coord_t>(std::clamp(c.y + c.radius, 0.0f, static_cast<float>(TFT_VER_RES - 1)));
    lv_area_t area = {left, top, right, bottom};
    lv_draw_rect(layer, &circle_dsc, &area);
  }
}

static void circle_timer_cb(lv_timer_t * timer)
{
  LV_UNUSED(timer);
  if (!circle_worker_started)
  {
    const uint32_t now = now_ms();
    circle_sim::simulate_step();
    request_frame_if_due(fallback_last_draw_ms, now);
  }
  if (draw_request_flag && sim_layer)
  {
    draw_request_flag = 0;
    lv_obj_invalidate(sim_layer);
  }
  else if (draw_request_flag)
  {
    draw_request_flag = 0;
  }
}

static void circle_core_entry()
{
  uint32_t last_draw_ms = now_ms();
  while (true)
  {
    critical_section_enter_blocking(&circle_lock);
    circle_sim::simulate_step();
    critical_section_exit(&circle_lock);
    const uint32_t current_ms = now_ms();
    request_frame_if_due(last_draw_ms, current_ms);
    tight_loop_contents();
  }
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

  pinMode(BUTTON_A_PIN, INPUT_PULLUP);
  pinMode(BUTTON_B_PIN, INPUT_PULLUP);
  pinMode(BUTTON_X_PIN, INPUT_PULLUP);
  pinMode(BUTTON_Y_PIN, INPUT_PULLUP);

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

  sim_layer = lv_obj_create(lv_screen_active());
  lv_obj_remove_style_all(sim_layer);
  lv_obj_set_size(sim_layer, TFT_HOR_RES, TFT_VER_RES);
  lv_obj_set_pos(sim_layer, 0, 0);
  lv_obj_add_event_cb(sim_layer, circle_draw_event_cb, LV_EVENT_DRAW_MAIN, nullptr);

  circle_sim::init();
  fallback_last_draw_ms = now_ms();
  signal_frame_request();
  if (!circle_lock_initialized)
  {
    critical_section_init(&circle_lock);
    circle_lock_initialized = true;
  }
  if (!circle_worker_started)
  {
    multicore_launch_core1(circle_core_entry);
    circle_worker_started = true;
  }
  lv_timer_t * timer = lv_timer_create(circle_timer_cb, LVGL_TIMER_POLL_MS, nullptr);
  if (!timer)
  {
    Serial.println("[Circles] Failed to create LVGL timer!");
  }

  Serial.println("LVGL initialized with circle physics");
}

void loop()
{
  // Handle LVGL tasks
  lv_timer_handler();
  delay(5); // Small delay to let the system breathe
}

