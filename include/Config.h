#pragma once
#include <Arduino.h>

// --- Pins ---
namespace Pins {
    // Pico Enviro+ Pack LCD pinout (PIM635)
    constexpr int8_t LCD_CS   = 17;
    constexpr int8_t LCD_DC   = 16;
    constexpr int8_t LCD_SCK  = 18;
    constexpr int8_t LCD_MOSI = 19;
    constexpr int8_t LCD_MISO = -1;
    constexpr int8_t LCD_RST  = -1;
    constexpr int8_t LCD_BL   = 20;

    // Buttons
    constexpr int BTN_A = 12;
    constexpr int BTN_B = 13;
    constexpr int BTN_X = 14;
    constexpr int BTN_Y = 15;
}

// --- Display ---
constexpr int TFT_HOR_RES = 240;
constexpr int TFT_VER_RES = 240;

// --- Game Constants ---
namespace GameConsts {
    constexpr int WIN_SCORE = 5;
    constexpr int PADDLE_W = 60;
    constexpr int PADDLE_H = 10;
    constexpr int BALL_SIZE = 8;
    constexpr int POWER_ACTIVATE_DELAY = 200;
    constexpr int PADDLE_SPEED = 4;
}
