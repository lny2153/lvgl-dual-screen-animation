#include "right_screen.h"

#include <stddef.h>

#include "motion.h"
#include "tokens.h"

#define RIGHT_RING_X              27
#define RIGHT_RING_Y              27
#define RIGHT_RING_SIZE           186
#define RIGHT_RING_RADIUS         84
#define RIGHT_RING_WIDTH          17
#define RIGHT_RING_WIDTH_ACTION   25
#define RIGHT_CENTER              120
#define RIGHT_SCALE_ONE           256
#define RIGHT_SCALE_POINT         12
#define RIGHT_UPLOAD_DOT_COUNT    12
#define RIGHT_PULSE_COUNT         4
#define RIGHT_GRADIENT_SEGMENTS   36

struct right_screen {
    lv_obj_t * root;
    lv_obj_t * home_layer;
    lv_obj_t * action_layer;
    lv_obj_t * upload_layer;
    lv_obj_t * safety_layer;
    lv_obj_t * charge_layer;
    lv_obj_t * power_layer;
    lv_obj_t * active_layer;

    lv_obj_t * home_base;
    lv_obj_t * home_gradient[RIGHT_GRADIENT_SEGMENTS];
    lv_obj_t * home_start_cap;
    lv_obj_t * home_cap;
    lv_obj_t * home_plus_h;
    lv_obj_t * home_plus_v;

    lv_obj_t * action_base;
    lv_obj_t * action_progress;
    lv_obj_t * action_cap;
    lv_obj_t * action_plus_h;
    lv_obj_t * action_plus_v;
    lv_obj_t * action_check;
    lv_point_precise_t action_check_points[3];

    lv_obj_t * upload_dots[RIGHT_UPLOAD_DOT_COUNT];
    lv_obj_t * safety_pulses[RIGHT_PULSE_COUNT];

    lv_obj_t * charge_arc;
    lv_obj_t * charge_cap;
    lv_obj_t * charge_bolt;
    lv_point_precise_t charge_bolt_points[6];
};

static float clamp01(float value)
{
    if(value < 0.0f) return 0.0f;
    if(value > 1.0f) return 1.0f;
    return value;
}

static int32_t scale_from_progress(float progress)
{
    return ui_motion_lerp_i32(RIGHT_SCALE_POINT, RIGHT_SCALE_ONE, clamp01(progress));
}

static lv_obj_t * make_layer(lv_obj_t * parent)
{
    lv_obj_t * layer = lv_obj_create(parent);
    lv_obj_remove_style_all(layer);
    lv_obj_set_pos(layer, 0, 0);
    lv_obj_set_size(layer, UI_RIGHT_W, UI_RIGHT_H);
    lv_obj_clear_flag(layer, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(layer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_transform_pivot_x(layer, RIGHT_CENTER, 0);
    lv_obj_set_style_transform_pivot_y(layer, RIGHT_CENTER, 0);
    return layer;
}

static lv_obj_t * make_arc(lv_obj_t * parent, lv_color_t base_color, lv_color_t progress_color)
{
    lv_obj_t * arc = lv_arc_create(parent);
    lv_obj_remove_style_all(arc);
    lv_obj_set_pos(arc, RIGHT_RING_X, RIGHT_RING_Y);
    lv_obj_set_size(arc, RIGHT_RING_SIZE, RIGHT_RING_SIZE);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_range(arc, 0, 1000);
    lv_arc_set_value(arc, 0);

    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, base_color, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, RIGHT_RING_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);

    lv_obj_set_style_arc_color(arc, progress_color, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, RIGHT_RING_WIDTH, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);

    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc, 0, 0);
    return arc;
}

static lv_obj_t * make_disc(lv_obj_t * parent, int32_t diameter, lv_color_t color)
{
    lv_obj_t * disc = lv_obj_create(parent);
    lv_obj_remove_style_all(disc);
    lv_obj_set_size(disc, diameter, diameter);
    lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(disc, color, 0);
    lv_obj_set_style_bg_opa(disc, LV_OPA_COVER, 0);
    lv_obj_clear_flag(disc, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return disc;
}

static lv_obj_t * make_arc_segment(lv_obj_t * parent)
{
    lv_obj_t * arc = lv_arc_create(parent);
    lv_obj_remove_style_all(arc);
    lv_obj_set_pos(arc, RIGHT_RING_X, RIGHT_RING_Y);
    lv_obj_set_size(arc, RIGHT_RING_SIZE, RIGHT_RING_SIZE);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 0);
    lv_obj_set_style_arc_width(arc, RIGHT_RING_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, false, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc, 0, 0);
    lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
    return arc;
}

static lv_obj_t * make_bar(lv_obj_t * parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t * bar = lv_obj_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_pos(bar, x, y);
    lv_obj_set_size(bar, w, h);
    lv_obj_set_style_radius(bar, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(bar, UI_COLOR_WHITE, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return bar;
}

static lv_obj_t * make_line(lv_obj_t * parent, const lv_point_precise_t * points, uint32_t count,
                            int32_t width, lv_color_t color)
{
    lv_obj_t * line = lv_line_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, 0, 0);
    lv_obj_set_size(line, UI_RIGHT_W, UI_RIGHT_H);
    lv_line_set_points(line, points, count);
    lv_obj_set_style_line_width(line, width, 0);
    lv_obj_set_style_line_color(line, color, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return line;
}

static void hide_obj(lv_obj_t * obj)
{
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void show_obj(lv_obj_t * obj)
{
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void reset_layer_transform(lv_obj_t * layer)
{
    lv_obj_set_style_opa(layer, LV_OPA_COVER, 0);
    lv_obj_set_style_transform_scale_x(layer, RIGHT_SCALE_ONE, 0);
    lv_obj_set_style_transform_scale_y(layer, RIGHT_SCALE_ONE, 0);
}

static void set_layer_transform(lv_obj_t * layer, int32_t scale_x, int32_t scale_y, lv_opa_t opa)
{
    lv_obj_set_style_transform_scale_x(layer, scale_x, 0);
    lv_obj_set_style_transform_scale_y(layer, scale_y, 0);
    lv_obj_set_style_opa(layer, opa, 0);
}

static void hide_all_primary_layers(right_screen_t * screen)
{
    hide_obj(screen->home_layer);
    hide_obj(screen->action_layer);
    hide_obj(screen->upload_layer);
    hide_obj(screen->safety_layer);
    hide_obj(screen->charge_layer);
    hide_obj(screen->power_layer);
    screen->active_layer = NULL;
}

static void activate_layer(right_screen_t * screen, lv_obj_t * layer)
{
    show_obj(layer);
    reset_layer_transform(layer);
    screen->active_layer = layer;
}

static void set_cap_position(lv_obj_t * cap, float progress, int32_t diameter)
{
    const float p = clamp01(progress);
    const int16_t angle = (int16_t)(-90 + (int32_t)(p * 360.0f));
    const int32_t center_x = RIGHT_CENTER + ((RIGHT_RING_RADIUS * lv_trigo_cos(angle)) >> LV_TRIGO_SHIFT);
    const int32_t center_y = RIGHT_CENTER + ((RIGHT_RING_RADIUS * lv_trigo_sin(angle)) >> LV_TRIGO_SHIFT);
    lv_obj_set_pos(cap, center_x - diameter / 2, center_y - diameter / 2);
}

static void set_arc_progress(lv_obj_t * arc, lv_obj_t * cap, float progress,
                             lv_color_t dark, lv_color_t light, int32_t width)
{
    const float p = clamp01(progress);
    lv_obj_set_style_arc_color(arc, dark, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, width, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, width, LV_PART_INDICATOR);
    lv_arc_set_value(arc, (int32_t)(p * 1000.0f));

    if(p <= 0.001f) {
        hide_obj(cap);
        return;
    }

    const int32_t diameter = width;
    lv_obj_set_size(cap, diameter, diameter);
    lv_obj_set_style_bg_color(cap, light, 0);
    set_cap_position(cap, p, diameter);
    show_obj(cap);
}

static void hide_gradient(lv_obj_t * const * segments)
{
    for(uint32_t i = 0; i < RIGHT_GRADIENT_SEGMENTS; ++i) hide_obj(segments[i]);
}

static void set_gradient_progress(lv_obj_t * const * segments,
                                  lv_obj_t * start_cap,
                                  lv_obj_t * end_cap,
                                  float progress,
                                  lv_color_t dark,
                                  lv_color_t light)
{
    const float p = clamp01(progress);
    const int32_t active_degrees = (int32_t)(p * 360.0f);
    hide_gradient(segments);

    if(active_degrees <= 0) {
        hide_obj(start_cap);
        hide_obj(end_cap);
        return;
    }

    for(int32_t i = 0; i < RIGHT_GRADIENT_SEGMENTS; ++i) {
        const int32_t start = (i * 360) / RIGHT_GRADIENT_SEGMENTS;
        const int32_t limit = ((i + 1) * 360) / RIGHT_GRADIENT_SEGMENTS;
        if(start >= active_degrees) break;
        int32_t end = limit < active_degrees ? limit : active_degrees;
        if(end < 360 && end < active_degrees) ++end; /* one-degree overlap removes seams */
        const uint8_t mix = (uint8_t)((255 * start) / active_degrees);
        lv_arc_set_bg_angles(segments[i], (lv_value_precise_t)start, (lv_value_precise_t)end);
        lv_obj_set_style_arc_color(segments[i], lv_color_mix(light, dark, mix), LV_PART_MAIN);
        show_obj(segments[i]);
    }

    lv_obj_set_style_bg_color(start_cap, dark, 0);
    lv_obj_set_style_bg_color(end_cap, light, 0);
    set_cap_position(start_cap, 0.0f, RIGHT_RING_WIDTH);
    set_cap_position(end_cap, p, RIGHT_RING_WIDTH);
    show_obj(start_cap);
    show_obj(end_cap);
}

static void set_plus_visible(lv_obj_t * horizontal, lv_obj_t * vertical, bool visible)
{
    if(visible) {
        show_obj(horizontal);
        show_obj(vertical);
    }
    else {
        hide_obj(horizontal);
        hide_obj(vertical);
    }
}

static void render_home(right_screen_t * screen, const ui_view_model_t * model,
                        int32_t scale, lv_opa_t opacity)
{
    activate_layer(screen, screen->home_layer);
    set_layer_transform(screen->home_layer, scale, scale, opacity);

    const bool plus_only = model->right == UI_RIGHT_PLUS_ONLY;
    if(plus_only) {
        hide_obj(screen->home_base);
        hide_gradient(screen->home_gradient);
        hide_obj(screen->home_start_cap);
        hide_obj(screen->home_cap);
    }
    else {
        show_obj(screen->home_base);
        const bool has_target = model->app_ready && model->goal_ratio > 0.0;
        if(has_target) {
            set_gradient_progress(screen->home_gradient,
                                  screen->home_start_cap,
                                  screen->home_cap,
                                  (float)model->goal_ratio,
                                  ui_nutrient_dark(model->nutrient),
                                  ui_nutrient_light(model->nutrient));
        }
        else {
            hide_gradient(screen->home_gradient);
            hide_obj(screen->home_start_cap);
            hide_obj(screen->home_cap);
        }
    }

    set_plus_visible(screen->home_plus_h, screen->home_plus_v, true);
}

static void render_action(right_screen_t * screen, float progress, lv_color_t dark, lv_color_t light,
                          int32_t width, bool plus_visible, lv_opa_t check_opacity,
                          int32_t scale_x, int32_t scale_y, lv_opa_t layer_opacity)
{
    activate_layer(screen, screen->action_layer);
    set_layer_transform(screen->action_layer, scale_x, scale_y, layer_opacity);
    show_obj(screen->action_base);
    lv_obj_set_style_arc_width(screen->action_base, width, LV_PART_MAIN);

    if(progress > 0.001f) {
        show_obj(screen->action_progress);
        set_arc_progress(screen->action_progress, screen->action_cap, progress, dark, light, width);
    }
    else {
        hide_obj(screen->action_progress);
        hide_obj(screen->action_cap);
    }

    set_plus_visible(screen->action_plus_h, screen->action_plus_v, plus_visible);
    if(check_opacity == LV_OPA_TRANSP) {
        hide_obj(screen->action_check);
    }
    else {
        show_obj(screen->action_check);
        lv_obj_set_style_line_opa(screen->action_check, check_opacity, 0);
    }
}

static void update_upload_dots(right_screen_t * screen, uint32_t elapsed_ms)
{
    const uint32_t head = (elapsed_ms / 75U) % RIGHT_UPLOAD_DOT_COUNT;
    for(uint32_t index = 0; index < RIGHT_UPLOAD_DOT_COUNT; index++) {
        const int16_t angle = (int16_t)(-90 + (int32_t)(index * 30U));
        const int32_t radius = 62;
        const int32_t x = RIGHT_CENTER + ((radius * lv_trigo_cos(angle)) >> LV_TRIGO_SHIFT);
        const int32_t y = RIGHT_CENTER + ((radius * lv_trigo_sin(angle)) >> LV_TRIGO_SHIFT);
        const uint32_t age = (head + RIGHT_UPLOAD_DOT_COUNT - index) % RIGHT_UPLOAD_DOT_COUNT;
        int32_t opacity = 255 - (int32_t)(age * 19U);
        if(opacity < 32) opacity = 32;
        const int32_t diameter = age < 2U ? 10 : 7;

        lv_obj_set_size(screen->upload_dots[index], diameter, diameter);
        lv_obj_set_pos(screen->upload_dots[index], x - diameter / 2, y - diameter / 2);
        lv_obj_set_style_bg_opa(screen->upload_dots[index], (lv_opa_t)opacity, 0);
    }
}

static void render_upload(right_screen_t * screen, uint32_t elapsed_ms,
                          int32_t scale, lv_opa_t opacity)
{
    activate_layer(screen, screen->upload_layer);
    set_layer_transform(screen->upload_layer, scale, scale, opacity);
    update_upload_dots(screen, elapsed_ms);
}

static void update_safety_pulse(right_screen_t * screen, uint32_t elapsed_ms)
{
    const uint32_t cycle_ms = 1800U;
    for(uint32_t index = 0; index < RIGHT_PULSE_COUNT; index++) {
        const uint32_t offset = index * (cycle_ms / RIGHT_PULSE_COUNT);
        const uint32_t phase_ms = (elapsed_ms + offset) % cycle_ms;
        const float phase = (float)phase_ms / (float)cycle_ms;
        const float eased = ui_motion_ease(phase);
        const int32_t diameter = ui_motion_lerp_i32(28, 192, eased);
        const int32_t border_width = ui_motion_lerp_i32(14, 3, phase);
        const float fade = 1.0f - phase;
        const lv_opa_t opacity = (lv_opa_t)(235.0f * fade * fade);

        lv_obj_set_size(screen->safety_pulses[index], diameter, diameter);
        lv_obj_set_pos(screen->safety_pulses[index], RIGHT_CENTER - diameter / 2, RIGHT_CENTER - diameter / 2);
        lv_obj_set_style_border_width(screen->safety_pulses[index], border_width, 0);
        lv_obj_set_style_border_opa(screen->safety_pulses[index], opacity, 0);
    }
}

static void render_safety(right_screen_t * screen, uint32_t elapsed_ms,
                          int32_t scale, lv_opa_t opacity)
{
    activate_layer(screen, screen->safety_layer);
    set_layer_transform(screen->safety_layer, scale, scale, opacity);
    update_safety_pulse(screen, elapsed_ms);
}

static void render_charge(right_screen_t * screen, const ui_view_model_t * model,
                          int32_t scale, lv_opa_t opacity)
{
    activate_layer(screen, screen->charge_layer);
    set_layer_transform(screen->charge_layer, scale, scale, opacity);

    float progress = (float)model->battery_percent / 100.0f;
    progress = clamp01(progress);
    set_arc_progress(screen->charge_arc, screen->charge_cap, progress,
                     UI_COLOR_CHARGE, lv_color_hex(0xA9FF8A), RIGHT_RING_WIDTH);
    show_obj(screen->charge_arc);
    show_obj(screen->charge_bolt);
}

static void render_power(right_screen_t * screen)
{
    activate_layer(screen, screen->power_layer);
}

static void render_stable(right_screen_t * screen, const ui_view_model_t * model, uint32_t elapsed_ms)
{
    switch(model->right) {
        case UI_RIGHT_PLUS_ONLY:
        case UI_RIGHT_PLUS_RING:
            render_home(screen, model, RIGHT_SCALE_ONE, LV_OPA_COVER);
            break;

        case UI_RIGHT_FINISH_HOLD: {
            float progress = clamp01((float)model->hold_ratio);
            if(progress > 0.0f && progress < 0.008f) progress = 0.008f;
            render_action(screen, progress, UI_COLOR_RECORD, lv_color_hex(0xC19DEE),
                          RIGHT_RING_WIDTH, progress <= 0.0f, LV_OPA_TRANSP,
                          RIGHT_SCALE_ONE, RIGHT_SCALE_ONE, LV_OPA_COVER);
            break;
        }

        case UI_RIGHT_UPLOAD:
            render_upload(screen, elapsed_ms, RIGHT_SCALE_ONE, LV_OPA_COVER);
            break;

        case UI_RIGHT_SAFETY:
            render_safety(screen, elapsed_ms, RIGHT_SCALE_ONE, LV_OPA_COVER);
            break;

        case UI_RIGHT_CHARGE:
            render_charge(screen, model, RIGHT_SCALE_ONE, LV_OPA_COVER);
            break;

        case UI_RIGHT_OFF:
        default:
            render_power(screen);
            break;
    }
}

static void render_noop(right_screen_t * screen, const ui_view_model_t * before,
                        const ui_view_model_t * after, uint32_t elapsed_ms, uint32_t duration_ms)
{
    const ui_view_model_t * model = (duration_ms > 0U && elapsed_ms < duration_ms) ? before : after;
    render_stable(screen, model, elapsed_ms);
}

static void render_boot(right_screen_t * screen, const ui_view_model_t * after, uint32_t elapsed_ms)
{
    if(elapsed_ms < 6300U) {
        render_power(screen);
        return;
    }

    const float enter = ui_motion_segment(elapsed_ms, 6300U, 8610U);
    render_stable(screen, after, elapsed_ms);
    if(screen->active_layer != screen->power_layer) {
        const int32_t scale = scale_from_progress(enter);
        const lv_opa_t opacity = ui_motion_lerp_opa(LV_OPA_TRANSP, LV_OPA_COVER, enter);
        set_layer_transform(screen->active_layer, scale, scale, opacity);
    }
}

static void render_wake(right_screen_t * screen, const ui_view_model_t * after, uint32_t elapsed_ms)
{
    if(elapsed_ms < 650U) {
        render_power(screen);
        return;
    }

    const float enter = ui_motion_segment(elapsed_ms, 650U, 1000U);
    render_stable(screen, after, elapsed_ms);
    if(screen->active_layer != screen->power_layer) {
        const int32_t scale = scale_from_progress(enter);
        const lv_opa_t opacity = ui_motion_lerp_opa(LV_OPA_TRANSP, LV_OPA_COVER, enter);
        set_layer_transform(screen->active_layer, scale, scale, opacity);
    }
}

static void render_sleep(right_screen_t * screen, const ui_view_model_t * before, uint32_t elapsed_ms)
{
    if(elapsed_ms >= UI_MS_SLEEP) {
        render_power(screen);
        return;
    }

    const float out = ui_motion_segment(elapsed_ms, 0U, UI_MS_SLEEP);
    render_stable(screen, before, elapsed_ms);
    if(screen->active_layer != screen->power_layer) {
        set_layer_transform(screen->active_layer, RIGHT_SCALE_ONE, RIGHT_SCALE_ONE,
                            ui_motion_lerp_opa(LV_OPA_COVER, LV_OPA_TRANSP, out));
    }
}

static void render_home_swap(right_screen_t * screen, const ui_view_model_t * before,
                             const ui_view_model_t * after, uint32_t elapsed_ms,
                             uint32_t out_end_ms, uint32_t in_start_ms, uint32_t in_end_ms)
{
    if(elapsed_ms < out_end_ms) {
        const float out = ui_motion_segment(elapsed_ms, 0U, out_end_ms);
        const int32_t scale = ui_motion_lerp_i32(RIGHT_SCALE_ONE, RIGHT_SCALE_POINT, out);
        render_home(screen, before, scale,
                    ui_motion_lerp_opa(LV_OPA_COVER, LV_OPA_TRANSP, out));
    }
    else if(elapsed_ms < in_start_ms) {
        render_power(screen);
    }
    else {
        const float enter = ui_motion_segment(elapsed_ms, in_start_ms, in_end_ms);
        render_home(screen, after, scale_from_progress(enter),
                    ui_motion_lerp_opa(LV_OPA_TRANSP, LV_OPA_COVER, enter));
    }
}

static void render_add(right_screen_t * screen, const ui_view_model_t * before,
                       const ui_view_model_t * after, uint32_t elapsed_ms)
{
    if(elapsed_ms < 400U) {
        render_home(screen, before, RIGHT_SCALE_ONE, LV_OPA_COVER);
        return;
    }

    if(elapsed_ms < 550U) {
        const float out = ui_motion_segment(elapsed_ms, 400U, 550U);
        render_home(screen, before,
                    ui_motion_lerp_i32(RIGHT_SCALE_ONE, RIGHT_SCALE_POINT, out),
                    ui_motion_lerp_opa(LV_OPA_COVER, LV_OPA_TRANSP, out));
        return;
    }

    if(elapsed_ms < 1990U) {
        const float grow = ui_motion_segment(elapsed_ms, 550U, 1990U);
        const float start = before->app_ready ? clamp01((float)before->goal_ratio) : 0.0f;
        const float target = after->app_ready ? clamp01((float)after->goal_ratio) : 0.0f;
        const float progress = start + (target - start) * grow;
        const int32_t width = ui_motion_lerp_i32(RIGHT_RING_WIDTH_ACTION, RIGHT_RING_WIDTH, grow);
        render_action(screen, progress,
                      ui_nutrient_dark(after->nutrient), ui_nutrient_light(after->nutrient),
                      width, false, LV_OPA_TRANSP,
                      scale_from_progress(grow), scale_from_progress(grow), LV_OPA_COVER);
        return;
    }

    if(elapsed_ms < 2160U) {
        const float out = ui_motion_segment(elapsed_ms, 1990U, 2160U);
        render_action(screen,
                      after->app_ready ? clamp01((float)after->goal_ratio) : 0.0f,
                      ui_nutrient_dark(after->nutrient), ui_nutrient_light(after->nutrient),
                      RIGHT_RING_WIDTH, false, LV_OPA_TRANSP,
                      ui_motion_lerp_i32(RIGHT_SCALE_ONE, RIGHT_SCALE_POINT, out),
                      ui_motion_lerp_i32(RIGHT_SCALE_ONE, RIGHT_SCALE_POINT, out),
                      ui_motion_lerp_opa(LV_OPA_COVER, LV_OPA_TRANSP, out));
        return;
    }

    if(elapsed_ms < 2200U) {
        render_power(screen);
        return;
    }

    const float enter = ui_motion_segment(elapsed_ms, 2200U, 2700U);
    render_home(screen, after, scale_from_progress(enter),
                ui_motion_lerp_opa(LV_OPA_TRANSP, LV_OPA_COVER, enter));
}

static void render_finish_upload(right_screen_t * screen, const ui_view_model_t * before,
                                 uint32_t elapsed_ms)
{
    if(elapsed_ms < 800U) {
        render_home(screen, before, RIGHT_SCALE_ONE, LV_OPA_COVER);
        return;
    }

    if(elapsed_ms < 2600U) {
        float progress = ui_motion_segment(elapsed_ms, 800U, 2000U);
        if(progress < 0.008f) progress = 0.008f;
        render_action(screen, progress, UI_COLOR_RECORD, lv_color_hex(0xC19DEE),
                      RIGHT_RING_WIDTH, false, LV_OPA_TRANSP,
                      RIGHT_SCALE_ONE, RIGHT_SCALE_ONE, LV_OPA_COVER);
        return;
    }

    if(elapsed_ms < 3000U) {
        const float check = ui_motion_segment(elapsed_ms, 2600U, 2840U);
        render_action(screen, 1.0f, UI_COLOR_RECORD, lv_color_hex(0xC19DEE),
                      RIGHT_RING_WIDTH, false,
                      ui_motion_lerp_opa(LV_OPA_TRANSP, LV_OPA_COVER, check),
                      RIGHT_SCALE_ONE, RIGHT_SCALE_ONE, LV_OPA_COVER);
        return;
    }

    if(elapsed_ms < 3400U) {
        const float out = ui_motion_segment(elapsed_ms, 3000U, 3400U);
        const int32_t scale = ui_motion_lerp_i32(RIGHT_SCALE_ONE, RIGHT_SCALE_POINT, out);
        render_action(screen, 1.0f, UI_COLOR_RECORD, lv_color_hex(0xC19DEE),
                      RIGHT_RING_WIDTH, false, LV_OPA_COVER,
                      scale, scale, ui_motion_lerp_opa(LV_OPA_COVER, LV_OPA_TRANSP, out));
        return;
    }

    const float enter = ui_motion_segment(elapsed_ms, 3400U, 3800U);
    render_upload(screen, elapsed_ms - 3400U, scale_from_progress(enter),
                  ui_motion_lerp_opa(LV_OPA_TRANSP, LV_OPA_COVER, enter));
}

static void render_record_success(right_screen_t * screen, const ui_view_model_t * before,
                                  const ui_view_model_t * after, uint32_t elapsed_ms)
{
    if(elapsed_ms < 300U) {
        const float out = ui_motion_segment(elapsed_ms, 0U, 300U);
        render_stable(screen, before, elapsed_ms);
        if(screen->active_layer != screen->power_layer) {
            const int32_t scale = ui_motion_lerp_i32(RIGHT_SCALE_ONE, RIGHT_SCALE_POINT, out);
            set_layer_transform(screen->active_layer, scale, scale,
                                ui_motion_lerp_opa(LV_OPA_COVER, LV_OPA_TRANSP, out));
        }
        return;
    }

    if(elapsed_ms < 1000U) {
        const float flip = ui_motion_segment(elapsed_ms, 300U, 850U);
        const int32_t scale_x = ui_motion_lerp_i32(20, RIGHT_SCALE_ONE, flip);
        const int32_t scale_y = ui_motion_lerp_i32(150, RIGHT_SCALE_ONE, flip);
        render_action(screen, 1.0f, UI_COLOR_RECORD, lv_color_hex(0xC19DEE),
                      RIGHT_RING_WIDTH, false, LV_OPA_TRANSP,
                      scale_x, scale_y,
                      ui_motion_lerp_opa(LV_OPA_TRANSP, LV_OPA_COVER, flip));
        return;
    }

    if(elapsed_ms < 2000U) {
        const float check = ui_motion_segment(elapsed_ms, 1000U, 1300U);
        render_action(screen, 1.0f, UI_COLOR_RECORD, lv_color_hex(0xC19DEE),
                      RIGHT_RING_WIDTH, false,
                      ui_motion_lerp_opa(LV_OPA_TRANSP, LV_OPA_COVER, check),
                      RIGHT_SCALE_ONE, RIGHT_SCALE_ONE, LV_OPA_COVER);
        return;
    }

    if(elapsed_ms < 2300U) {
        const float out = ui_motion_segment(elapsed_ms, 2000U, 2300U);
        const int32_t scale = ui_motion_lerp_i32(RIGHT_SCALE_ONE, RIGHT_SCALE_POINT, out);
        render_action(screen, 1.0f, UI_COLOR_RECORD, lv_color_hex(0xC19DEE),
                      RIGHT_RING_WIDTH, false, LV_OPA_COVER,
                      scale, scale, ui_motion_lerp_opa(LV_OPA_COVER, LV_OPA_TRANSP, out));
        return;
    }

    if(elapsed_ms < 2500U) {
        render_power(screen);
        return;
    }

    const float enter = ui_motion_segment(elapsed_ms, 2500U, 3000U);
    render_stable(screen, after, elapsed_ms);
    if(screen->active_layer != screen->power_layer) {
        const int32_t scale = scale_from_progress(enter);
        set_layer_transform(screen->active_layer, scale, scale,
                            ui_motion_lerp_opa(LV_OPA_TRANSP, LV_OPA_COVER, enter));
    }
}

static void render_alert_enter(right_screen_t * screen, const ui_view_model_t * before,
                               uint32_t elapsed_ms)
{
    if(elapsed_ms < 600U) {
        const float out = ui_motion_segment(elapsed_ms, 0U, 600U);
        render_stable(screen, before, elapsed_ms);
        if(screen->active_layer != screen->power_layer) {
            const int32_t scale = ui_motion_lerp_i32(RIGHT_SCALE_ONE, RIGHT_SCALE_POINT, out);
            set_layer_transform(screen->active_layer, scale, scale,
                                ui_motion_lerp_opa(LV_OPA_COVER, LV_OPA_TRANSP, out));
        }
        return;
    }

    const float enter = ui_motion_segment(elapsed_ms, 600U, 1000U);
    render_safety(screen, elapsed_ms - 600U, scale_from_progress(enter),
                  ui_motion_lerp_opa(LV_OPA_TRANSP, LV_OPA_COVER, enter));
}

static void render_alert_exit(right_screen_t * screen, const ui_view_model_t * after,
                              uint32_t elapsed_ms)
{
    if(elapsed_ms < 700U) {
        const float out = ui_motion_segment(elapsed_ms, 0U, 700U);
        render_safety(screen, elapsed_ms,
                      ui_motion_lerp_i32(RIGHT_SCALE_ONE, RIGHT_SCALE_POINT, out),
                      ui_motion_lerp_opa(LV_OPA_COVER, LV_OPA_TRANSP, out));
        return;
    }

    const float enter = ui_motion_segment(elapsed_ms, 700U, 1100U);
    render_stable(screen, after, elapsed_ms);
    if(screen->active_layer != screen->power_layer) {
        const int32_t scale = scale_from_progress(enter);
        set_layer_transform(screen->active_layer, scale, scale,
                            ui_motion_lerp_opa(LV_OPA_TRANSP, LV_OPA_COVER, enter));
    }
}

static void render_charging_enter(right_screen_t * screen, const ui_view_model_t * before,
                                  const ui_view_model_t * after, uint32_t elapsed_ms)
{
    if(elapsed_ms < 450U) {
        const float out = ui_motion_segment(elapsed_ms, 0U, 450U);
        render_stable(screen, before, elapsed_ms);
        if(screen->active_layer != screen->power_layer) {
            const int32_t scale = ui_motion_lerp_i32(RIGHT_SCALE_ONE, RIGHT_SCALE_POINT, out);
            set_layer_transform(screen->active_layer, scale, scale,
                                ui_motion_lerp_opa(LV_OPA_COVER, LV_OPA_TRANSP, out));
        }
        return;
    }

    if(elapsed_ms < 650U) {
        render_power(screen);
        return;
    }

    const float enter = ui_motion_segment(elapsed_ms, 650U, 1450U);
    render_charge(screen, after, scale_from_progress(enter),
                  ui_motion_lerp_opa(LV_OPA_TRANSP, LV_OPA_COVER, enter));
}

static void render_charging_exit(right_screen_t * screen, const ui_view_model_t * before,
                                 const ui_view_model_t * after, uint32_t elapsed_ms)
{
    if(elapsed_ms < 500U) {
        const float out = ui_motion_segment(elapsed_ms, 0U, 500U);
        render_charge(screen, before,
                      ui_motion_lerp_i32(RIGHT_SCALE_ONE, RIGHT_SCALE_POINT, out),
                      ui_motion_lerp_opa(LV_OPA_COVER, LV_OPA_TRANSP, out));
        return;
    }

    if(elapsed_ms < 650U) {
        render_power(screen);
        return;
    }

    const float enter = ui_motion_segment(elapsed_ms, 650U, 1100U);
    render_stable(screen, after, elapsed_ms);
    if(screen->active_layer != screen->power_layer) {
        const int32_t scale = scale_from_progress(enter);
        set_layer_transform(screen->active_layer, scale, scale,
                            ui_motion_lerp_opa(LV_OPA_TRANSP, LV_OPA_COVER, enter));
    }
}

static void render_critical_shutdown(right_screen_t * screen, const ui_view_model_t * before,
                                     uint32_t elapsed_ms)
{
    if(elapsed_ms < 600U) {
        const float out = ui_motion_segment(elapsed_ms, 0U, 600U);
        render_stable(screen, before, elapsed_ms);
        if(screen->active_layer != screen->power_layer) {
            const int32_t scale = ui_motion_lerp_i32(RIGHT_SCALE_ONE, RIGHT_SCALE_POINT, out);
            set_layer_transform(screen->active_layer, scale, scale,
                                ui_motion_lerp_opa(LV_OPA_COVER, LV_OPA_TRANSP, out));
        }
        return;
    }

    if(elapsed_ms < 6000U) {
        const float enter = ui_motion_segment(elapsed_ms, 600U, 1000U);
        render_safety(screen, elapsed_ms - 600U, scale_from_progress(enter),
                      ui_motion_lerp_opa(LV_OPA_TRANSP, LV_OPA_COVER, enter));
        return;
    }

    if(elapsed_ms < 7750U) {
        const float out = ui_motion_segment(elapsed_ms, 6000U, 7750U);
        render_safety(screen, elapsed_ms - 600U,
                      ui_motion_lerp_i32(RIGHT_SCALE_ONE, RIGHT_SCALE_POINT, out),
                      ui_motion_lerp_opa(LV_OPA_COVER, LV_OPA_TRANSP, out));
        return;
    }

    render_power(screen);
}

right_screen_t * right_screen_create(lv_obj_t * parent)
{
    if(parent == NULL) return NULL;

    right_screen_t * screen = lv_malloc_zeroed(sizeof(*screen));
    if(screen == NULL) return NULL;

    screen->root = lv_obj_create(parent);
    lv_obj_remove_style_all(screen->root);
    lv_obj_set_pos(screen->root, 0, 0);
    lv_obj_set_size(screen->root, UI_RIGHT_W, UI_RIGHT_H);
    lv_obj_set_style_bg_color(screen->root, UI_COLOR_BLACK, 0);
    lv_obj_set_style_bg_opa(screen->root, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(screen->root, LV_OBJ_FLAG_CLICKABLE);

    screen->home_layer = make_layer(screen->root);
    screen->action_layer = make_layer(screen->root);
    screen->upload_layer = make_layer(screen->root);
    screen->safety_layer = make_layer(screen->root);
    screen->charge_layer = make_layer(screen->root);
    screen->power_layer = make_layer(screen->root);
    lv_obj_set_style_bg_color(screen->power_layer, UI_COLOR_BLACK, 0);
    lv_obj_set_style_bg_opa(screen->power_layer, LV_OPA_COVER, 0);

    screen->home_base = make_arc(screen->home_layer, UI_COLOR_MUTED, UI_COLOR_CAL_DARK);
    for(uint32_t i = 0; i < RIGHT_GRADIENT_SEGMENTS; ++i) {
        screen->home_gradient[i] = make_arc_segment(screen->home_layer);
    }
    screen->home_start_cap = make_disc(screen->home_layer, RIGHT_RING_WIDTH, UI_COLOR_CAL_DARK);
    screen->home_cap = make_disc(screen->home_layer, RIGHT_RING_WIDTH, UI_COLOR_CAL_LIGHT);
    screen->home_plus_h = make_bar(screen->home_layer, 83, 113, 72, 12);
    screen->home_plus_v = make_bar(screen->home_layer, 113, 84, 12, 70);

    screen->action_base = make_arc(screen->action_layer, UI_COLOR_MUTED, UI_COLOR_RECORD);
    screen->action_progress = make_arc(screen->action_layer, UI_COLOR_BLACK, UI_COLOR_RECORD);
    lv_obj_set_style_arc_opa(screen->action_progress, LV_OPA_TRANSP, LV_PART_MAIN);
    screen->action_cap = make_disc(screen->action_layer, RIGHT_RING_WIDTH, lv_color_hex(0xC19DEE));
    screen->action_plus_h = make_bar(screen->action_layer, 83, 113, 72, 12);
    screen->action_plus_v = make_bar(screen->action_layer, 113, 84, 12, 70);
    screen->action_check_points[0] = (lv_point_precise_t){82, 122};
    screen->action_check_points[1] = (lv_point_precise_t){108, 146};
    screen->action_check_points[2] = (lv_point_precise_t){158, 91};
    screen->action_check = make_line(screen->action_layer, screen->action_check_points, 3U, 12, UI_COLOR_WHITE);

    for(uint32_t index = 0; index < RIGHT_UPLOAD_DOT_COUNT; index++) {
        screen->upload_dots[index] = make_disc(screen->upload_layer, 7, UI_COLOR_RECORD);
    }

    for(uint32_t index = 0; index < RIGHT_PULSE_COUNT; index++) {
        screen->safety_pulses[index] = lv_obj_create(screen->safety_layer);
        lv_obj_remove_style_all(screen->safety_pulses[index]);
        lv_obj_set_style_radius(screen->safety_pulses[index], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(screen->safety_pulses[index], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(screen->safety_pulses[index], UI_COLOR_ALERT, 0);
        lv_obj_set_style_border_width(screen->safety_pulses[index], 8, 0);
        lv_obj_clear_flag(screen->safety_pulses[index], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    }

    screen->charge_arc = make_arc(screen->charge_layer, lv_color_hex(0x596551), UI_COLOR_CHARGE);
    screen->charge_cap = make_disc(screen->charge_layer, RIGHT_RING_WIDTH, lv_color_hex(0xA9FF8A));
    screen->charge_bolt_points[0] = (lv_point_precise_t){131, 78};
    screen->charge_bolt_points[1] = (lv_point_precise_t){105, 119};
    screen->charge_bolt_points[2] = (lv_point_precise_t){122, 119};
    screen->charge_bolt_points[3] = (lv_point_precise_t){108, 162};
    screen->charge_bolt_points[4] = (lv_point_precise_t){139, 111};
    screen->charge_bolt_points[5] = (lv_point_precise_t){121, 111};
    screen->charge_bolt = make_line(screen->charge_layer, screen->charge_bolt_points, 6U, 9,
                                    lv_color_hex(0xA9FF8A));

    hide_all_primary_layers(screen);
    render_power(screen);
    return screen;
}

void right_screen_render(right_screen_t * screen,
                         const ui_view_model_t * before,
                         const ui_view_model_t * after,
                         ui_transition_t transition,
                         uint32_t elapsed_ms)
{
    if(screen == NULL || before == NULL || after == NULL) return;

    hide_all_primary_layers(screen);

    switch(transition) {
        case UI_TRANSITION_BOOT:
            render_boot(screen, after, elapsed_ms);
            break;

        case UI_TRANSITION_WAKE:
            render_wake(screen, after, elapsed_ms);
            break;

        case UI_TRANSITION_SLEEP:
            render_sleep(screen, before, elapsed_ms);
            break;

        case UI_TRANSITION_ADD:
            render_add(screen, before, after, elapsed_ms);
            break;

        case UI_TRANSITION_CONNECTED:
            render_home_swap(screen, before, after, elapsed_ms, 600U, 750U, 1300U);
            break;

        case UI_TRANSITION_NUTRIENT:
            render_home_swap(screen, before, after, elapsed_ms, 400U, 400U, 700U);
            break;

        case UI_TRANSITION_FINISH_UPLOAD:
            render_finish_upload(screen, before, elapsed_ms);
            break;

        case UI_TRANSITION_RECORD_SUCCESS:
            render_record_success(screen, before, after, elapsed_ms);
            break;

        case UI_TRANSITION_ALERT_ENTER:
            render_alert_enter(screen, before, elapsed_ms);
            break;

        case UI_TRANSITION_ALERT_EXIT:
            render_alert_exit(screen, after, elapsed_ms);
            break;

        case UI_TRANSITION_CHARGING_ENTER:
            render_charging_enter(screen, before, after, elapsed_ms);
            break;

        case UI_TRANSITION_CHARGING_EXIT:
            render_charging_exit(screen, before, after, elapsed_ms);
            break;

        case UI_TRANSITION_CRITICAL_SHUTDOWN:
            render_critical_shutdown(screen, before, elapsed_ms);
            break;

        case UI_TRANSITION_NONE:
            render_stable(screen, after, elapsed_ms);
            break;

        case UI_TRANSITION_RECOGNIZE:
        case UI_TRANSITION_FOOD_OVERRIDE:
        case UI_TRANSITION_TARE_SUCCESS:
        case UI_TRANSITION_GUIDE_ENTER:
        case UI_TRANSITION_GUIDE_TIMEOUT:
        case UI_TRANSITION_GUIDE_TO_FOOD:
        case UI_TRANSITION_UNIT:
        case UI_TRANSITION_LOW_BATTERY:
        default:
            render_noop(screen, before, after, elapsed_ms, 0U);
            break;
    }
}

lv_obj_t * right_screen_object(right_screen_t * screen)
{
    return screen != NULL ? screen->root : NULL;
}

lv_obj_t * ui_right_screen_touch_target(ui_right_screen_t * screen)
{
    return right_screen_object(screen);
}

void right_screen_destroy(right_screen_t * screen)
{
    if(screen == NULL) return;
    if(screen->root != NULL) lv_obj_delete(screen->root);
    lv_free(screen);
}
