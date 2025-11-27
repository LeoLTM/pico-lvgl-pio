#include "PongGame.h"
#include "Config.h"

struct explosion_ctx_t {
    lv_obj_t * obj;
    int frame;
    int max_frames;
    int max_size;
    int cx;
    int cy;
};

void PongGame::init() {
    pinMode(Pins::BTN_A, INPUT_PULLUP);
    pinMode(Pins::BTN_B, INPUT_PULLUP);
    pinMode(Pins::BTN_X, INPUT_PULLUP);
    pinMode(Pins::BTN_Y, INPUT_PULLUP);

    randomSeed(micros());
    paddle_top_x = (TFT_HOR_RES - GameConsts::PADDLE_W) / 2;
    paddle_bottom_x = (TFT_HOR_RES - GameConsts::PADDLE_W) / 2;
    
    // Set screen background to black
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    // Create top paddle
    paddle_top = lv_obj_create(scr);
    lv_obj_set_size(paddle_top, GameConsts::PADDLE_W, GameConsts::PADDLE_H);
    lv_obj_set_style_bg_color(paddle_top, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(paddle_top, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(paddle_top, 0, 0);
    lv_obj_set_style_radius(paddle_top, 0, 0);
    lv_obj_set_scrollbar_mode(paddle_top, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_pos(paddle_top, paddle_top_x, 8);

    // Create bottom paddle
    paddle_bottom = lv_obj_create(scr);
    lv_obj_set_size(paddle_bottom, GameConsts::PADDLE_W, GameConsts::PADDLE_H);
    lv_obj_set_style_bg_color(paddle_bottom, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(paddle_bottom, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(paddle_bottom, 0, 0);
    lv_obj_set_style_radius(paddle_bottom, 0, 0);
    lv_obj_set_scrollbar_mode(paddle_bottom, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_pos(paddle_bottom, paddle_bottom_x, TFT_VER_RES - GameConsts::PADDLE_H - 8);

    // Create ball
    ball = lv_obj_create(scr);
    lv_obj_set_size(ball, GameConsts::BALL_SIZE, GameConsts::BALL_SIZE);
    lv_obj_set_style_bg_color(ball, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(ball, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ball, 0, 0);
    lv_obj_set_style_radius(ball, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_flag(ball, LV_OBJ_FLAG_HIDDEN);

    // Score labels
    lv_obj_t* p1_small = lv_label_create(scr);
    lv_label_set_text(p1_small, "P1");
    lv_obj_set_style_text_color(p1_small, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(p1_small, &lv_font_montserrat_14, 0);
    lv_obj_align(p1_small, LV_ALIGN_LEFT_MID, 8, -20);

    p1_score_label = lv_label_create(scr);
    lv_label_set_text_fmt(p1_score_label, "%d", score_top);
    lv_obj_set_style_text_color(p1_score_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(p1_score_label, &lv_font_montserrat_28, 0);
    lv_obj_align(p1_score_label, LV_ALIGN_LEFT_MID, 8, 10);

    lv_obj_t* p2_small = lv_label_create(scr);
    lv_label_set_text(p2_small, "P2");
    lv_obj_set_style_text_color(p2_small, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(p2_small, &lv_font_montserrat_14, 0);
    lv_obj_align(p2_small, LV_ALIGN_RIGHT_MID, -8, -20);

    p2_score_label = lv_label_create(scr);
    lv_label_set_text_fmt(p2_score_label, "%d", score_bottom);
    lv_obj_set_style_text_color(p2_score_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(p2_score_label, &lv_font_montserrat_28, 0);
    lv_obj_align(p2_score_label, LV_ALIGN_RIGHT_MID, -8, 10);

    // Win labels
    win_label_top = lv_label_create(scr);
    lv_label_set_text(win_label_top, "");
    lv_obj_align(win_label_top, LV_ALIGN_CENTER, 0, -12);
    lv_obj_set_style_text_color(win_label_top, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(win_label_top, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_align(win_label_top, LV_TEXT_ALIGN_CENTER, 0);

    win_label_bottom = lv_label_create(scr);
    lv_label_set_text(win_label_bottom, "");
    lv_obj_align(win_label_bottom, LV_ALIGN_CENTER, 0, 12);
    lv_obj_set_style_text_color(win_label_bottom, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(win_label_bottom, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(win_label_bottom, LV_TEXT_ALIGN_CENTER, 0);

    // Ultimate bars
    ult_container_top = lv_obj_create(scr);
    lv_obj_set_size(ult_container_top, 6, 28);
    lv_obj_set_style_bg_opa(ult_container_top, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ult_container_top, 0, 0);
    lv_obj_set_style_outline_color(ult_container_top, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_outline_pad(ult_container_top, 2, 0);
    lv_obj_set_pos(ult_container_top, 12, 35);
    
    for (int i = 0; i < 3; i++) {
        ult_seg_top[i] = lv_obj_create(scr);
        lv_obj_set_size(ult_seg_top[i], 6, 8);
        lv_obj_set_style_bg_color(ult_seg_top[i], lv_color_hex(0x444444), 0);
        lv_obj_set_style_border_width(ult_seg_top[i], 0, 0);
        lv_obj_set_style_radius(ult_seg_top[i], 1, 0);
        lv_obj_set_pos(ult_seg_top[i], 12, 35 + i * 10);
    }

    ult_container_bottom = lv_obj_create(scr);
    lv_obj_set_size(ult_container_bottom, 6, 28);
    lv_obj_set_style_bg_opa(ult_container_bottom, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ult_container_bottom, 0, 0);
    lv_obj_set_style_outline_color(ult_container_bottom, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_outline_pad(ult_container_bottom, 2, 0);
    lv_obj_set_pos(ult_container_bottom, TFT_HOR_RES - 18, 175);
    
    for (int i = 0; i < 3; i++) {
        ult_seg_bottom[i] = lv_obj_create(scr);
        lv_obj_set_size(ult_seg_bottom[i], 6, 8);
        lv_obj_set_style_bg_color(ult_seg_bottom[i], lv_color_hex(0x444444), 0);
        lv_obj_set_style_border_width(ult_seg_bottom[i], 0, 0);
        lv_obj_set_style_radius(ult_seg_bottom[i], 1, 0);
        lv_obj_set_pos(ult_seg_bottom[i], TFT_HOR_RES - 18, 175 + i * 10);
    }

    resetBall();
    startCountdown();
}

void PongGame::update() {
    if (millis() - last_update_time < 20) return;
    last_update_time = millis();

    handleInput();
    updateUltimateLogic();
    updateUltimateBars();

    if (game_over) return;

    if (!playing && ball_pause_until_ms == 0) {
        if (ball) lv_obj_add_flag(ball, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    moveBall();
    checkCollisions();
    checkScoring();
}

void PongGame::handleInput() {
    bool a_pressed = digitalRead(Pins::BTN_A) == LOW;
    bool b_pressed = digitalRead(Pins::BTN_B) == LOW;
    bool x_pressed = digitalRead(Pins::BTN_X) == LOW;
    bool y_pressed = digitalRead(Pins::BTN_Y) == LOW;

    if (game_over && (a_pressed || b_pressed || x_pressed || y_pressed)) {
        game_over = false;
        score_top = 0;
        score_bottom = 0;
        if(p1_score_label) lv_label_set_text_fmt(p1_score_label, "%d", score_top);
        if(p2_score_label) lv_label_set_text_fmt(p2_score_label, "%d", score_bottom);
        if(win_label_top) lv_label_set_text(win_label_top, "");
        if(win_label_bottom) lv_label_set_text(win_label_bottom, "");
        startCountdown();
        return;
    }

    if (game_over) return;

    if (a_pressed) paddle_top_x += GameConsts::PADDLE_SPEED;
    if (b_pressed) paddle_top_x -= GameConsts::PADDLE_SPEED;
    if (x_pressed) paddle_bottom_x += GameConsts::PADDLE_SPEED;
    if (y_pressed) paddle_bottom_x -= GameConsts::PADDLE_SPEED;

    // Clamp paddles
    if (paddle_top_x < 0) paddle_top_x = 0;
    if (paddle_top_x > TFT_HOR_RES - GameConsts::PADDLE_W) paddle_top_x = TFT_HOR_RES - GameConsts::PADDLE_W;
    if (paddle_bottom_x < 0) paddle_bottom_x = 0;
    if (paddle_bottom_x > TFT_HOR_RES - GameConsts::PADDLE_W) paddle_bottom_x = TFT_HOR_RES - GameConsts::PADDLE_W;

    if (paddle_top) lv_obj_set_pos(paddle_top, paddle_top_x, 8);
    if (paddle_bottom) lv_obj_set_pos(paddle_bottom, paddle_bottom_x, TFT_VER_RES - GameConsts::PADDLE_H - 8);

    // Ultimate activation
    const uint32_t tick_ms = 20;
    if (a_pressed && b_pressed) {
        ult_hold_top_ms += tick_ms;
        if (ult_hold_top_ms >= GameConsts::POWER_ACTIVATE_DELAY && ult_count_top >= 3 && !ult_active_top) {
            ult_active_top = true;
            ult_count_top = 0;
            ult_next_charge_top = millis() + random(2000, 6000);
        }
    } else {
        ult_hold_top_ms = 0;
    }

    if (x_pressed && y_pressed) {
        ult_hold_bottom_ms += tick_ms;
        if (ult_hold_bottom_ms >= GameConsts::POWER_ACTIVATE_DELAY && ult_count_bottom >= 3 && !ult_active_bottom) {
            ult_active_bottom = true;
            ult_count_bottom = 0;
            ult_next_charge_bottom = millis() + random(2000, 6000);
        }
    } else {
        ult_hold_bottom_ms = 0;
    }
}

void PongGame::updateUltimateLogic() {
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
}

void PongGame::updateUltimateBars() {
    for (int i = 0; i < 3; i++) {
        if (ult_seg_top[i]) {
            lv_obj_set_style_bg_color(ult_seg_top[i], (i < ult_count_top) ? lv_color_hex(0x00FF00) : lv_color_hex(0x444444), 0);
        }
        if (ult_seg_bottom[i]) {
            lv_obj_set_style_bg_color(ult_seg_bottom[i], (i < ult_count_bottom) ? lv_color_hex(0x00FF00) : lv_color_hex(0x444444), 0);
        }
    }

    if (ult_container_top) {
        lv_obj_set_style_outline_width(ult_container_top, (ult_count_top >= 3 && !ult_active_top) ? 2 : 0, 0);
    }
    if (ult_container_bottom) {
        lv_obj_set_style_outline_width(ult_container_bottom, (ult_count_bottom >= 3 && !ult_active_bottom) ? 2 : 0, 0);
    }

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
}

void PongGame::moveBall() {
    uint32_t now_ms = millis();
    if (now_ms < ball_pause_until_ms) {
        return;
    } else {
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

    // Bounce walls
    if (ball_x <= 0) { ball_x = 0; ball_vx = -ball_vx; }
    if (ball_x + GameConsts::BALL_SIZE >= TFT_HOR_RES) { ball_x = TFT_HOR_RES - GameConsts::BALL_SIZE; ball_vx = -ball_vx; }

    // Cap speed
    if (ball_vx > 3.5f) ball_vx = 3.5f;
    if (ball_vx < -3.5f) ball_vx = -3.5f;
    if (ball_vy > 4.5f) ball_vy = 4.5f;
    if (ball_vy < -4.5f) ball_vy = -4.5f;

    int draw_x = (int)ball_x;
    int draw_y = (int)ball_y;
    if (draw_x < 0) draw_x = 0;
    if (draw_x + GameConsts::BALL_SIZE > TFT_HOR_RES) draw_x = TFT_HOR_RES - GameConsts::BALL_SIZE;
    if (draw_y < 0) draw_y = 0;
    if (draw_y + GameConsts::BALL_SIZE > TFT_VER_RES) draw_y = TFT_VER_RES - GameConsts::BALL_SIZE;
    if (ball) lv_obj_set_pos(ball, draw_x, draw_y);
}

void PongGame::checkCollisions() {
    // Top paddle
    if (ball_y <= 8 + GameConsts::PADDLE_H && ball_y >= 8 - GameConsts::BALL_SIZE) {
        if (ball_x + GameConsts::BALL_SIZE >= paddle_top_x && ball_x <= (paddle_top_x + GameConsts::PADDLE_W)) {
            ball_y = 8 + GameConsts::PADDLE_H;
            ball_vy = fabsf(ball_vy);
            float hitpos = (ball_x + GameConsts::BALL_SIZE / 2.0f) - (paddle_top_x + GameConsts::PADDLE_W / 2.0f);
            ball_vx += hitpos * 0.04f;

            if (ult_active_top) {
                ult_active_top = false;
                ball_ultra = true;
                ball_ultra_hits_remaining = 2;
                ball_pause_until_ms = millis() + 1000;
                float speed = sqrtf(ball_vx*ball_vx + ball_vy*ball_vy);
                float angle = (random(20, 160) * 3.14159f) / 180.0f;
                float multiplier = 1.9f;
                ball_vx_target = cosf(angle) * speed * multiplier;
                ball_vy_target = fabsf(sinf(angle) * speed * multiplier);
                createExplosion((int)(ball_x + GameConsts::BALL_SIZE/2), (int)(ball_y + GameConsts::BALL_SIZE/2));
                if (ball) lv_obj_set_style_bg_color(ball, lv_color_hex(0x00FF00), 0);
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

    // Bottom paddle
    if (ball_y + GameConsts::BALL_SIZE >= TFT_VER_RES - GameConsts::PADDLE_H - 8 && ball_y + GameConsts::BALL_SIZE <= TFT_VER_RES - 8 + GameConsts::BALL_SIZE) {
        if (ball_x + GameConsts::BALL_SIZE >= paddle_bottom_x && ball_x <= (paddle_bottom_x + GameConsts::PADDLE_W)) {
            ball_y = TFT_VER_RES - GameConsts::PADDLE_H - 8 - GameConsts::BALL_SIZE;
            ball_vy = -fabsf(ball_vy);
            float hitpos = (ball_x + GameConsts::BALL_SIZE / 2.0f) - (paddle_bottom_x + GameConsts::PADDLE_W / 2.0f);
            ball_vx += hitpos * 0.04f;

            if (ult_active_bottom) {
                ult_active_bottom = false;
                ball_ultra = true;
                ball_ultra_hits_remaining = 2;
                ball_pause_until_ms = millis() + 1000;
                float speed = sqrtf(ball_vx*ball_vx + ball_vy*ball_vy);
                float angle = (random(200, 340) * 3.14159f) / 180.0f;
                float multiplier = 1.9f;
                ball_vx_target = cosf(angle) * speed * multiplier;
                ball_vy_target = -fabsf(sinf(angle) * speed * multiplier);
                createExplosion((int)(ball_x + GameConsts::BALL_SIZE/2), (int)(ball_y + GameConsts::BALL_SIZE/2));
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
}

void PongGame::checkScoring() {
    if (ball_y < -GameConsts::BALL_SIZE) {
        score_bottom++;
        if(p2_score_label) lv_label_set_text_fmt(p2_score_label, "%d", score_bottom);
        if (score_bottom >= GameConsts::WIN_SCORE) {
            game_over = true;
            playing = false;
            if (ball) lv_obj_add_flag(ball, LV_OBJ_FLAG_HIDDEN);
            if (win_label_top) lv_label_set_text(win_label_top, "Player 2");
            if (win_label_bottom) lv_label_set_text(win_label_bottom, "wins!");
        }
        if (ball_ultra) {
            ball_ultra = false;
            ball_ultra_hits_remaining = 0;
            if (ball) lv_obj_set_style_bg_color(ball, lv_color_hex(0xFFFFFF), 0);
        }
        resetBall();
    } else if (ball_y > TFT_VER_RES + GameConsts::BALL_SIZE) {
        score_top++;
        if(p1_score_label) lv_label_set_text_fmt(p1_score_label, "%d", score_top);
        if (score_top >= GameConsts::WIN_SCORE) {
            game_over = true;
            playing = false;
            if (ball) lv_obj_add_flag(ball, LV_OBJ_FLAG_HIDDEN);
            if (win_label_top) lv_label_set_text(win_label_top, "Player 1");
            if (win_label_bottom) lv_label_set_text(win_label_bottom, "wins!");
        }
        if (ball_ultra) {
            ball_ultra = false;
            ball_ultra_hits_remaining = 0;
            if (ball) lv_obj_set_style_bg_color(ball, lv_color_hex(0xFFFFFF), 0);
        }
        resetBall();
    }
}

void PongGame::resetBall() {
    ball_x = TFT_HOR_RES / 2.0f;
    ball_y = TFT_VER_RES / 2.0f;
    ball_vx = ((random(0, 2) == 0) ? 1.4f : -1.4f);
    ball_vy = (random(0, 2) == 0) ? 1.4f : -1.4f;
    if (ball) lv_obj_set_pos(ball, (int)ball_x, (int)ball_y);
    
    ult_count_top = 0;
    ult_count_bottom = 0;
    ult_active_top = false;
    ult_active_bottom = false;
    ball_ultra = false;
    ball_ultra_hits_remaining = 0;
    
    ult_next_charge_top = millis() + random(2000, 6000);
    ult_next_charge_bottom = millis() + random(2000, 6000);
    
    if (paddle_top) lv_obj_set_style_outline_width(paddle_top, 0, 0);
    if (paddle_bottom) lv_obj_set_style_outline_width(paddle_bottom, 0, 0);
}

void PongGame::startCountdown() {
    playing = false;
    countdown_value = 3;
    if (!countdown_label) {
        countdown_label = lv_label_create(lv_screen_active());
        lv_obj_set_style_text_color(countdown_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(countdown_label, &lv_font_montserrat_28, 0);
        lv_obj_align(countdown_label, LV_ALIGN_CENTER, 0, 0);
    }
    if (win_label_top) lv_label_set_text(win_label_top, "");
    if (win_label_bottom) lv_label_set_text(win_label_bottom, "");
    lv_label_set_text_fmt(countdown_label, "%d", countdown_value);
    
    if (ball) lv_obj_add_flag(ball, LV_OBJ_FLAG_HIDDEN);
    if (countdown_timer) lv_timer_del(countdown_timer);
    countdown_timer = lv_timer_create(countdownCallback, 1000, this);
}

void PongGame::countdownCallback(lv_timer_t * t) {
    PongGame* game = static_cast<PongGame*>(lv_timer_get_user_data(t));
    game->countdownTick();
}

void PongGame::countdownTick() {
    countdown_value--;
    if (countdown_value <= 0) {
        if (countdown_label) lv_label_set_text(countdown_label, "");
        playing = true;
        if (countdown_timer) { lv_timer_del(countdown_timer); countdown_timer = nullptr; }
        resetBall();
        if (ball) lv_obj_clear_flag(ball, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (countdown_label) lv_label_set_text_fmt(countdown_label, "%d", countdown_value);
}

void PongGame::createExplosion(int cx, int cy) {
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
    lv_obj_set_pos(ctx->obj, cx, cy);
    lv_timer_create(explosionCallback, 30, ctx);
}

void PongGame::explosionCallback(lv_timer_t * t) {
    explosion_ctx_t * ctx = (explosion_ctx_t *)lv_timer_get_user_data(t);
    if (!ctx) { lv_timer_del(t); return; }
    ctx->frame++;
    float progress = (float)ctx->frame / (float)ctx->max_frames;
    if (progress > 1.0f) progress = 1.0f;
    int size = (int)(ctx->max_size * progress);
    lv_obj_set_size(ctx->obj, size, size);
    lv_obj_set_pos(ctx->obj, ctx->cx - size/2, ctx->cy - size/2);
    uint8_t opa = (uint8_t)(LV_OPA_COVER * (1.0f - progress));
    lv_obj_set_style_bg_opa(ctx->obj, opa, 0);
    if (ctx->frame >= ctx->max_frames) {
        lv_timer_del(t);
        lv_obj_del(ctx->obj);
        free(ctx);
    }
}
