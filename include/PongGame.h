#pragma once
#include <lvgl.h>
#include <Arduino.h>

class PongGame {
public:
    void init();
    void update();

private:
    static void countdownCallback(lv_timer_t * t);
    static void explosionCallback(lv_timer_t * t);

    void countdownTick();

    void resetBall();
    void startCountdown();
    void createExplosion(int cx, int cy);
    void handleInput();
    void moveBall();
    void checkCollisions();
    void checkScoring();
    void updateUltimateBars();
    void updateUltimateLogic();

    // UI Objects
    lv_obj_t *paddle_top = nullptr;
    lv_obj_t *paddle_bottom = nullptr;
    lv_obj_t *ball = nullptr;
    lv_obj_t *p1_score_label = nullptr;
    lv_obj_t *p2_score_label = nullptr;
    lv_obj_t *win_label_top = nullptr;
    lv_obj_t *win_label_bottom = nullptr;
    lv_obj_t *countdown_label = nullptr;
    
    // Ultimate UI
    lv_obj_t *ult_container_top = nullptr;
    lv_obj_t *ult_seg_top[3] = {nullptr};
    lv_obj_t *ult_container_bottom = nullptr;
    lv_obj_t *ult_seg_bottom[3] = {nullptr};

    // State
    lv_timer_t *countdown_timer = nullptr;
    uint32_t last_update_time = 0;
    
    bool playing = false;
    bool game_over = false;
    int countdown_value = 0;
    
    int paddle_top_x = 0;
    int paddle_bottom_x = 0;
    float ball_x = 0;
    float ball_y = 0;
    float ball_vx = 0;
    float ball_vy = 0;
    int score_top = 0;
    int score_bottom = 0;

    // Ultimate State
    int ult_count_top = 0;
    int ult_count_bottom = 0;
    uint32_t ult_next_charge_top = 0;
    uint32_t ult_next_charge_bottom = 0;
    bool ult_active_top = false;
    bool ult_active_bottom = false;
    uint32_t ult_hold_top_ms = 0;
    uint32_t ult_hold_bottom_ms = 0;

    // Ball Ultra State
    bool ball_ultra = false;
    int ball_ultra_hits_remaining = 0;
    uint32_t ball_pause_until_ms = 0;
    float ball_vx_target = 0;
    float ball_vy_target = 0;
};
