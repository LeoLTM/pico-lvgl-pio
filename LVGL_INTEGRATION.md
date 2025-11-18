# LVGL Integration with ArduinoGFX

This project has been successfully integrated with LVGL (Light and Versatile Graphics Library) using ArduinoGFX as middleware for the ST7789 display.

## Project Structure

```
pico-lvgl-pio/
├── include/
│   ├── lv_conf.h              # LVGL configuration file (enabled)
│   └── lv_conf_template.h     # Original LVGL template
├── src/
│   └── main.cpp               # Main application with LVGL integration
├── platformio.ini             # PlatformIO configuration
└── LVGL_INTEGRATION.md        # This file
```

## Integration Details

### 1. Dependencies Added

In `platformio.ini`:
- `lvgl/lvgl@^9.4.0` - LVGL library
- `moononournation/GFX Library for Arduino@^1.6.2` - ArduinoGFX (already present)

### 2. Configuration

The `lv_conf.h` file has been created from the LVGL template with the following key settings:
- **Color depth**: 16-bit (RGB565) - matches the ST7789 display
- **Default configuration**: Using LVGL defaults optimized for embedded systems
- **Memory**: Built-in memory management with 64KB pool

### 3. Code Integration

The main.cpp file implements:

#### LVGL Initialization
- **Tick callback**: Uses Arduino `millis()` for time tracking
- **Display creation**: 240x240 resolution matching the ST7789 panel
- **Buffer allocation**: Dual buffers (1/10 screen size each) for smooth rendering
- **Flush callback**: Bridges LVGL to ArduinoGFX using `draw16bitRGBBitmap()`

#### Display Integration via ArduinoGFX
- ArduinoGFX acts as middleware between LVGL and the ST7789 display
- The flush callback (`my_flush_cb`) transfers LVGL's rendered content to ArduinoGFX
- This approach provides hardware abstraction and optimal performance

#### Simple UI Demo
- Centered label with "Hello LVGL!\nwith ArduinoGFX" text
- Button widget with "Click Me!" label
- Demonstrates LVGL's widget system and layout capabilities

### 4. Display Hardware

**Pimoroni Enviro+ Pack (PIM635)**:
- Display: ST7789 240x240 IPS LCD
- SPI Interface on Raspberry Pi Pico
- GPIO Pins:
  - CS: GP17
  - DC: GP16
  - SCK: GP18
  - MOSI: GP19
  - Backlight: GP20

## How It Works

1. **Initialization** (`setup()`):
   - Initialize ArduinoGFX for hardware display control
   - Initialize LVGL library
   - Create display object with buffers
   - Register flush callback to connect LVGL → ArduinoGFX
   - Create UI widgets (label, button)

2. **Main Loop** (`loop()`):
   - Call `lv_timer_handler()` to process LVGL tasks
   - LVGL handles rendering, animations, and events internally
   - Rendered content is sent to display via flush callback

