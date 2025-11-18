#include <Arduino.h>
#include <Arduino_GFX_Library.h>
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
    LCD_RST,     // RST (-1 because it’s tied to RUN)
    2,           // rotation (0,1,2,3) – 2 usually gives the “normal” Enviro+ orientation
    true         // IPS
);

void setup(void)
{
#ifdef DEV_DEVICE_INIT
  DEV_DEVICE_INIT();
#endif

  Serial.begin(115200);
  // Serial.setDebugOutput(true);
  // while(!Serial);
  Serial.println("Arduino_GFX Hello World example");

  // Init Display
  if (!gfx->begin())
  {
    Serial.println("gfx->begin() failed!");
  }
  gfx->fillScreen(RGB565_BLACK);

  // turn on backlight
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  gfx->fillScreen(RGB565_BLACK);
  gfx->setCursor(10, 10);
  gfx->setTextColor(RGB565_RED);
  gfx->println("Hello Enviro+!");

  delay(5000); // 5 seconds
}

void loop()
{
  gfx->setCursor(random(gfx->width()), random(gfx->height()));
  gfx->setTextColor(random(0xffff), random(0xffff));
  gfx->setTextSize(random(6) /* x scale */, random(6) /* y scale */, random(2) /* pixel_margin */);
  gfx->println("Hello World!");

  delay(1000); // 1 second
}