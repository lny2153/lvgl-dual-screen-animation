#ifndef CALORIE_SCALE_RIGHT_SCREEN_H
#define CALORIE_SCALE_RIGHT_SCREEN_H

#include <stdint.h>

#include "lvgl.h"
#include "ui_view_model.h"

typedef struct right_screen right_screen_t;
typedef right_screen_t ui_right_screen_t;

/*
 * Creates a fixed 240 x 240 TFT scene at (0, 0) inside parent.
 * The returned renderer owns every child object it creates.
 */
right_screen_t * right_screen_create(lv_obj_t * parent);

/*
 * Pure frame renderer. elapsed_ms is measured from the shared dual-screen t0.
 * Exactly one right-screen primary layer is made visible on every call.
 */
void right_screen_render(right_screen_t * screen,
                         const ui_view_model_t * before,
                         const ui_view_model_t * after,
                         ui_transition_t transition,
                         uint32_t elapsed_ms);

lv_obj_t * right_screen_object(right_screen_t * screen);
lv_obj_t * ui_right_screen_touch_target(ui_right_screen_t * screen);
void right_screen_destroy(right_screen_t * screen);

#endif
