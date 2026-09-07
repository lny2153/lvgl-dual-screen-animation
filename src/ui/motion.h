#ifndef CALORIE_SCALE_UI_MOTION_H
#define CALORIE_SCALE_UI_MOTION_H

#include <stdint.h>

float ui_motion_ease(float progress);
float ui_motion_segment(uint32_t elapsed_ms, uint32_t start_ms, uint32_t end_ms);
int32_t ui_motion_lerp_i32(int32_t from, int32_t to, float progress);
uint8_t ui_motion_lerp_opa(uint8_t from, uint8_t to, float progress);

#endif
