#include <Arduino.h>
#include "Config.h"
#include "DisplayDriver.h"
#include "PongGame.h"
#include "DinoGame.h"

// PongGame game;
DinoGame game;

void setup() {
    Serial.begin(115200);
    // while(!Serial); 
    
    Display_Init();
    game.init();
    
    Serial.println("LVGL Dino initialized successfully!");
}

void loop() {
    Display_Loop();
    game.update();
}
