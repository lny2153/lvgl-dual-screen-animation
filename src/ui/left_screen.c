#include "left_screen.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "motion.h"

LV_IMAGE_DECLARE(img_eyes);
LV_IMAGE_DECLARE(img_apple);

struct ui_left_screen {
    lv_obj_t *root;
    lv_obj_t *page;
    lv_obj_t *weight_slot;
    lv_obj_t *nutrient_slot;
    lv_obj_t *divider;

    lv_obj_t *weight_total;
    lv_obj_t *weight_current;
    lv_obj_t *weight_unit;
    lv_obj_t *nutrient_title;
    lv_obj_t *nutrient_total;
    lv_obj_t *nutrient_current;
    lv_obj_t *nutrient_unit;

    lv_obj_t *eyes;
    lv_obj_t *recognize_gif;
    bool recognize_gif_started;
    lv_obj_t *food_group;
    lv_obj_t *food_image;
    lv_obj_t *food_name;

    lv_obj_t *guide_scene;
    lv_obj_t *pairing_scene;
    lv_obj_t *overload_scene;
    lv_obj_t *low_scene;
    lv_obj_t *critical_scene;
    lv_obj_t *status_connected;
    lv_obj_t *status_tare;
    lv_obj_t *status_recorded;
    lv_obj_t *upload_scene;
    lv_obj_t *scene_artifact_mask;

    lv_obj_t *picker;
    lv_obj_t *charging;
    lv_obj_t *charge_value;
    lv_obj_t *charge_percent;
    lv_obj_t *charge_time;
    lv_obj_t *charge_spinner[8];

    lv_obj_t *unit_mask_top;
    lv_obj_t *unit_mask_bottom;
};

static lv_obj_t *make_group(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, UI_LEFT_W, UI_LEFT_H);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

static lv_obj_t *make_rect(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t color, int radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, radius, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

static lv_obj_t *make_label(
    lv_obj_t *parent,
    int x,
    int y,
    int w,
    int h,
    const char *text,
    const lv_font_t *font,
    lv_color_t color,
    lv_text_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_remove_style_all(label);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, w, h);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_set_style_text_align(label, align, LV_PART_MAIN);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, LV_PART_MAIN);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_label_set_text(label, text);
    return label;
}

static void set_hidden(lv_obj_t *obj, bool hidden)
{
    if(hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void set_opa(lv_obj_t *obj, lv_opa_t opacity)
{
    lv_obj_set_style_opa(obj, opacity, LV_PART_MAIN);
}

static void hide_primary(ui_left_screen_t *screen)
{
    set_hidden(screen->eyes, true);
    set_hidden(screen->recognize_gif, true);
    set_hidden(screen->food_group, true);
    set_hidden(screen->guide_scene, true);
    set_hidden(screen->pairing_scene, true);
    set_hidden(screen->overload_scene, true);
    set_hidden(screen->low_scene, true);
    set_hidden(screen->critical_scene, true);
    set_hidden(screen->status_connected, true);
    set_hidden(screen->status_tare, true);
    set_hidden(screen->status_recorded, true);
    set_hidden(screen->upload_scene, true);
    set_hidden(screen->scene_artifact_mask, true);
    set_hidden(screen->picker, true);
    set_hidden(screen->charging, true);
}

static void format_value(char *buffer, size_t capacity, double value, bool decimal)
{
    if(value > -0.05 && value < 0.05) value = 0.0;
    if(decimal) snprintf(buffer, capacity, "%.1f", value);
    else snprintf(buffer, capacity, "%.0f", value);
}

static void format_weight(char *buffer, size_t capacity, double grams, ui_weight_unit_t unit)
{
    switch(unit) {
        case UI_UNIT_OZ:
            snprintf(buffer, capacity, "%.1f", grams / 28.349523125);
            break;
        case UI_UNIT_LB_OZ: {
            double total_ounces = grams / 28.349523125;
            int pounds = (int)(total_ounces / 16.0);
            double ounces = total_ounces - (double)pounds * 16.0;
            snprintf(buffer, capacity, "%d:%.1f", pounds, ounces);
            break;
        }
        case UI_UNIT_ML:
            /* Simulator uses water-equivalent volume. Firmware must inject density. */
            snprintf(buffer, capacity, "%.0f", grams);
            break;
        case UI_UNIT_G:
        default:
            snprintf(buffer, capacity, "%.0f", grams);
            break;
    }
}

static void update_data(ui_left_screen_t *screen, const ui_view_model_t *model)
{
    char text[32];
    const bool nutrient_decimal = model->nutrient != UI_NUTRIENT_CALORIES;

    format_weight(text, sizeof(text), model->total_weight_g, model->unit);
    lv_label_set_text(screen->weight_total, text);
    format_weight(text, sizeof(text), model->current_weight_g, model->unit);
    lv_label_set_text(screen->weight_current, text);
    lv_label_set_text(screen->weight_unit, ui_weight_unit_name(model->unit));

    lv_label_set_text(screen->nutrient_title, ui_nutrient_name(model->nutrient));
    format_value(text, sizeof(text), model->total_nutrient, nutrient_decimal);
    lv_label_set_text(screen->nutrient_total, text);
    format_value(text, sizeof(text), model->current_nutrient, nutrient_decimal);
    lv_label_set_text(screen->nutrient_current, text);
    lv_label_set_text(screen->nutrient_unit, ui_nutrient_unit(model->nutrient));

    lv_color_t nutrient_color = ui_nutrient_dark(model->nutrient);
    lv_obj_set_style_text_color(screen->nutrient_title, nutrient_color, LV_PART_MAIN);
    lv_obj_set_style_text_color(lv_obj_get_child(screen->nutrient_slot, 0), nutrient_color, LV_PART_MAIN);

    const lv_font_t *weight_font = model->unit == UI_UNIT_LB_OZ ? &font_ui_36 : &font_digits_84;
    lv_obj_set_style_text_font(screen->weight_current, weight_font, LV_PART_MAIN);
    lv_obj_set_style_text_font(screen->weight_total, &font_digits_30, LV_PART_MAIN);
    lv_obj_set_style_text_font(screen->nutrient_total, &font_digits_30, LV_PART_MAIN);
    lv_obj_set_style_text_font(screen->nutrient_current, &font_digits_84, LV_PART_MAIN);

    if(model->food_name && model->food_name[0]) lv_label_set_text(screen->food_name, model->food_name);
    else lv_label_set_text(screen->food_name, "苹果");

    lv_label_set_text_fmt(screen->charge_value, "%d", model->battery_percent);
    lv_label_set_text(screen->charge_percent, "%");
    if(model->estimated_charge_time_valid) {
        lv_label_set_text_fmt(screen->charge_time, "预计 %d 分钟充满", model->charge_minutes);
    }
    else {
        lv_label_set_text(screen->charge_time, "充电中");
    }
}

static void reset_geometry(ui_left_screen_t *screen)
{
    set_opa(screen->root, LV_OPA_COVER);
    set_opa(screen->page, LV_OPA_COVER);
    set_opa(screen->weight_slot, LV_OPA_COVER);
    set_opa(screen->nutrient_slot, LV_OPA_COVER);
    set_opa(screen->divider, LV_OPA_COVER);
    set_opa(screen->eyes, LV_OPA_COVER);
    set_opa(screen->food_group, LV_OPA_COVER);
    lv_image_set_scale(screen->eyes, 256);
    lv_image_set_scale(screen->food_image, 256);
    lv_obj_set_style_transform_scale(screen->food_group, 256, LV_PART_MAIN);
    lv_obj_set_style_transform_scale(screen->nutrient_slot, 256, LV_PART_MAIN);
    lv_obj_set_style_transform_scale(screen->weight_slot, 256, LV_PART_MAIN);
    lv_obj_set_style_translate_y(screen->weight_slot, 0, LV_PART_MAIN);
    lv_obj_set_style_translate_y(screen->nutrient_slot, 0, LV_PART_MAIN);
    set_hidden(screen->unit_mask_top, true);
    set_hidden(screen->unit_mask_bottom, true);
}

static void show_data(ui_left_screen_t *screen, lv_opa_t opacity)
{
    set_hidden(screen->page, false);
    set_hidden(screen->weight_slot, false);
    set_hidden(screen->nutrient_slot, false);
    set_hidden(screen->divider, false);
    set_opa(screen->weight_slot, opacity);
    set_opa(screen->nutrient_slot, opacity);
    set_opa(screen->divider, opacity);
}

static void show_stable(ui_left_screen_t *screen, const ui_view_model_t *model)
{
    hide_primary(screen);
    reset_geometry(screen);
    update_data(screen, model);

    switch(model->primary) {
        case UI_PRIMARY_EYES:
            show_data(screen, LV_OPA_COVER);
            set_hidden(screen->eyes, false);
            break;
        case UI_PRIMARY_FOOD:
            show_data(screen, LV_OPA_COVER);
            set_hidden(screen->food_group, false);
            break;
        case UI_PRIMARY_PAIRING:
            show_data(screen, LV_OPA_TRANSP);
            set_hidden(screen->pairing_scene, false);
            break;
        case UI_PRIMARY_GUIDE:
            show_data(screen, LV_OPA_TRANSP);
            set_hidden(screen->guide_scene, false);
            break;
        case UI_PRIMARY_PICKER:
            show_data(screen, LV_OPA_TRANSP);
            set_hidden(screen->picker, false);
            break;
        case UI_PRIMARY_UPLOAD:
            show_data(screen, LV_OPA_TRANSP);
            set_hidden(screen->upload_scene, false);
            break;
        case UI_PRIMARY_ALERT:
            show_data(screen, LV_OPA_TRANSP);
            if(model->alert == UI_ALERT_LOW_BATTERY) set_hidden(screen->low_scene, false);
            else if(model->alert == UI_ALERT_CRITICAL_BATTERY) {
                set_hidden(screen->critical_scene, false);
                set_hidden(screen->scene_artifact_mask, false);
            }
            else {
                set_hidden(screen->overload_scene, false);
                set_hidden(screen->scene_artifact_mask, false);
            }
            break;
        case UI_PRIMARY_STATUS:
            show_data(screen, LV_OPA_TRANSP);
            if(model->status == UI_STATUS_TARE_DONE) set_hidden(screen->status_tare, false);
            else if(model->status == UI_STATUS_RECORDED) set_hidden(screen->status_recorded, false);
            else set_hidden(screen->status_connected, false);
            break;
        case UI_PRIMARY_CHARGING:
            show_data(screen, LV_OPA_TRANSP);
            set_hidden(screen->charging, false);
            break;
        case UI_PRIMARY_OFF:
        default:
            show_data(screen, LV_OPA_TRANSP);
            break;
    }
}

static void show_status_frame(
    ui_left_screen_t *screen,
    ui_status_kind_t kind,
    uint32_t elapsed_ms)
{
    lv_obj_t *status = screen->status_connected;
    if(kind == UI_STATUS_TARE_DONE) status = screen->status_tare;
    else if(kind == UI_STATUS_RECORDED) status = screen->status_recorded;

    hide_primary(screen);
    show_data(screen, LV_OPA_TRANSP);

    if(elapsed_ms < 600U || elapsed_ms >= 2400U) return;
    set_hidden(status, false);

    if(elapsed_ms < 850U) {
        float p = ui_motion_segment(elapsed_ms, 600U, 850U);
        lv_obj_set_style_transform_scale_x(status, ui_motion_lerp_i32(20, 256, p), LV_PART_MAIN);
        lv_obj_set_style_transform_scale_y(status, ui_motion_lerp_i32(110, 256, p), LV_PART_MAIN);
        set_opa(status, ui_motion_lerp_opa(0, 255, p));
    }
    else if(elapsed_ms < 1100U) {
        lv_obj_set_style_transform_scale(status, 256, LV_PART_MAIN);
        set_opa(status, ui_motion_lerp_opa(180, 255, ui_motion_segment(elapsed_ms, 850U, 1100U)));
    }
    else if(elapsed_ms < 2000U) {
        lv_obj_set_style_transform_scale(status, 256, LV_PART_MAIN);
        set_opa(status, LV_OPA_COVER);
    }
    else {
        float p = ui_motion_segment(elapsed_ms, 2000U, 2400U);
        lv_obj_set_style_transform_scale(status, ui_motion_lerp_i32(256, 0, p), LV_PART_MAIN);
        set_opa(status, ui_motion_lerp_opa(255, 0, p));
    }
}

static void show_alert_asset(ui_left_screen_t *screen, ui_alert_kind_t alert, uint32_t elapsed_ms, lv_opa_t opacity)
{
    lv_obj_t *asset = screen->overload_scene;
    if(alert == UI_ALERT_LOW_BATTERY) asset = screen->low_scene;
    else if(alert == UI_ALERT_CRITICAL_BATTERY) asset = screen->critical_scene;
    set_hidden(asset, false);
    set_opa(asset, opacity);

    if(alert != UI_ALERT_LOW_BATTERY) {
        set_hidden(screen->scene_artifact_mask, false);
        set_opa(screen->scene_artifact_mask, LV_OPA_COVER);
    }

    if(opacity > 0U) {
        static const int8_t jitter[] = {0, 2, -1, 1, -2, 1, 0, -1};
        int offset = jitter[(elapsed_ms / 90U) % (sizeof(jitter) / sizeof(jitter[0]))];
        lv_obj_set_style_translate_x(asset, offset, LV_PART_MAIN);
    }
}

static uint32_t transition_duration(ui_transition_t transition)
{
    switch(transition) {
        case UI_TRANSITION_RECOGNIZE: return 2200U;
        case UI_TRANSITION_FOOD_OVERRIDE: return 1160U;
        case UI_TRANSITION_ADD: return 2730U;
        case UI_TRANSITION_CONNECTED:
        case UI_TRANSITION_TARE_SUCCESS:
        case UI_TRANSITION_RECORD_SUCCESS: return 3000U;
        case UI_TRANSITION_GUIDE_ENTER: return 1200U;
        case UI_TRANSITION_PAIRING_ENTER: return 1200U;
        case UI_TRANSITION_PAIRING_TIMEOUT: return 700U;
        case UI_TRANSITION_GUIDE_TIMEOUT: return 700U;
        case UI_TRANSITION_GUIDE_TO_FOOD: return 700U;
        case UI_TRANSITION_NUTRIENT: return 700U;
        case UI_TRANSITION_UNIT: return 1800U;
        case UI_TRANSITION_ALERT_ENTER: return 1300U;
        case UI_TRANSITION_ALERT_EXIT: return 1100U;
        case UI_TRANSITION_LOW_BATTERY: return 6500U;
        case UI_TRANSITION_CHARGING_ENTER: return 2300U;
        case UI_TRANSITION_CHARGING_EXIT: return 900U;
        case UI_TRANSITION_SLEEP: return 800U;
        case UI_TRANSITION_WAKE: return 1300U;
        case UI_TRANSITION_CRITICAL_SHUTDOWN: return 7750U;
        default: return 0U;
    }
}

static void render_recognize(
    ui_left_screen_t *screen,
    const ui_view_model_t *before,
    const ui_view_model_t *after,
    uint32_t elapsed_ms)
{
    const uint32_t duration = transition_duration(UI_TRANSITION_RECOGNIZE);
    if(elapsed_ms >= duration) {
        show_stable(screen, after);
        return;
    }

    hide_primary(screen);
    reset_geometry(screen);
    update_data(screen, elapsed_ms < 1480U ? before : after);
    show_data(screen, LV_OPA_COVER);

    if(elapsed_ms < 1640U && lv_gif_is_loaded(screen->recognize_gif)) {
        if(!screen->recognize_gif_started) {
            lv_gif_restart(screen->recognize_gif);
            lv_gif_set_loop_count(screen->recognize_gif, 1);
            screen->recognize_gif_started = true;
        }
        set_hidden(screen->recognize_gif, false);
        lv_gif_resume(screen->recognize_gif);
        return;
    }

    lv_gif_pause(screen->recognize_gif);
    if(elapsed_ms >= 1640U) {
        float p = ui_motion_segment(elapsed_ms, 1640U, 2100U);
        set_hidden(screen->food_group, false);
        lv_obj_set_style_transform_scale(screen->food_group,
            ui_motion_lerp_i32(8, 256, p), LV_PART_MAIN);
        set_opa(screen->food_group, ui_motion_lerp_opa(0, 255, p));
    }
}

static void render_add(
    ui_left_screen_t *screen,
    const ui_view_model_t *before,
    const ui_view_model_t *after,
    uint32_t elapsed_ms)
{
    if(elapsed_ms >= 2730U) {
        show_stable(screen, after);
        return;
    }

    hide_primary(screen);
    reset_geometry(screen);
    update_data(screen, elapsed_ms < 1380U ? before : after);
    show_data(screen, LV_OPA_COVER);

    if(elapsed_ms < 720U) {
        set_hidden(screen->food_group, false);
        if(elapsed_ms >= 400U) {
            float p = ui_motion_segment(elapsed_ms, 400U, 720U);
            lv_image_set_scale(screen->food_image, (uint32_t)ui_motion_lerp_i32(256, 0, p));
            set_opa(screen->food_group, ui_motion_lerp_opa(255, 0, p));
            set_opa(screen->weight_current, ui_motion_lerp_opa(255, 0, p));
            set_opa(screen->nutrient_current, ui_motion_lerp_opa(255, 0, p));
        }
        return;
    }

    if(elapsed_ms < 1380U) {
        float p = ui_motion_segment(elapsed_ms, 720U, 1380U);
        set_opa(screen->weight_slot, ui_motion_lerp_opa(90, 180, p));
        set_opa(screen->nutrient_slot, ui_motion_lerp_opa(90, 180, p));
        set_opa(screen->weight_current, LV_OPA_TRANSP);
        set_opa(screen->nutrient_current, LV_OPA_TRANSP);
        set_opa(screen->weight_total, ui_motion_lerp_opa(150, 0, p));
        set_opa(screen->nutrient_total, ui_motion_lerp_opa(150, 0, p));
        return;
    }

    if(elapsed_ms < 1950U) {
        float p = ui_motion_segment(elapsed_ms, 1380U, 1950U);
        set_opa(screen->weight_total, ui_motion_lerp_opa(0, 255, p));
        set_opa(screen->nutrient_total, ui_motion_lerp_opa(0, 255, p));
        set_opa(screen->weight_current, LV_OPA_TRANSP);
        set_opa(screen->nutrient_current, LV_OPA_TRANSP);
        return;
    }

    set_opa(screen->weight_current, elapsed_ms < 2200U ? LV_OPA_TRANSP : LV_OPA_COVER);
    set_opa(screen->nutrient_current, elapsed_ms < 2200U ? LV_OPA_TRANSP : LV_OPA_COVER);
    if(elapsed_ms >= 2200U) {
        float p = ui_motion_segment(elapsed_ms, 2200U, 2700U);
        set_hidden(screen->eyes, false);
        lv_image_set_scale(screen->eyes, (uint32_t)ui_motion_lerp_i32(0, 256, p));
        set_opa(screen->eyes, ui_motion_lerp_opa(0, 255, p));
    }
}

static void render_nutrient(
    ui_left_screen_t *screen,
    const ui_view_model_t *before,
    const ui_view_model_t *after,
    uint32_t elapsed_ms)
{
    const ui_view_model_t *shown = elapsed_ms < 400U ? before : after;
    show_stable(screen, shown);
    float p;
    if(elapsed_ms < 400U) {
        p = ui_motion_segment(elapsed_ms, 0U, 400U);
        lv_obj_set_style_transform_scale(screen->nutrient_slot, ui_motion_lerp_i32(256, 0, p), LV_PART_MAIN);
        set_opa(screen->nutrient_slot, ui_motion_lerp_opa(255, 0, p));
    }
    else if(elapsed_ms < 700U) {
        p = ui_motion_segment(elapsed_ms, 400U, 700U);
        lv_obj_set_style_transform_scale(screen->nutrient_slot, ui_motion_lerp_i32(0, 256, p), LV_PART_MAIN);
        set_opa(screen->nutrient_slot, ui_motion_lerp_opa(0, 255, p));
    }
}

static void render_unit(
    ui_left_screen_t *screen,
    const ui_view_model_t *before,
    const ui_view_model_t *after,
    uint32_t elapsed_ms)
{
    const ui_view_model_t *shown = elapsed_ms < 900U ? before : after;
    show_stable(screen, shown);
    set_hidden(screen->unit_mask_top, false);
    set_hidden(screen->unit_mask_bottom, false);

    if(elapsed_ms < 600U) {
        float p = ui_motion_segment(elapsed_ms, 0U, 600U);
        set_opa(screen->unit_mask_top, ui_motion_lerp_opa(0, 255, p));
        set_opa(screen->unit_mask_bottom, ui_motion_lerp_opa(0, 255, p));
    }
    else if(elapsed_ms < 1200U) {
        float p = ui_motion_segment(elapsed_ms, 600U, 1200U);
        int offset = elapsed_ms < 900U ? ui_motion_lerp_i32(0, -42, p * 2.0f) : ui_motion_lerp_i32(42, 0, (p - 0.5f) * 2.0f);
        lv_obj_set_style_translate_y(screen->weight_slot, offset, LV_PART_MAIN);
    }
    else if(elapsed_ms < 1600U) {
        float p = ui_motion_segment(elapsed_ms, 1200U, 1600U);
        set_opa(screen->unit_mask_top, ui_motion_lerp_opa(255, 0, p));
        set_opa(screen->unit_mask_bottom, ui_motion_lerp_opa(255, 0, p));
    }
    else if(elapsed_ms >= 1800U) {
        set_hidden(screen->unit_mask_top, true);
        set_hidden(screen->unit_mask_bottom, true);
    }
}

static void render_transition(
    ui_left_screen_t *screen,
    const ui_view_model_t *before,
    const ui_view_model_t *after,
    ui_transition_t transition,
    uint32_t elapsed_ms)
{
    switch(transition) {
        case UI_TRANSITION_RECOGNIZE:
        case UI_TRANSITION_FOOD_OVERRIDE:
            if(transition == UI_TRANSITION_RECOGNIZE) render_recognize(screen, before, after, elapsed_ms);
            else {
                hide_primary(screen);
                reset_geometry(screen);
                update_data(screen, elapsed_ms < 540U ? before : after);
                show_data(screen, LV_OPA_COVER);
                if(elapsed_ms < 540U) {
                    float p = ui_motion_segment(elapsed_ms, 0U, 540U);
                    set_hidden(screen->food_group, false);
                    lv_image_set_scale(screen->food_image, (uint32_t)ui_motion_lerp_i32(256, 0, p));
                    set_opa(screen->food_group, ui_motion_lerp_opa(255, 0, p));
                }
                else if(elapsed_ms < 1160U) {
                    float p = ui_motion_segment(elapsed_ms, 540U, 800U);
                    set_hidden(screen->food_group, false);
                    lv_image_set_scale(screen->food_image, (uint32_t)ui_motion_lerp_i32(0, 256, p));
                    set_opa(screen->food_group, ui_motion_lerp_opa(0, 255, p));
                }
                else show_stable(screen, after);
            }
            break;
        case UI_TRANSITION_ADD:
            render_add(screen, before, after, elapsed_ms);
            break;
        case UI_TRANSITION_CONNECTED:
        case UI_TRANSITION_TARE_SUCCESS:
        case UI_TRANSITION_RECORD_SUCCESS: {
            ui_status_kind_t status = UI_STATUS_CONNECTED;
            if(transition == UI_TRANSITION_TARE_SUCCESS) status = UI_STATUS_TARE_DONE;
            else if(transition == UI_TRANSITION_RECORD_SUCCESS) status = UI_STATUS_RECORDED;
            if(elapsed_ms < 300U) {
                show_stable(screen, before);
                set_opa(screen->page, ui_motion_lerp_opa(255, 0, ui_motion_segment(elapsed_ms, 0U, 300U)));
            }
            else if(elapsed_ms < 2400U) show_status_frame(screen, status, elapsed_ms);
            else if(elapsed_ms < 3000U) {
                show_stable(screen, after);
                float p = ui_motion_segment(elapsed_ms, 2400U, 3000U);
                set_opa(screen->page, ui_motion_lerp_opa(0, 255, p));
                if(after->primary == UI_PRIMARY_EYES) {
                    lv_image_set_scale(screen->eyes, (uint32_t)ui_motion_lerp_i32(0, 256, p));
                }
            }
            else show_stable(screen, after);
            break;
        }
        case UI_TRANSITION_GUIDE_ENTER:
            if(elapsed_ms < 800U) {
                show_stable(screen, before);
                set_opa(screen->page, ui_motion_lerp_opa(255, 35, ui_motion_segment(elapsed_ms, 0U, 800U)));
            }
            else {
                hide_primary(screen);
                show_data(screen, LV_OPA_TRANSP);
                set_hidden(screen->guide_scene, false);
                set_opa(screen->guide_scene, ui_motion_lerp_opa(0, 255, ui_motion_segment(elapsed_ms, 800U, 1200U)));
            }
            break;
        case UI_TRANSITION_PAIRING_ENTER:
            if(elapsed_ms < 800U) {
                show_stable(screen, before);
                set_opa(screen->page, ui_motion_lerp_opa(255, 35, ui_motion_segment(elapsed_ms, 0U, 800U)));
            }
            else {
                hide_primary(screen);
                show_data(screen, LV_OPA_TRANSP);
                set_hidden(screen->pairing_scene, false);
                set_opa(screen->pairing_scene, ui_motion_lerp_opa(0, 255, ui_motion_segment(elapsed_ms, 800U, 1200U)));
            }
            break;
        case UI_TRANSITION_PAIRING_TIMEOUT:
            if(elapsed_ms < 700U) {
                hide_primary(screen);
                show_data(screen, LV_OPA_TRANSP);
                set_hidden(screen->pairing_scene, false);
                set_opa(screen->pairing_scene, ui_motion_lerp_opa(255, 0, ui_motion_segment(elapsed_ms, 0U, 700U)));
            }
            else show_stable(screen, after);
            break;
        case UI_TRANSITION_GUIDE_TIMEOUT:
            if(elapsed_ms < 700U) {
                hide_primary(screen);
                show_data(screen, LV_OPA_TRANSP);
                set_hidden(screen->guide_scene, false);
                set_opa(screen->guide_scene, ui_motion_lerp_opa(255, 0, ui_motion_segment(elapsed_ms, 0U, 700U)));
            }
            else show_stable(screen, after);
            break;
        case UI_TRANSITION_GUIDE_TO_FOOD:
            if(elapsed_ms < 300U) {
                hide_primary(screen);
                show_data(screen, LV_OPA_TRANSP);
                set_hidden(screen->guide_scene, false);
                set_opa(screen->guide_scene, ui_motion_lerp_opa(255, 0, ui_motion_segment(elapsed_ms, 0U, 300U)));
            }
            else {
                show_stable(screen, after);
                float p = ui_motion_segment(elapsed_ms, 300U, 700U);
                lv_image_set_scale(screen->food_image, (uint32_t)ui_motion_lerp_i32(0, 256, p));
                set_opa(screen->food_group, ui_motion_lerp_opa(0, 255, p));
                set_opa(screen->page, ui_motion_lerp_opa(0, 255, p));
            }
            break;
        case UI_TRANSITION_NUTRIENT:
            render_nutrient(screen, before, after, elapsed_ms);
            break;
        case UI_TRANSITION_UNIT:
            render_unit(screen, before, after, elapsed_ms);
            break;
        case UI_TRANSITION_ALERT_ENTER:
            if(elapsed_ms < 600U) {
                show_stable(screen, before);
                set_opa(screen->page, ui_motion_lerp_opa(255, 0, ui_motion_segment(elapsed_ms, 0U, 600U)));
            }
            else {
                hide_primary(screen);
                show_data(screen, LV_OPA_TRANSP);
                lv_opa_t opa = ui_motion_lerp_opa(0, 255, ui_motion_segment(elapsed_ms, 1000U, 1300U));
                show_alert_asset(screen, after->alert, elapsed_ms, opa);
            }
            break;
        case UI_TRANSITION_ALERT_EXIT:
            if(elapsed_ms < 700U) {
                hide_primary(screen);
                show_data(screen, LV_OPA_TRANSP);
                show_alert_asset(screen, before->alert, elapsed_ms, ui_motion_lerp_opa(255, 0, ui_motion_segment(elapsed_ms, 300U, 700U)));
            }
            else {
                show_stable(screen, after);
                set_opa(screen->page, ui_motion_lerp_opa(0, 255, ui_motion_segment(elapsed_ms, 700U, 1100U)));
            }
            break;
        case UI_TRANSITION_LOW_BATTERY:
            if(elapsed_ms < 800U) {
                show_stable(screen, before);
                set_opa(screen->page, ui_motion_lerp_opa(255, 0, ui_motion_segment(elapsed_ms, 0U, 800U)));
            }
            else if(elapsed_ms < 5900U) {
                hide_primary(screen);
                show_data(screen, LV_OPA_TRANSP);
                lv_opa_t opa = elapsed_ms < 1100U
                    ? ui_motion_lerp_opa(0, 255, ui_motion_segment(elapsed_ms, 800U, 1100U))
                    : ui_motion_lerp_opa(255, 0, ui_motion_segment(elapsed_ms, 5100U, 5900U));
                show_alert_asset(screen, UI_ALERT_LOW_BATTERY, elapsed_ms, opa);
            }
            else {
                show_stable(screen, after);
                set_opa(screen->page, ui_motion_lerp_opa(0, 255, ui_motion_segment(elapsed_ms, 5900U, 6500U)));
            }
            break;
        case UI_TRANSITION_CHARGING_ENTER:
            if(elapsed_ms < 500U) {
                show_stable(screen, before);
                set_opa(screen->page, ui_motion_lerp_opa(255, 0, ui_motion_segment(elapsed_ms, 0U, 500U)));
            }
            else {
                hide_primary(screen);
                show_data(screen, LV_OPA_TRANSP);
                if(elapsed_ms >= 1500U) {
                    set_hidden(screen->charging, false);
                    set_opa(screen->charging, ui_motion_lerp_opa(0, 255, ui_motion_segment(elapsed_ms, 1500U, 2300U)));
                }
            }
            break;
        case UI_TRANSITION_CHARGING_EXIT:
            if(elapsed_ms < 500U) {
                show_stable(screen, before);
                set_opa(screen->charging, ui_motion_lerp_opa(255, 0, ui_motion_segment(elapsed_ms, 0U, 500U)));
            }
            else {
                show_stable(screen, after);
                set_opa(screen->page, ui_motion_lerp_opa(0, 255, ui_motion_segment(elapsed_ms, 500U, 900U)));
            }
            break;
        case UI_TRANSITION_SLEEP:
            show_stable(screen, before);
            set_opa(screen->root, ui_motion_lerp_opa(255, 0, ui_motion_segment(elapsed_ms, 0U, 800U)));
            break;
        case UI_TRANSITION_WAKE:
            show_stable(screen, after);
            set_opa(screen->root, ui_motion_lerp_opa(0, 255, ui_motion_segment(elapsed_ms, 0U, 1100U)));
            if(after->primary == UI_PRIMARY_EYES) {
                lv_image_set_scale(screen->eyes, (uint32_t)ui_motion_lerp_i32(0, 256, ui_motion_segment(elapsed_ms, 150U, 800U)));
            }
            break;
        case UI_TRANSITION_CRITICAL_SHUTDOWN:
            if(elapsed_ms < 900U) {
                show_stable(screen, before);
                set_opa(screen->page, ui_motion_lerp_opa(255, 0, ui_motion_segment(elapsed_ms, 0U, 900U)));
            }
            else if(elapsed_ms < 7000U) {
                hide_primary(screen);
                show_data(screen, LV_OPA_TRANSP);
                show_alert_asset(screen, UI_ALERT_CRITICAL_BATTERY, elapsed_ms,
                    ui_motion_lerp_opa(0, 255, ui_motion_segment(elapsed_ms, 1200U, 1600U)));
            }
            else {
                hide_primary(screen);
                show_data(screen, LV_OPA_TRANSP);
                show_alert_asset(screen, UI_ALERT_CRITICAL_BATTERY, elapsed_ms,
                    ui_motion_lerp_opa(255, 0, ui_motion_segment(elapsed_ms, 7000U, 7750U)));
            }
            break;
        case UI_TRANSITION_FINISH_UPLOAD:
            if(elapsed_ms < 700U) {
                show_stable(screen, before);
                set_opa(screen->page, ui_motion_lerp_opa(255, 0, ui_motion_segment(elapsed_ms, 0U, 700U)));
            }
            else {
                hide_primary(screen);
                show_data(screen, LV_OPA_TRANSP);
                set_hidden(screen->upload_scene, false);
                set_opa(screen->upload_scene, ui_motion_lerp_opa(0, 255, ui_motion_segment(elapsed_ms, 700U, 1300U)));
            }
            break;
        default:
            show_stable(screen, after);
            break;
    }
}

static lv_obj_t *create_scene_image(lv_obj_t *parent, const char *path)
{
    lv_obj_t *image = lv_image_create(parent);
    lv_obj_remove_style_all(image);
    lv_image_set_src(image, path);
    lv_obj_set_pos(image, 0, 0);
    return image;
}

static void create_corner_bars(lv_obj_t *parent)
{
    const int edge = 22;
    const int stroke = 4;
    const int right = 133 - edge;
    const int bottom = 133 - edge;
    make_rect(parent, 0, 0, edge, stroke, UI_COLOR_WEIGHT, 2);
    make_rect(parent, 0, 0, stroke, 20, UI_COLOR_WEIGHT, 2);
    make_rect(parent, right, 0, edge, stroke, UI_COLOR_WEIGHT, 2);
    make_rect(parent, 129, 0, stroke, 20, UI_COLOR_WEIGHT, 2);
    make_rect(parent, 0, 129, edge, stroke, UI_COLOR_WEIGHT, 2);
    make_rect(parent, 0, bottom, stroke, 22, UI_COLOR_WEIGHT, 2);
    make_rect(parent, right, 129, edge, stroke, UI_COLOR_WEIGHT, 2);
    make_rect(parent, 129, bottom, stroke, 22, UI_COLOR_WEIGHT, 2);
}

static void create_data_slots(ui_left_screen_t *screen)
{
    screen->weight_slot = make_group(screen->page);
    screen->nutrient_slot = make_group(screen->page);

    make_label(screen->weight_slot, 225, 0, 120, 38, "总计", &font_ui_27, UI_COLOR_WEIGHT, LV_TEXT_ALIGN_LEFT);
    screen->weight_total = make_label(screen->weight_slot, 338, 7, 70, 31, "0", &font_digits_30, UI_COLOR_WHITE, LV_TEXT_ALIGN_RIGHT);
    make_rect(screen->weight_slot, 225, 44, 183, 1, UI_COLOR_WHITE, 1);
    lv_obj_set_style_bg_opa(lv_obj_get_child(screen->weight_slot, 2), 102, LV_PART_MAIN);
    make_label(screen->weight_slot, 225, 58, 120, 38, "重量", &font_ui_27, UI_COLOR_WEIGHT, LV_TEXT_ALIGN_LEFT);
    screen->weight_unit = make_label(screen->weight_slot, 338, 61, 70, 35, "g", &font_units_24, UI_COLOR_WHITE, LV_TEXT_ALIGN_RIGHT);
    screen->weight_current = make_label(screen->weight_slot, 225, 124, 191, 76, "0", &font_digits_84, UI_COLOR_WHITE, LV_TEXT_ALIGN_RIGHT);

    make_label(screen->nutrient_slot, 466, 0, 110, 38, "总计", &font_ui_27, UI_COLOR_CAL_DARK, LV_TEXT_ALIGN_LEFT);
    screen->nutrient_total = make_label(screen->nutrient_slot, 574, 7, 70, 31, "0", &font_digits_30, UI_COLOR_WHITE, LV_TEXT_ALIGN_RIGHT);
    make_rect(screen->nutrient_slot, 466, 44, 178, 1, UI_COLOR_WHITE, 1);
    lv_obj_set_style_bg_opa(lv_obj_get_child(screen->nutrient_slot, 2), 102, LV_PART_MAIN);
    screen->nutrient_title = make_label(screen->nutrient_slot, 466, 58, 132, 38, "热量", &font_ui_27, UI_COLOR_CAL_DARK, LV_TEXT_ALIGN_LEFT);
    screen->nutrient_unit = make_label(screen->nutrient_slot, 574, 61, 70, 35, "kcal", &font_units_24, UI_COLOR_WHITE, LV_TEXT_ALIGN_RIGHT);
    screen->nutrient_current = make_label(screen->nutrient_slot, 466, 124, 183, 76, "0", &font_digits_84, UI_COLOR_WHITE, LV_TEXT_ALIGN_RIGHT);

    screen->divider = make_rect(screen->page, 436, 2, 3, 193, UI_COLOR_WHITE, 2);
}

static void create_picker(ui_left_screen_t *screen)
{
    screen->picker = make_group(screen->root);
    static const char *names[] = {"苹果", "香蕉", "鸡胸肉", "牛奶"};
    for(int i = 0; i < 4; ++i) {
        int x = i * 162;
        if(i > 0) {
            lv_obj_t *line = make_rect(screen->picker, x, 26, 1, 148, UI_COLOR_WHITE, 1);
            lv_obj_set_style_bg_opa(line, 58, LV_PART_MAIN);
        }
        make_label(screen->picker, x + 8, 76, 146, 42, names[i], &font_ui_27,
            i == 0 ? UI_COLOR_WEIGHT : UI_COLOR_WHITE, LV_TEXT_ALIGN_CENTER);
        make_rect(screen->picker, x + 57, 132, 48, 3,
            i == 0 ? UI_COLOR_WEIGHT : UI_COLOR_MUTED, 2);
    }
}

static void create_charging(ui_left_screen_t *screen)
{
    screen->charging = make_group(screen->root);
    screen->charge_value = make_label(screen->charging, 204, 35, 190, 100, "16", &font_digits_84,
        UI_COLOR_WHITE, LV_TEXT_ALIGN_RIGHT);
    screen->charge_percent = make_label(screen->charging, 400, 64, 50, 50, "%", &font_ui_34,
        UI_COLOR_WHITE, LV_TEXT_ALIGN_LEFT);
    screen->charge_time = make_label(screen->charging, 172, 135, 330, 38, "预计 60 分钟充满", &font_ui_24,
        UI_COLOR_CHARGE, LV_TEXT_ALIGN_CENTER);

    for(int i = 0; i < 8; ++i) {
        double angle = (double)i * 3.14159265358979323846 / 4.0;
        int x = 536 + (int)(cos(angle) * 22.0);
        int y = 100 + (int)(sin(angle) * 22.0);
        screen->charge_spinner[i] = make_rect(screen->charging, x, y, 6, 6, UI_COLOR_CHARGE, 3);
        lv_obj_set_style_opa(screen->charge_spinner[i], (lv_opa_t)(55 + i * 25), LV_PART_MAIN);
    }
}

ui_left_screen_t *ui_left_screen_create(lv_obj_t *parent)
{
    ui_left_screen_t *screen = lv_malloc_zeroed(sizeof(*screen));
    if(!screen) return NULL;

    screen->root = make_group(parent);
    lv_obj_add_flag(screen->root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(screen->root, UI_COLOR_BLACK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen->root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(screen->root, true, LV_PART_MAIN);

    screen->page = make_group(screen->root);
    create_data_slots(screen);

    screen->eyes = lv_image_create(screen->page);
    lv_obj_remove_style_all(screen->eyes);
    lv_image_set_src(screen->eyes, &img_eyes);
    lv_obj_set_pos(screen->eyes, 3, 40);

    screen->recognize_gif = lv_gif_create(screen->page);
    lv_obj_remove_style_all(screen->recognize_gif);
    lv_gif_set_color_format(screen->recognize_gif, LV_COLOR_FORMAT_RGB565);
    lv_gif_set_src(screen->recognize_gif, "A:assets/fixed/recognize_left.gif");
    lv_gif_set_auto_pause_invisible(screen->recognize_gif, true);
    lv_obj_set_pos(screen->recognize_gif, 0, 0);

    screen->food_group = make_group(screen->page);
    lv_obj_set_style_transform_pivot_x(screen->food_group, 66, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(screen->food_group, 100, LV_PART_MAIN);
    create_corner_bars(screen->food_group);
    screen->food_image = lv_image_create(screen->food_group);
    lv_obj_remove_style_all(screen->food_image);
    lv_image_set_src(screen->food_image, &img_apple);
    lv_obj_set_pos(screen->food_image, 22, 22);
    screen->food_name = make_label(screen->food_group, 0, 151, 133, 45, "苹果", &font_ui_36,
        UI_COLOR_WHITE, LV_TEXT_ALIGN_CENTER);

    screen->guide_scene = create_scene_image(screen->root, "A:assets/images/scene_guide.png");
    screen->pairing_scene = create_scene_image(screen->root, "A:assets/images/scene_pairing.png");
    screen->overload_scene = create_scene_image(screen->root, "A:assets/images/scene_overload.png");
    screen->low_scene = create_scene_image(screen->root, "A:assets/images/scene_low_battery.png");
    screen->critical_scene = create_scene_image(screen->root, "A:assets/images/scene_critical_battery.png");
    screen->status_connected = create_scene_image(screen->root, "A:assets/images/scene_connected.png");
    screen->status_tare = create_scene_image(screen->root, "A:assets/images/scene_tare_done.png");
    screen->status_recorded = create_scene_image(screen->root, "A:assets/images/scene_recorded.png");
    screen->upload_scene = create_scene_image(screen->root, "A:assets/images/scene_upload.png");
    screen->scene_artifact_mask = make_rect(screen->root, 42, 160, 55, 40, UI_COLOR_BLACK, 0);

    create_picker(screen);
    create_charging(screen);

    screen->unit_mask_top = make_rect(screen->root, 220, 42, 192, 28, UI_COLOR_BLACK, 0);
    screen->unit_mask_bottom = make_rect(screen->root, 220, 164, 192, 36, UI_COLOR_BLACK, 0);

    ui_view_model_t initial = {0};
    initial.primary = UI_PRIMARY_EYES;
    initial.nutrient = UI_NUTRIENT_CALORIES;
    initial.unit = UI_UNIT_G;
    initial.food_name = "苹果";
    show_stable(screen, &initial);
    return screen;
}

lv_obj_t *ui_left_screen_touch_target(ui_left_screen_t *screen)
{
    return screen ? screen->root : NULL;
}

void ui_left_screen_render(
    ui_left_screen_t *screen,
    const ui_view_model_t *before,
    const ui_view_model_t *after,
    ui_transition_t transition,
    uint32_t elapsed_ms)
{
    if(!screen || !before || !after) return;
    if(transition != UI_TRANSITION_RECOGNIZE && screen->recognize_gif_started) {
        lv_gif_pause(screen->recognize_gif);
        screen->recognize_gif_started = false;
    }
    if(transition == UI_TRANSITION_NONE) {
        show_stable(screen, after);
        if(after->primary == UI_PRIMARY_ALERT) {
            show_alert_asset(screen, after->alert, elapsed_ms, LV_OPA_COVER);
        }
        if(after->primary == UI_PRIMARY_CHARGING && after->battery_percent < 100) {
            unsigned head = (elapsed_ms / 120U) % 8U;
            for(unsigned i = 0; i < 8U; ++i) {
                unsigned distance = (i + 8U - head) % 8U;
                lv_obj_set_style_opa(screen->charge_spinner[i], (lv_opa_t)(255U - distance * 24U), LV_PART_MAIN);
            }
        }
        return;
    }
    render_transition(screen, before, after, transition, elapsed_ms);
}
