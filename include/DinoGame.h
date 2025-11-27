#pragma once
#include <lvgl.h>
#include <Arduino.h>
#include <vector>

class DinoGame {
public:
    void init();
    void update();

private:
    struct Player {
        int id;
        int lane_index;
        int pin;
        lv_color_t color;
        
        // Physics
        float y; // Height from ground (0 = on ground)
        float vy;
        bool is_jumping;
        bool is_dead;
        bool button_held;
        
        // Animation
        int anim_frame;
        
        // UI
        lv_obj_t* obj;
    };

    struct Obstacle {
        float x;
        int width;
        int height;
        lv_obj_t* obj[4]; // One visual per lane
        bool active;
    };

    struct Scenery {
        float x;
        float y;
        int type; // 0 = cactus, 1 = rock
        lv_obj_t* obj;
    };

    struct TrackDetail {
        float x;
        int width;
        int height;
        int lane;
        lv_obj_t* obj;
    };

    void handleInput();
    void updatePhysics();
    void updateObstacles();
    void updateScenery();
    void checkCollisions();
    void render();
    void resetGame();
    void gameOver();

    // Game State
    bool playing = false;
    bool game_over = false;
    int last_winner_id = -1;
    uint32_t last_logic_time = 0;
    uint32_t last_render_time = 0;
    float game_speed = 0;
    float score = 0;
    int players_alive = 0;

    // Entities
    std::vector<Player> players;
    std::vector<Obstacle> obstacles;
    std::vector<Scenery> scenery_items;
    std::vector<TrackDetail> track_details;

    // Timers
    uint32_t next_obstacle_time = 0;
    uint32_t next_scenery_time = 0;

    // UI Containers
    lv_obj_t* screen = nullptr;
    lv_obj_t* lane_containers[4] = {nullptr};
    lv_obj_t* game_over_label = nullptr;
    lv_obj_t* score_label = nullptr;
};
