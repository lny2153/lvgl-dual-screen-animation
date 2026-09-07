#ifndef CALORIE_SCALE_LEFT_SCREEN_H
#define CALORIE_SCALE_LEFT_SCREEN_H

#include "lvgl.h"
#include "ui_view_model.h"

typedef struct ui_left_screen ui_left_screen_t;

ui_left_screen_t *ui_left_screen_create(lv_obj_t *parent);
lv_obj_t *ui_left_screen_touch_target(ui_left_screen_t *screen);
void ui_left_screen_render(
    ui_left_screen_t *screen,
    const ui_view_model_t *before,
    const ui_view_model_t *after,
    ui_transition_t transition,
    uint32_t elapsed_ms);

#endif
