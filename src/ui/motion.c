#include "motion.h"

static float clamp01(float value)
{
    if(value < 0.0f) return 0.0f;
    if(value > 1.0f) return 1.0f;
    return value;
}

static float cubic(float t, float p1, float p2)
{
    const float u = 1.0f - t;
    return 3.0f * u * u * t * p1 + 3.0f * u * t * t * p2 + t * t * t;
}

static float cubic_derivative(float t, float p1, float p2)
{
    const float u = 1.0f - t;
    return 3.0f * u * u * p1 + 6.0f * u * t * (p2 - p1) + 3.0f * t * t * (1.0f - p2);
}

float ui_motion_ease(float progress)
{
    const float x = clamp01(progress);
    float t = x;

    /* CSS cubic-bezier(0.5, 0, 0, 1): solve x(t), then evaluate y(t). */
    for(int i = 0; i < 6; i++) {
        const float error = cubic(t, 0.5f, 0.0f) - x;
        const float slope = cubic_derivative(t, 0.5f, 0.0f);
        if(slope > -0.0001f && slope < 0.0001f) break;
        t = clamp01(t - error / slope);
    }
    return clamp01(cubic(t, 0.0f, 1.0f));
}

float ui_motion_segment(uint32_t elapsed_ms, uint32_t start_ms, uint32_t end_ms)
{
    if(elapsed_ms <= start_ms) return 0.0f;
    if(elapsed_ms >= end_ms || end_ms <= start_ms) return 1.0f;
    return ui_motion_ease((float)(elapsed_ms - start_ms) / (float)(end_ms - start_ms));
}

int32_t ui_motion_lerp_i32(int32_t from, int32_t to, float progress)
{
    return from + (int32_t)((float)(to - from) * clamp01(progress));
}

uint8_t ui_motion_lerp_opa(uint8_t from, uint8_t to, float progress)
{
    int32_t value = ui_motion_lerp_i32(from, to, progress);
    if(value < 0) value = 0;
    if(value > 255) value = 255;
    return (uint8_t)value;
}
