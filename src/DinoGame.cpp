#include "DinoGame.h"
#include "Config.h"

// Constants
static const int LANE_HEIGHT = 60; // 240 / 4
static const int LANE_Y_START = 0;
static const int PLAYER_X = 30;
static const int PLAYER_SIZE = 12;
static const int GROUND_Y_OFFSET = 20; // Lift everything up more (was 10)
static const float GRAVITY = 0.15f; // Reduced for floatier jump
static const float JUMP_FORCE = 4.2f; // Adjusted for longer air time (~560ms)
static const float HOLD_GRAVITY_MULT = 0.5f;
static const float INITIAL_SPEED = 2.0f; // Reduced from 3.0
static const float MAX_SPEED = 6.0f; // Reduced from 8.0

void DinoGame::init() {
    screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

    // Create Lanes
    for (int i = 0; i < 4; i++) {
        lane_containers[i] = lv_obj_create(screen);
        lv_obj_set_size(lane_containers[i], TFT_HOR_RES, LANE_HEIGHT);
        lv_obj_set_pos(lane_containers[i], 0, LANE_Y_START + i * LANE_HEIGHT);
        lv_obj_set_style_border_width(lane_containers[i], 0, 0);
        lv_obj_set_style_radius(lane_containers[i], 0, 0);
        lv_obj_set_scrollbar_mode(lane_containers[i], LV_SCROLLBAR_MODE_OFF);
        
        // Alternating colors
        if (i % 2 == 0) {
            lv_obj_set_style_bg_color(lane_containers[i], lv_color_hex(0xD2691E), 0); // Chocolate
        } else {
            lv_obj_set_style_bg_color(lane_containers[i], lv_color_hex(0xA0522D), 0); // Sienna (Lighter than Saddle Brown)
        }
    }

    // Initialize Players
    players.clear();
    // P1 (Top) - BTN_B
    players.push_back({0, 0, Pins::BTN_B, lv_color_hex(0xFF0000), 0, 0, false, false, false, 0, nullptr, nullptr, nullptr});
    // P2 - BTN_A
    players.push_back({1, 1, Pins::BTN_A, lv_color_hex(0x0000FF), 0, 0, false, false, false, 0, nullptr, nullptr, nullptr});
    // P3 - BTN_Y
    players.push_back({2, 2, Pins::BTN_Y, lv_color_hex(0x00FF00), 0, 0, false, false, false, 0, nullptr, nullptr, nullptr});
    // P4 (Bottom) - BTN_X
    players.push_back({3, 3, Pins::BTN_X, lv_color_hex(0xFFFF00), 0, 0, false, false, false, 0, nullptr, nullptr, nullptr});

    for (auto& p : players) {
        pinMode(p.pin, INPUT_PULLUP);
        
        // Create Player UI
        p.obj = lv_obj_create(lane_containers[p.lane_index]);
        lv_obj_set_size(p.obj, PLAYER_SIZE, PLAYER_SIZE);
        lv_obj_set_style_bg_color(p.obj, p.color, 0);
        lv_obj_set_style_border_width(p.obj, 2, 0);
        lv_obj_set_style_border_color(p.obj, lv_color_hex(0x000000), 0);
        lv_obj_set_style_radius(p.obj, 4, 0);
        lv_obj_set_scrollbar_mode(p.obj, LV_SCROLLBAR_MODE_OFF);
    }

    // Score Label
    score_label = lv_label_create(screen);
    lv_label_set_text(score_label, "0000");
    lv_obj_set_style_text_font(score_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(score_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(score_label, LV_ALIGN_TOP_RIGHT, -10, 0);

    // Game Over Label
    game_over_label = lv_label_create(screen);
    lv_label_set_text(game_over_label, "GAME OVER");
    lv_obj_set_style_text_font(game_over_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(game_over_label, lv_color_hex(0xFF0000), 0);
    lv_obj_align(game_over_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(game_over_label, LV_OBJ_FLAG_HIDDEN);

    resetGame();
}

void DinoGame::resetGame() {
    game_speed = INITIAL_SPEED;
    score = 0;
    playing = true;
    game_over = false;
    players_alive = 4;
    last_winner_id = -1;
    
    lv_obj_add_flag(game_over_label, LV_OBJ_FLAG_HIDDEN);

    // Reset lane colors
    for (int i = 0; i < 4; i++) {
        if (i % 2 == 0) {
            lv_obj_set_style_bg_color(lane_containers[i], lv_color_hex(0xD2691E), 0);
        } else {
            lv_obj_set_style_bg_color(lane_containers[i], lv_color_hex(0xA0522D), 0);
        }
    }

    for (auto& p : players) {
        p.y = 0;
        p.vy = 0;
        p.is_jumping = false;
        p.is_dead = false;
        lv_obj_set_style_bg_opa(p.obj, LV_OPA_COVER, 0);
    }

    // Clear obstacles
    for (auto& obs : obstacles) {
        for (int i = 0; i < 4; i++) {
            if (obs.obj[i]) lv_obj_del(obs.obj[i]);
        }
    }
    obstacles.clear();

    // Clear scenery
    for (auto& s : scenery_items) {
        if (s.obj) lv_obj_del(s.obj);
    }
    scenery_items.clear();

    // Clear and Init Track Details
    for (auto& td : track_details) {
        if (td.obj) lv_obj_del(td.obj);
    }
    track_details.clear();

    for (int i = 0; i < 40; i++) { // 40 individual rocks
        TrackDetail td;
        td.x = random(0, TFT_HOR_RES);
        td.width = random(4, 15);
        td.height = random(2, 6);
        td.lane = random(0, 4); // Random lane
        
        td.obj = lv_obj_create(lane_containers[td.lane]);
        lv_obj_set_size(td.obj, td.width, td.height);
        lv_obj_set_style_bg_color(td.obj, lv_color_hex(0x999999), 0); // Brighter Grey
        lv_obj_set_style_border_width(td.obj, 0, 0);
        lv_obj_set_style_radius(td.obj, 1, 0);
        lv_obj_set_scrollbar_mode(td.obj, LV_SCROLLBAR_MODE_OFF);
        // Distribute evenly across height
        int y_pos = random(0, LANE_HEIGHT - td.height);
        lv_obj_set_pos(td.obj, (int)td.x, y_pos);
        lv_obj_move_background(td.obj);
        
        track_details.push_back(td);
    }

    next_obstacle_time = millis() + 2000;
}

void DinoGame::update() {
    uint32_t now = millis();

    // Initialize timers if 0 (first run)
    if (last_logic_time == 0) last_logic_time = now;
    if (last_render_time == 0) last_render_time = now;

    // Logic Loop (100Hz = 10ms)
    int steps = 0;
    while (now - last_logic_time >= 10 && steps < 5) {
        if (!game_over) {
            handleInput();
            updatePhysics();
            updateObstacles();
            updateScenery();
            checkCollisions();
            
            // Increase speed and score
            game_speed += 0.0005f;
            if (game_speed > MAX_SPEED) game_speed = MAX_SPEED;
            
            score += game_speed * 0.05f;
            lv_label_set_text_fmt(score_label, "%05d", (int)score);
        } else {
             // Restart on any button press
            for (const auto& p : players) {
                if (digitalRead(p.pin) == LOW) {
                    resetGame();
                    return;
                }
            }
        }
        last_logic_time += 10;
        steps++;
    }
    if (now - last_logic_time > 100) last_logic_time = now;

    // Render Loop (Max 60Hz = 16ms)
    if (now - last_render_time >= 16) {
        render();
        last_render_time = now;
    }
}

void DinoGame::handleInput() {
    for (auto& p : players) {
        if (p.is_dead) continue;

        bool pressed = digitalRead(p.pin) == LOW;
        p.button_held = pressed;

        if (pressed && !p.is_jumping) {
            p.vy = JUMP_FORCE;
            p.is_jumping = true;
        }
    }
}

void DinoGame::updatePhysics() {
    for (auto& p : players) {
        if (p.is_dead) continue;

        // Apply gravity
        float gravity = GRAVITY;
        if (p.button_held && p.vy > 0) {
            gravity *= HOLD_GRAVITY_MULT; // Jump higher if held
        }
        
        p.vy -= gravity;
        p.y += p.vy;

        // Ground collision
        if (p.y <= 0) {
            p.y = 0;
            p.vy = 0;
            p.is_jumping = false;
        }
        
        // Ceiling collision
        if (p.y > LANE_HEIGHT - PLAYER_SIZE - 2) {
            p.y = LANE_HEIGHT - PLAYER_SIZE - 2;
            p.vy = 0;
        }
    }
}

void DinoGame::updateObstacles() {
    // Move existing obstacles
    for (auto it = obstacles.begin(); it != obstacles.end();) {
        it->x -= game_speed;
        if (it->x < -50) {
            for (int i = 0; i < 4; i++) {
                if (it->obj[i]) lv_obj_del(it->obj[i]);
            }
            it = obstacles.erase(it);
        } else {
            ++it;
        }
    }

    // Spawn new obstacles
    if (millis() > next_obstacle_time) {
        Obstacle obs;
        obs.x = TFT_HOR_RES + 20;
        obs.width = 4 + random(0, 4); // Narrower (4-8)
        obs.height = 15 + random(0, 10); // Taller
        obs.active = true;

        for (int i = 0; i < 4; i++) {
            obs.obj[i] = lv_obj_create(lane_containers[i]);
            lv_obj_set_size(obs.obj[i], obs.width, obs.height);
            lv_obj_set_style_bg_color(obs.obj[i], lv_color_hex(0x32CD32), 0); // Lime Green
            lv_obj_set_style_border_width(obs.obj[i], 0, 0);
            lv_obj_set_style_radius(obs.obj[i], 2, 0);
            lv_obj_set_scrollbar_mode(obs.obj[i], LV_SCROLLBAR_MODE_OFF);
        }
        
        obstacles.push_back(obs);
        // Drastically expand spacing
        next_obstacle_time = millis() + random(2000, 4000) / (game_speed / 3.0f);
    }
}

void DinoGame::updateScenery() {
    for (auto& td : track_details) {
        td.x -= game_speed;
        if (td.x < -20) {
            td.x = TFT_HOR_RES + random(0, 100);
            td.width = random(4, 15);
            td.height = random(2, 6);
            td.lane = random(0, 4); // New random lane
            
            // Move to new lane
            lv_obj_set_parent(td.obj, lane_containers[td.lane]);
            
            // Distribute evenly across height
            int y_pos = random(0, LANE_HEIGHT - td.height);
            
            lv_obj_set_size(td.obj, td.width, td.height);
            lv_obj_set_pos(td.obj, (int)td.x, y_pos);
        } else {
            lv_obj_set_x(td.obj, (int)td.x);
        }
    }
}

void DinoGame::checkCollisions() {
    int current_alive_count = 0;
    int potential_winner = -1;

    for (auto& p : players) {
        if (p.is_dead) continue;

        current_alive_count++;
        potential_winner = p.id;

        // 3-point collision check on feet
        // Left, Center, Right of the bottom of the player
        float feet_y = p.y; 
        float feet_x_left = PLAYER_X;
        float feet_x_center = PLAYER_X + PLAYER_SIZE / 2.0f;
        float feet_x_right = PLAYER_X + PLAYER_SIZE;

        for (const auto& obs : obstacles) {
            // Obstacle bounds
            float obs_l = obs.x;
            float obs_r = obs.x + obs.width;
            float obs_h = obs.height;

            // Check if feet are below obstacle height
            if (feet_y < obs_h) {
                // Check X overlap for any of the 3 points
                bool hit = false;
                if (feet_x_left > obs_l && feet_x_left < obs_r) hit = true;
                else if (feet_x_center > obs_l && feet_x_center < obs_r) hit = true;
                else if (feet_x_right > obs_l && feet_x_right < obs_r) hit = true;

                if (hit) {
                    p.is_dead = true;
                    lv_obj_set_style_bg_opa(p.obj, LV_OPA_50, 0);
                    // Darken lane (less dark than before)
                    lv_obj_set_style_bg_color(lane_containers[p.lane_index], lv_color_hex(0x663311), 0);
                    players_alive--;
                    current_alive_count--;
                }
            }
        }
    }

    if (current_alive_count > 0) {
        last_winner_id = potential_winner;
    }

    if (players_alive <= 0) {
        gameOver();
    }
}

void DinoGame::gameOver() {
    game_over = true;
    lv_obj_clear_flag(game_over_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(game_over_label);
    
    if (last_winner_id != -1) {
        lv_label_set_text_fmt(game_over_label, "WINNER: P%d", last_winner_id + 1);
        lv_obj_set_style_text_color(game_over_label, lv_color_hex(0x00FF00), 0);
    } else {
        lv_label_set_text(game_over_label, "GAME OVER");
        lv_obj_set_style_text_color(game_over_label, lv_color_hex(0xFF0000), 0);
    }
    lv_obj_align(game_over_label, LV_ALIGN_CENTER, 0, 0);
}

void DinoGame::render() {
    // Render Players
    for (auto& p : players) {
        // Position relative to lane bottom
        // Lane height is LANE_HEIGHT. Player y is 0 at bottom.
        // LVGL coords are top-left.
        // y_pos = LANE_HEIGHT - PLAYER_SIZE - p.y
        int draw_y = LANE_HEIGHT - PLAYER_SIZE - (int)p.y - GROUND_Y_OFFSET;
        lv_obj_set_pos(p.obj, PLAYER_X, draw_y);
    }

    // Render Obstacles
    for (auto& obs : obstacles) {
        for (int i = 0; i < 4; i++) {
            if (obs.obj[i]) {
                // Align to bottom of lane
                lv_obj_set_pos(obs.obj[i], (int)obs.x, LANE_HEIGHT - obs.height - GROUND_Y_OFFSET);
            }
        }
    }
}
