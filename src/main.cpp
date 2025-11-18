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

// --- Pong game globals ---
// Player button GPIO pins - customize to your wiring
// --- Pico Enviro+ Pack Button pinout (Pimoroni mapping) ---
// Physical layout: top-left = BTN_B, top-right = BTN_A, bottom-right = BTN_X, bottom-left = BTN_Y
static const int BTN_A = 12;       // GP12 -> Button A (top-right)
static const int BTN_B = 13;       // GP13 -> Button B (top-left)
static const int BTN_X = 14;       // GP14 -> Button X (bottom-right)
static const int BTN_Y = 15;       // GP15 -> Button Y (bottom-left)

// Game constants
static const int WIN_SCORE = 5;
static const int PADDLE_W = 60;
static const int PADDLE_H = 10;
static const int BALL_SIZE = 8;
static const int POWER_ACTIVATE_DELAY = 200;

// Game objects
static lv_obj_t * paddle_top = NULL;
static lv_obj_t * paddle_bottom = NULL;
static lv_obj_t * ball = NULL;
// small and large score labels for each side
static lv_obj_t * p1_small_label = NULL;
static lv_obj_t * p1_score_label = NULL;
static lv_obj_t * p2_small_label = NULL;
static lv_obj_t * p2_score_label = NULL;
static lv_obj_t * win_label_top = NULL;
static lv_obj_t * win_label_bottom = NULL;
static lv_obj_t * countdown_label = NULL;
static lv_timer_t * countdown_timer = NULL;
static int countdown_value = 0;
static bool playing = false;

// Game state
static int paddle_top_x = 0;
static int paddle_bottom_x = 0;
static float ball_x = 0.0f;
static float ball_y = 0.0f;
static float ball_vx = 0.0f;
static float ball_vy = 0.0f;
static int score_top = 0;
static int score_bottom = 0;
static bool game_over = false;

// Ultimate powerup state
static int ult_count_top = 0;
static int ult_count_bottom = 0;
static uint32_t ult_next_charge_top = 0;
static uint32_t ult_next_charge_bottom = 0;
static bool ult_active_top = false;
static bool ult_active_bottom = false;
static uint32_t ult_hold_top_ms = 0;
static uint32_t ult_hold_bottom_ms = 0;
// Three-segment vertical ultimate bars
static lv_obj_t * ult_container_top = NULL;
static lv_obj_t * ult_seg_top[3] = {NULL, NULL, NULL};
static lv_obj_t * ult_container_bottom = NULL;
static lv_obj_t * ult_seg_bottom[3] = {NULL, NULL, NULL};

// Ball pause/ultimate state
static bool ball_ultra = false;
static int ball_ultra_hits_remaining = 0;
static uint32_t ball_pause_until_ms = 0;
static float ball_vx_target = 0.0f;
static float ball_vy_target = 0.0f;

// Helper to reset ball to center and give it a direction
static void reset_ball(void) {
  ball_x = TFT_HOR_RES / 2.0f;
  ball_y = TFT_VER_RES / 2.0f;
  ball_vx = ((random(0, 2) == 0) ? 1.4f : -1.4f);
  ball_vy = (random(0, 2) == 0) ? 1.4f : -1.4f;
  if (ball) lv_obj_set_pos(ball, (int)ball_x, (int)ball_y);
  // reset ultimate state and set random timers
  ult_count_top = 0;
  ult_count_bottom = 0;
  ult_active_top = false;
  ult_active_bottom = false;
  ball_ultra = false;
  ball_ultra_hits_remaining = 0;
  // Set random timers for next segment charge (2-6 seconds)
  ult_next_charge_top = millis() + random(2000, 6000);
  ult_next_charge_bottom = millis() + random(2000, 6000);
  // Clear paddle outline if any
  if (paddle_top) {
    lv_obj_set_style_outline_width(paddle_top, 0, 0);
  }
  if (paddle_bottom) {
    lv_obj_set_style_outline_width(paddle_bottom, 0, 0);
  }
}
// forward decl for countdown start
static void start_countdown(void);

// Explosion animation context
struct explosion_ctx_t {
  lv_obj_t * obj;
  int frame;
  int max_frames;
  int max_size;
  int cx;
  int cy;
};
static void create_explosion(int cx, int cy);

// LVGL timer callback
static void game_update(lv_timer_t * t) {
  // ball movement should not move while countdown or while specific ball pause
  // paddles and other input are still processed when !playing (so players can practice moving)
  // but if the whole game isn't playing, skip additional updates like scoring
  // read buttons (active LOW)
  bool a_pressed = digitalRead(BTN_A) == LOW;
  bool b_pressed = digitalRead(BTN_B) == LOW;
  bool x_pressed = digitalRead(BTN_X) == LOW;
  bool y_pressed = digitalRead(BTN_Y) == LOW;

  // if game over and a button is pressed -> restart
  if (game_over && (a_pressed || b_pressed || x_pressed || y_pressed)) {
    // restart game after win - clear scores and start countdown
    game_over = false;
    score_top = 0;
    score_bottom = 0;
    if(p1_score_label) lv_label_set_text_fmt(p1_score_label, "%d", score_top);
    if(p2_score_label) lv_label_set_text_fmt(p2_score_label, "%d", score_bottom);
    if(win_label_top) lv_label_set_text(win_label_top, "");
    if(win_label_bottom) lv_label_set_text(win_label_bottom, "");
    start_countdown();
    return;
  }

  if (game_over) return; // don't process movement while win screen is shown

  // Move paddles
  const int paddle_speed = 4;
  if (a_pressed) paddle_top_x += paddle_speed; // A -> player1 right
  if (b_pressed) paddle_top_x -= paddle_speed; // B -> player1 left
  if (x_pressed) paddle_bottom_x += paddle_speed; // X -> player2 right
  if (y_pressed) paddle_bottom_x -= paddle_speed; // Y -> player2 left

  // clamp paddle position
  if (paddle_top_x < 0) paddle_top_x = 0;
  if (paddle_top_x > TFT_HOR_RES - PADDLE_W) paddle_top_x = TFT_HOR_RES - PADDLE_W;
  if (paddle_bottom_x < 0) paddle_bottom_x = 0;
  if (paddle_bottom_x > TFT_HOR_RES - PADDLE_W) paddle_bottom_x = TFT_HOR_RES - PADDLE_W;
  if (paddle_top) lv_obj_set_pos(paddle_top, paddle_top_x, 8);
  if (paddle_bottom) lv_obj_set_pos(paddle_bottom, paddle_bottom_x, TFT_VER_RES - PADDLE_H - 8);

  // Ultimate charge with random timers (increment counter when timer expires)
  uint32_t now = millis();
  if (!ult_active_top && ult_count_top < 3 && now >= ult_next_charge_top) {
    ult_count_top++;
    if (ult_count_top < 3) {
      ult_next_charge_top = now + random(2000, 6000);
    }
  }
  if (!ult_active_bottom && ult_count_bottom < 3 && now >= ult_next_charge_bottom) {
    ult_count_bottom++;
    if (ult_count_bottom < 3) {
      ult_next_charge_bottom = now + random(2000, 6000);
    }
  }
  
  // Update 3-segment bars (show as grey, turn green when charged)
  for (int i = 0; i < 3; i++) {
    if (ult_seg_top[i]) {
      if (i < ult_count_top) {
        lv_obj_set_style_bg_color(ult_seg_top[i], lv_color_hex(0x00FF00), 0);
      } else {
        lv_obj_set_style_bg_color(ult_seg_top[i], lv_color_hex(0x444444), 0);
      }
    }
    if (ult_seg_bottom[i]) {
      if (i < ult_count_bottom) {
        lv_obj_set_style_bg_color(ult_seg_bottom[i], lv_color_hex(0x00FF00), 0);
      } else {
        lv_obj_set_style_bg_color(ult_seg_bottom[i], lv_color_hex(0x444444), 0);
      }
    }
  }
  
  // Show/hide green stroke around bar when fully charged
  if (ult_container_top) {
    if (ult_count_top >= 3 && !ult_active_top) {
      lv_obj_set_style_outline_width(ult_container_top, 2, 0);
    } else {
      lv_obj_set_style_outline_width(ult_container_top, 0, 0);
    }
  }
  if (ult_container_bottom) {
    if (ult_count_bottom >= 3 && !ult_active_bottom) {
      lv_obj_set_style_outline_width(ult_container_bottom, 2, 0);
    } else {
      lv_obj_set_style_outline_width(ult_container_bottom, 0, 0);
    }
  }

  // Ultimate activation: hold both movement buttons for >= 1 second (200ms)
  const uint32_t tick_ms = 20;
  if (a_pressed && b_pressed) {
    ult_hold_top_ms += tick_ms;
    if (ult_hold_top_ms >= POWER_ACTIVATE_DELAY && ult_count_top >= 3 && !ult_active_top) {
      ult_active_top = true;
      ult_count_top = 0;
      ult_next_charge_top = millis() + random(2000, 6000);
    }
  } else {
    ult_hold_top_ms = 0;
  }
  if (x_pressed && y_pressed) {
    ult_hold_bottom_ms += tick_ms;
    if (ult_hold_bottom_ms >= POWER_ACTIVATE_DELAY && ult_count_bottom >= 3 && !ult_active_bottom) {
      ult_active_bottom = true;
      ult_count_bottom = 0;
      ult_next_charge_bottom = millis() + random(2000, 6000);
    }
  } else {
    ult_hold_bottom_ms = 0;
  }
  
  // Apply green outline to paddle while ultimate is active (with gap using outline_pad)
  if (paddle_top) {
    if (ult_active_top) {
      lv_obj_set_style_outline_width(paddle_top, 3, 0);
      lv_obj_set_style_outline_color(paddle_top, lv_color_hex(0x00FF00), 0);
      lv_obj_set_style_outline_opa(paddle_top, LV_OPA_COVER, 0);
      lv_obj_set_style_outline_pad(paddle_top, 2, 0);
    } else {
      lv_obj_set_style_outline_width(paddle_top, 0, 0);
    }
  }
  if (paddle_bottom) {
    if (ult_active_bottom) {
      lv_obj_set_style_outline_width(paddle_bottom, 3, 0);
      lv_obj_set_style_outline_color(paddle_bottom, lv_color_hex(0x00FF00), 0);
      lv_obj_set_style_outline_opa(paddle_bottom, LV_OPA_COVER, 0);
      lv_obj_set_style_outline_pad(paddle_bottom, 2, 0);
    } else {
      lv_obj_set_style_outline_width(paddle_bottom, 0, 0);
    }
  }

  // If the game isn't actively playing (e.g., in countdown), don't move the ball or process collisions
  if (!playing && ball_pause_until_ms == 0) {
    // Ensure ball is hidden while waiting
    if (ball) lv_obj_add_flag(ball, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  // Update ball position (unless it's paused for an explosion)
  uint32_t now_ms = millis();
  if (now_ms < ball_pause_until_ms) {
    // ball is paused; do not update its movement until pause ends
  } else {
    // If we were paused and just ended, apply the target velocity
    if (ball_pause_until_ms != 0 && now_ms >= ball_pause_until_ms) {
      ball_pause_until_ms = 0;
      if (ball_vx_target != 0.0f || ball_vy_target != 0.0f) {
        ball_vx = ball_vx_target;
        ball_vy = ball_vy_target;
        ball_vx_target = 0.0f;
        ball_vy_target = 0.0f;
      }
    }
    ball_x += ball_vx;
    ball_y += ball_vy;
  }

  // bounce off left & right walls
  if (ball_x <= 0) { ball_x = 0; ball_vx = -ball_vx; }
  if (ball_x + BALL_SIZE >= TFT_HOR_RES) { ball_x = TFT_HOR_RES - BALL_SIZE; ball_vx = -ball_vx; }

  // check paddle collisions
  // top paddle collision
  if (ball_y <= 8 + PADDLE_H && ball_y >= 8 - BALL_SIZE) {
    if (ball_x + BALL_SIZE >= paddle_top_x && ball_x <= (paddle_top_x + PADDLE_W)) {
      ball_y = 8 + PADDLE_H; // make sure not to overlap
      ball_vy = fabsf(ball_vy); // move down
      // add some horizontal spin based on where it hit
      float hitpos = (ball_x + BALL_SIZE / 2.0f) - (paddle_top_x + PADDLE_W / 2.0f);
      ball_vx += hitpos * 0.04f;
      // if player had activated ultimate, trigger the explosion
      if (ult_active_top) {
        ult_active_top = false;
        // Set the ball as ultra (it remains ultra for 2 hits)
        ball_ultra = true;
        ball_ultra_hits_remaining = 2;
        // Pause and create an explosion effect; after pause, the ball will be set to a fast random direction
        ball_pause_until_ms = millis() + 1000;
        // compute target velocity towards the opponent (downwards)
        float speed = sqrtf(ball_vx*ball_vx + ball_vy*ball_vy);
        float angle = (random(20, 160) * 3.14159f) / 180.0f; // random angle roughly downward
        float multiplier = 1.9f;
        ball_vx_target = cosf(angle) * speed * multiplier;
        ball_vy_target = fabsf(sinf(angle) * speed * multiplier);
        // Create a small effect at paddle center
        // We'll create an explosion animation object
        // (function create_explosion will create anim using LVGL timers)
        // center coordinates for explosion
        int cx = (int)(ball_x + BALL_SIZE/2);
        int cy = (int)(ball_y + BALL_SIZE/2);
        // create explosion effect
        create_explosion(cx, cy);
        if (ball) lv_obj_set_style_bg_color(ball, lv_color_hex(0x00FF00), 0);
        if (ball) lv_obj_set_style_bg_color(ball, lv_color_hex(0x00FF00), 0);
      }
      // If ball is in ultra mode and we are not currently paused (so this is a real hit), count down its remaining hits
      if (ball_ultra && ball_pause_until_ms == 0) {
        ball_ultra_hits_remaining--;
        if (ball_ultra_hits_remaining <= 0) {
          ball_ultra = false;
          // tone down the speed a bit after ultra
          ball_vx /= 1.6f;
          ball_vy /= 1.6f;
          if (ball) lv_obj_set_style_bg_color(ball, lv_color_hex(0xFFFFFF), 0);
        }
      }
    }
  }
  // bottom paddle collision
  if (ball_y + BALL_SIZE >= TFT_VER_RES - PADDLE_H - 8 && ball_y + BALL_SIZE <= TFT_VER_RES - 8 + BALL_SIZE) {
    if (ball_x + BALL_SIZE >= paddle_bottom_x && ball_x <= (paddle_bottom_x + PADDLE_W)) {
      ball_y = TFT_VER_RES - PADDLE_H - 8 - BALL_SIZE;
      ball_vy = -fabsf(ball_vy);
      float hitpos = (ball_x + BALL_SIZE / 2.0f) - (paddle_bottom_x + PADDLE_W / 2.0f);
      ball_vx += hitpos * 0.04f;
      // if player had activated ultimate, trigger the explosion
      if (ult_active_bottom) {
        ult_active_bottom = false;
        ball_ultra = true;
        ball_ultra_hits_remaining = 2;
        ball_pause_until_ms = millis() + 1000;
        float speed = sqrtf(ball_vx*ball_vx + ball_vy*ball_vy);
        float angle = (random(200, 340) * 3.14159f) / 180.0f; // random angle roughly upwards
        float multiplier = 1.9f;
        ball_vx_target = cosf(angle) * speed * multiplier;
        ball_vy_target = -fabsf(sinf(angle) * speed * multiplier);
        int cx = (int)(ball_x + BALL_SIZE/2);
        int cy = (int)(ball_y + BALL_SIZE/2);
        create_explosion(cx, cy);
      }
      if (ball_ultra && ball_pause_until_ms == 0) {
        ball_ultra_hits_remaining--;
        if (ball_ultra_hits_remaining <= 0) {
          ball_ultra = false;
          ball_vx /= 1.6f;
          ball_vy /= 1.6f;
          if (ball) lv_obj_set_style_bg_color(ball, lv_color_hex(0xFFFFFF), 0);
        }
      }
    }
  }

  // check scoring (also resets ultra state)
  if (ball_y < -BALL_SIZE) {
    // ball passed top -> player 2 scores
    score_bottom++;
    if(p2_score_label) lv_label_set_text_fmt(p2_score_label, "%d", score_bottom);
      if (score_bottom >= WIN_SCORE) {
        game_over = true;
        playing = false;
        if (ball) lv_obj_add_flag(ball, LV_OBJ_FLAG_HIDDEN);
        if (win_label_top) lv_label_set_text(win_label_top, "Player 2");
        if (win_label_bottom) lv_label_set_text(win_label_bottom, "wins!");
        lv_obj_align(win_label_top, LV_ALIGN_CENTER, 0, -12);
        lv_obj_align(win_label_bottom, LV_ALIGN_CENTER, 0, 12);
      }
    // Reset ultra state on scoring
    if (ball_ultra) {
      ball_ultra = false;
      ball_ultra_hits_remaining = 0;
      if (ball) lv_obj_set_style_bg_color(ball, lv_color_hex(0xFFFFFF), 0);
    }
    reset_ball();
    return;
  }
  if (ball_y > TFT_VER_RES + BALL_SIZE) {
    // ball passed bottom -> player 1 scores
    score_top++;
    if(p1_score_label) lv_label_set_text_fmt(p1_score_label, "%d", score_top);
      if (score_top >= WIN_SCORE) {
        game_over = true;
        playing = false;
        if (ball) lv_obj_add_flag(ball, LV_OBJ_FLAG_HIDDEN);
        if (win_label_top) lv_label_set_text(win_label_top, "Player 1");
        if (win_label_bottom) lv_label_set_text(win_label_bottom, "wins!");
        lv_obj_align(win_label_top, LV_ALIGN_CENTER, 0, -12);
        lv_obj_align(win_label_bottom, LV_ALIGN_CENTER, 0, 12);
      }
    // Reset ultra state on scoring
    if (ball_ultra) {
      ball_ultra = false;
      ball_ultra_hits_remaining = 0;
      if (ball) lv_obj_set_style_bg_color(ball, lv_color_hex(0xFFFFFF), 0);
    }
    reset_ball();
    return;
  }

  // apply some cap on ball speed to keep gameplay playable
  if (ball_vx > 3.5f) ball_vx = 3.5f;
  if (ball_vx < -3.5f) ball_vx = -3.5f;
  if (ball_vy > 4.5f) ball_vy = 4.5f;
  if (ball_vy < -4.5f) ball_vy = -4.5f;

  // Avoid setting object beyond the screen to prevent LVGL scrollbars – clamp strictly
  int draw_x = (int)ball_x;
  int draw_y = (int)ball_y;
  if (draw_x < 0) draw_x = 0;
  if (draw_x + BALL_SIZE > TFT_HOR_RES) draw_x = TFT_HOR_RES - BALL_SIZE;
  if (draw_y < 0) draw_y = 0;
  if (draw_y + BALL_SIZE > TFT_VER_RES) draw_y = TFT_VER_RES - BALL_SIZE;
  if (ball) lv_obj_set_pos(ball, draw_x, draw_y);
}

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

    // --- Pong game UI ---
    pinMode(BTN_A, INPUT_PULLUP);
    pinMode(BTN_B, INPUT_PULLUP);
    pinMode(BTN_X, INPUT_PULLUP);
    pinMode(BTN_Y, INPUT_PULLUP);

    // init game state
    randomSeed(micros());
    paddle_top_x = (TFT_HOR_RES - PADDLE_W) / 2;
    paddle_bottom_x = (TFT_HOR_RES - PADDLE_W) / 2;
    reset_ball();
    score_top = 0;
    score_bottom = 0;
    game_over = false;

    // Set screen background to black
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    // Disable scrollbars to prevent them appearing when paddles have outlines
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    // Create top paddle
    paddle_top = lv_obj_create(lv_screen_active());
    lv_obj_set_size(paddle_top, PADDLE_W, PADDLE_H);
    lv_obj_set_style_bg_color(paddle_top, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(paddle_top, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(paddle_top, 0, 0);
    lv_obj_set_style_shadow_width(paddle_top, 0, 0);
    lv_obj_set_style_bg_grad_dir(paddle_top, LV_GRAD_DIR_NONE, 0);
    lv_obj_set_style_radius(paddle_top, 0, 0);
    lv_obj_set_style_outline_width(paddle_top, 0, 0);
    lv_obj_set_scrollbar_mode(paddle_top, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_pos(paddle_top, paddle_top_x, 8);

    // Create bottom paddle
    paddle_bottom = lv_obj_create(lv_screen_active());
    lv_obj_set_size(paddle_bottom, PADDLE_W, PADDLE_H);
    lv_obj_set_style_bg_color(paddle_bottom, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(paddle_bottom, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(paddle_bottom, 0, 0);
    lv_obj_set_style_shadow_width(paddle_bottom, 0, 0);
    lv_obj_set_style_bg_grad_dir(paddle_bottom, LV_GRAD_DIR_NONE, 0);
    lv_obj_set_style_radius(paddle_bottom, 0, 0);
    lv_obj_set_style_outline_width(paddle_bottom, 0, 0);
    lv_obj_set_scrollbar_mode(paddle_bottom, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_pos(paddle_bottom, paddle_bottom_x, TFT_VER_RES - PADDLE_H - 8);

    // Create ball
    ball = lv_obj_create(lv_screen_active());
    lv_obj_set_size(ball, BALL_SIZE, BALL_SIZE);
    lv_obj_set_style_bg_color(ball, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(ball, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ball, 0, 0);
    lv_obj_set_style_radius(ball, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_pos(ball, (int)ball_x, (int)ball_y);
    // hide ball initially until the countdown finishes
    lv_obj_add_flag(ball, LV_OBJ_FLAG_HIDDEN);

    // Score labels: small 'P1' and big number left-center, 'P2' and big number right-center
    p1_small_label = lv_label_create(lv_screen_active());
    lv_label_set_text(p1_small_label, "P1");
    lv_obj_set_style_text_color(p1_small_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(p1_small_label, &lv_font_montserrat_14, 0);
    lv_obj_align(p1_small_label, LV_ALIGN_LEFT_MID, 8, -20);

    p1_score_label = lv_label_create(lv_screen_active());
    lv_label_set_text_fmt(p1_score_label, "%d", score_top);
    lv_obj_set_style_text_color(p1_score_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(p1_score_label, &lv_font_montserrat_28, 0);
    lv_obj_align(p1_score_label, LV_ALIGN_LEFT_MID, 8, 10);

    p2_small_label = lv_label_create(lv_screen_active());
    lv_label_set_text(p2_small_label, "P2");
    lv_obj_set_style_text_color(p2_small_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(p2_small_label, &lv_font_montserrat_14, 0);
    lv_obj_align(p2_small_label, LV_ALIGN_RIGHT_MID, -8, -20);

    p2_score_label = lv_label_create(lv_screen_active());
    lv_label_set_text_fmt(p2_score_label, "%d", score_bottom);
    lv_obj_set_style_text_color(p2_score_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(p2_score_label, &lv_font_montserrat_28, 0);
    lv_obj_align(p2_score_label, LV_ALIGN_RIGHT_MID, -8, 10);

    // Win labels (two-line) - hidden initially
    win_label_top = lv_label_create(lv_screen_active());
    lv_label_set_text(win_label_top, "");
    lv_obj_align(win_label_top, LV_ALIGN_CENTER, 0, -12);
    lv_obj_set_style_text_color(win_label_top, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(win_label_top, &lv_font_montserrat_28, 0);

    win_label_bottom = lv_label_create(lv_screen_active());
    lv_label_set_text(win_label_bottom, "");
    lv_obj_align(win_label_bottom, LV_ALIGN_CENTER, 0, 12);
    lv_obj_set_style_text_color(win_label_bottom, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(win_label_bottom, &lv_font_montserrat_14, 0);
    // Ensure multiline text is centered
    lv_obj_set_style_text_align(win_label_top, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(win_label_bottom, LV_TEXT_ALIGN_CENTER, 0);

    // Create a timer to update the game
    lv_timer_create(game_update, 20, NULL);

    // Create vertical 3-segment ultimate charge bars with container for stroke
    // Top player (P1) - container with green outline for ready state (above score)
    ult_container_top = lv_obj_create(lv_screen_active());
    lv_obj_set_size(ult_container_top, 6, 28);
    lv_obj_set_style_bg_opa(ult_container_top, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ult_container_top, 0, 0);
    lv_obj_set_style_outline_width(ult_container_top, 0, 0);
    lv_obj_set_style_outline_color(ult_container_top, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_outline_pad(ult_container_top, 2, 0);
    lv_obj_set_style_radius(ult_container_top, 0, 0);
    lv_obj_set_style_pad_all(ult_container_top, 0, 0);
    lv_obj_set_pos(ult_container_top, 12, 35);
    
    for (int i = 0; i < 3; i++) {
        ult_seg_top[i] = lv_obj_create(lv_screen_active());
        lv_obj_set_size(ult_seg_top[i], 6, 8);
        lv_obj_set_style_bg_color(ult_seg_top[i], lv_color_hex(0x444444), 0); // grey by default
        lv_obj_set_style_border_width(ult_seg_top[i], 0, 0);
        lv_obj_set_style_radius(ult_seg_top[i], 1, 0);
        lv_obj_set_pos(ult_seg_top[i], 12, 35 + i * 10);
    }

    // Bottom player (P2) - container below score
    ult_container_bottom = lv_obj_create(lv_screen_active());
    lv_obj_set_size(ult_container_bottom, 6, 28);
    lv_obj_set_style_bg_opa(ult_container_bottom, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ult_container_bottom, 0, 0);
    lv_obj_set_style_outline_width(ult_container_bottom, 0, 0);
    lv_obj_set_style_outline_color(ult_container_bottom, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_outline_pad(ult_container_bottom, 2, 0);
    lv_obj_set_style_radius(ult_container_bottom, 0, 0);
    lv_obj_set_style_pad_all(ult_container_bottom, 0, 0);
    lv_obj_set_pos(ult_container_bottom, TFT_HOR_RES - 18, 175);
    
    for (int i = 0; i < 3; i++) {
        ult_seg_bottom[i] = lv_obj_create(lv_screen_active());
        lv_obj_set_size(ult_seg_bottom[i], 6, 8);
        lv_obj_set_style_bg_color(ult_seg_bottom[i], lv_color_hex(0x444444), 0); // grey by default
        lv_obj_set_style_border_width(ult_seg_bottom[i], 0, 0);
        lv_obj_set_style_radius(ult_seg_bottom[i], 1, 0);
        lv_obj_set_pos(ult_seg_bottom[i], TFT_HOR_RES - 18, 175 + i * 10);
    }

    // Start countdown initially
    start_countdown();

  Serial.println("LVGL initialized successfully!");
}

void loop()
{
  // Handle LVGL tasks
  lv_timer_handler();
  delay(5); // Small delay to let the system breathe
}

static void countdown_tick(lv_timer_t * t) {
  countdown_value--;
  if (countdown_value <= 0) {
    // hide countdown and start playing
    if (countdown_label) lv_label_set_text(countdown_label, "");
    playing = true;
    // delete timer
    if (countdown_timer) { lv_timer_del(countdown_timer); countdown_timer = NULL; }
    reset_ball();
    // make sure the ball is visible now that the game is starting
    if (ball) lv_obj_clear_flag(ball, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  if (countdown_label) lv_label_set_text_fmt(countdown_label, "%d", countdown_value);
}

// Explosion animation tick handler
static void explosion_tick(lv_timer_t * t) {
  explosion_ctx_t * ctx = (explosion_ctx_t *)lv_timer_get_user_data(t);
  if (!ctx) { lv_timer_del(t); return; }
  ctx->frame++;
  float progress = (float)ctx->frame / (float)ctx->max_frames;
  if (progress > 1.0f) progress = 1.0f;
  int size = (int)(ctx->max_size * progress);
  lv_obj_set_size(ctx->obj, size, size);
  lv_obj_set_pos(ctx->obj, ctx->cx - size/2, ctx->cy - size/2);
  // fade out over frames
  uint8_t opa = (uint8_t)(LV_OPA_COVER * (1.0f - progress));
  lv_obj_set_style_bg_opa(ctx->obj, opa, 0);
  if (ctx->frame >= ctx->max_frames) {
    lv_timer_del(t);
    lv_obj_del(ctx->obj);
    free(ctx);
  }
}

static void create_explosion(int cx, int cy) {
  explosion_ctx_t * ctx = (explosion_ctx_t *)malloc(sizeof(explosion_ctx_t));
  if (!ctx) return;
  ctx->frame = 0;
  ctx->max_frames = 12;
  ctx->max_size = 80;
  ctx->cx = cx;
  ctx->cy = cy;
  ctx->obj = lv_obj_create(lv_screen_active());
  lv_obj_set_size(ctx->obj, 0, 0);
  lv_obj_set_style_bg_color(ctx->obj, lv_color_hex(0x00FF00), 0);
  lv_obj_set_style_radius(ctx->obj, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(ctx->obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(ctx->obj, 0, 0);
  lv_obj_set_style_shadow_width(ctx->obj, 0, 0);
  lv_obj_set_pos(ctx->obj, cx, cy);
  lv_timer_create(explosion_tick, 30, ctx);
}

static void start_countdown(void) {
  playing = false;
  countdown_value = 3;
  if (!countdown_label) {
    countdown_label = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_color(countdown_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(countdown_label, &lv_font_montserrat_28, 0);
    lv_obj_align(countdown_label, LV_ALIGN_CENTER, 0, 0);
  }
  // Clear win labels when starting the countdown so we don't confuse the user
  if (win_label_top) lv_label_set_text(win_label_top, "");
  if (win_label_bottom) lv_label_set_text(win_label_bottom, "");
  lv_label_set_text_fmt(countdown_label, "%d", countdown_value);
  // ensure the ball is hidden while we count down
  if (ball) lv_obj_add_flag(ball, LV_OBJ_FLAG_HIDDEN);
  if (countdown_timer) lv_timer_del(countdown_timer);
  countdown_timer = lv_timer_create(countdown_tick, 1000, NULL);
}

// (No further forward declarations needed below)