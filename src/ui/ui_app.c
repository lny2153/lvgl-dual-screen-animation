#include "ui_app.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "domain/transition_rules.h"
#include "left_screen.h"
#include "right_screen.h"
#include "tokens.h"
#include "ui_view_model.h"

typedef enum {
    ACK_NONE = 0,
    ACK_ADD,
    ACK_TARE,
    ACK_FINISH
} pending_ack_t;

typedef enum {
    ACTION_LOAD = 1,
    ACTION_RECOGNIZE,
    ACTION_WEIGHT_PLUS,
    ACTION_ADD,
    ACTION_TARE,
    ACTION_NUTRIENT,
    ACTION_UNIT,
    ACTION_BLE_LINK,
    ACTION_APP_READY,
    ACTION_OVERLOAD,
    ACTION_LOW_BATTERY,
    ACTION_CRITICAL_BATTERY,
    ACTION_CHARGE,
    ACTION_SLEEP_WAKE,
    ACTION_PHONE_RESPONSE,
    ACTION_FINISH_DEMO
} simulator_action_t;

typedef struct {
    tr_state_t domain;
    ui_view_model_t visible;
    ui_transition_snapshot_t transition;
    bool transition_active;
    bool transition_finishes_domain;
    uint32_t transition_scene_token;

    ui_left_screen_t *left;
    right_screen_t *right;
    lv_obj_t *left_frame;
    lv_obj_t *right_frame;
    lv_obj_t *boot_left;
    lv_obj_t *boot_right;
    bool boot_playing;
    bool boot_assets_ready;
    uint32_t boot_started_ms;

    lv_obj_t *state_label;
    lv_obj_t *hint_label;
    lv_obj_t *phone_button_label;

    double total_weight_g;
    double current_weight_g;
    double total_nutrients[UI_NUTRIENT_COUNT];
    double current_nutrients[UI_NUTRIENT_COUNT];
    double nutrient_goals[UI_NUTRIENT_COUNT];
    ui_nutrient_t nutrient;
    ui_weight_unit_t unit;
    const char *food_name;
    int battery_percent;
    int charge_minutes;

    pending_ack_t pending_ack;
    uint32_t pending_command_token;
    uint32_t pending_ack_due_ms;
    bool phone_responds;

    bool right_pressed;
    bool finish_hold_started;
    bool finish_request_sent;
    uint32_t right_pressed_ms;

    bool left_pressed;
    uint32_t left_pressed_ms;
    lv_point_t left_start;

    bool pairing_prompt_active;
    uint32_t pairing_deadline_ms;
    uint32_t guide_deadline_ms;
    uint32_t hint_clear_ms;
} ui_app_t;

static ui_app_t g_app;

static const double APPLE_PER_GRAM[UI_NUTRIENT_COUNT] = {
    0.52, 0.003, 0.002, 0.138, 0.01
};

static lv_obj_t *make_stage_rect(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t color, int radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, radius, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *make_stage_label(
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
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_label_set_text(label, text);
    return label;
}

static void set_hint(const char *text, uint32_t duration_ms)
{
    lv_label_set_text(g_app.hint_label, text);
    g_app.hint_clear_ms = lv_tick_get() + duration_ms;
}

static const char *left_scene_name(const tr_state_t *state)
{
    if(state->operation == TR_OP_FINISH_WAIT_ACK) return "上传中";
    switch(state->left_scene) {
        case TR_LEFT_BOOT: return "开机";
        case TR_LEFT_WAKE: return "唤醒";
        case TR_LEFT_EYE: return "待机识别";
        case TR_LEFT_FOOD: return "食材已锁定";
        case TR_LEFT_CANDIDATES: return "候选修正";
        case TR_LEFT_FIRST_WEIGHT_GUIDE: return "首次放置提示";
        case TR_LEFT_ADD_FEEDBACK: return "添加反馈";
        case TR_LEFT_BLE_SUCCESS: return "连接成功";
        case TR_LEFT_TARE_SUCCESS: return "去皮成功";
        case TR_LEFT_FINISH_SUCCESS: return "记录完成";
        case TR_LEFT_OVERLOAD: return "超重保护";
        case TR_LEFT_LOW_BATTERY: return "低电量提示";
        case TR_LEFT_CRITICAL_BATTERY: return "极低电量";
        case TR_LEFT_CHARGING: return "充电";
        case TR_LEFT_FADE_TO_BLACK: return "渐隐";
        case TR_LEFT_OFF:
        default: return "黑屏";
    }
}

static void update_stage_status(void)
{
    const char *connection = g_app.domain.app_session_ready
        ? "APP 已就绪"
        : (g_app.domain.ble_link_up ? "蓝牙已连，等待 APP" : "未连接");
    lv_label_set_text_fmt(
        g_app.state_label,
        "当前：%s    本餐：%u 项    未提交：%.0f g    %s",
        left_scene_name(&g_app.domain),
        (unsigned)g_app.domain.meal_item_count,
        g_app.current_weight_g,
        connection);
}

static void recalculate_current_nutrients(void)
{
    for(unsigned i = 0; i < UI_NUTRIENT_COUNT; ++i) {
        g_app.current_nutrients[i] = g_app.domain.food_active
            ? g_app.current_weight_g * APPLE_PER_GRAM[i]
            : 0.0;
    }
}

static ui_primary_mode_t map_left_primary(const tr_state_t *state)
{
    if(state->operation == TR_OP_FINISH_WAIT_ACK) return UI_PRIMARY_UPLOAD;
    switch(state->left_scene) {
        case TR_LEFT_FOOD: return UI_PRIMARY_FOOD;
        case TR_LEFT_CANDIDATES: return UI_PRIMARY_PICKER;
        case TR_LEFT_FIRST_WEIGHT_GUIDE: return UI_PRIMARY_GUIDE;
        case TR_LEFT_OVERLOAD:
        case TR_LEFT_LOW_BATTERY:
        case TR_LEFT_CRITICAL_BATTERY: return UI_PRIMARY_ALERT;
        case TR_LEFT_BLE_SUCCESS:
        case TR_LEFT_TARE_SUCCESS:
        case TR_LEFT_FINISH_SUCCESS: return UI_PRIMARY_STATUS;
        case TR_LEFT_CHARGING: return UI_PRIMARY_CHARGING;
        case TR_LEFT_OFF:
        case TR_LEFT_BOOT:
        case TR_LEFT_FADE_TO_BLACK: return UI_PRIMARY_OFF;
        case TR_LEFT_WAKE:
        case TR_LEFT_ADD_FEEDBACK:
        case TR_LEFT_EYE:
        default: return UI_PRIMARY_EYES;
    }
}

static ui_right_mode_t map_right_mode(const tr_state_t *state)
{
    if(state->operation == TR_OP_FINISH_WAIT_ACK) return UI_RIGHT_UPLOAD;
    switch(state->right_scene) {
        case TR_RIGHT_PLUS_WITH_GOAL:
        case TR_RIGHT_ADD_FEEDBACK:
        case TR_RIGHT_BLE_SUCCESS:
        case TR_RIGHT_FINISH_SUCCESS:
            return UI_RIGHT_PLUS_RING;
        case TR_RIGHT_FINISH_PROGRESS:
            return UI_RIGHT_FINISH_HOLD;
        case TR_RIGHT_OVERLOAD:
        case TR_RIGHT_CRITICAL_BATTERY:
            return UI_RIGHT_SAFETY;
        case TR_RIGHT_CHARGING:
            return UI_RIGHT_CHARGE;
        case TR_RIGHT_OFF:
        case TR_RIGHT_BOOT:
        case TR_RIGHT_FADE_TO_BLACK:
            return UI_RIGHT_OFF;
        case TR_RIGHT_WAKE:
        case TR_RIGHT_PLUS:
        default:
            return state->app_session_ready ? UI_RIGHT_PLUS_RING : UI_RIGHT_PLUS_ONLY;
    }
}

static ui_view_model_t view_from_state(const tr_state_t *state)
{
    ui_view_model_t view;
    memset(&view, 0, sizeof(view));
    view.primary = map_left_primary(state);
    view.right = map_right_mode(state);
    view.nutrient = g_app.nutrient;
    view.unit = g_app.unit;
    view.total_weight_g = g_app.total_weight_g;
    view.current_weight_g = g_app.current_weight_g;
    view.total_nutrient = g_app.total_nutrients[g_app.nutrient];
    view.current_nutrient = state->food_active ? g_app.current_nutrients[g_app.nutrient] : 0.0;
    view.goal_ratio = g_app.nutrient_goals[g_app.nutrient] > 0.0
        ? g_app.total_nutrients[g_app.nutrient] / g_app.nutrient_goals[g_app.nutrient]
        : 0.0;
    if(view.goal_ratio < 0.0) view.goal_ratio = 0.0;
    if(view.goal_ratio > 1.0) view.goal_ratio = 1.0;
    view.app_ready = state->app_session_ready && state->goal_valid;
    view.has_food = state->food_active;
    view.has_committed_items = state->meal_item_count > 0U;
    view.food_name = g_app.food_name;
    view.battery_percent = g_app.battery_percent;
    view.charge_minutes = g_app.charge_minutes;
    view.estimated_charge_time_valid = g_app.charge_minutes > 0;

    if(state->left_scene == TR_LEFT_TARE_SUCCESS) view.status = UI_STATUS_TARE_DONE;
    else if(state->left_scene == TR_LEFT_FINISH_SUCCESS) view.status = UI_STATUS_RECORDED;
    else view.status = UI_STATUS_CONNECTED;

    if(state->left_scene == TR_LEFT_LOW_BATTERY) view.alert = UI_ALERT_LOW_BATTERY;
    else if(state->left_scene == TR_LEFT_CRITICAL_BATTERY) view.alert = UI_ALERT_CRITICAL_BATTERY;
    else view.alert = UI_ALERT_OVERLOAD;
    return view;
}

static bool operation_finishes_with_animation(tr_operation_t operation)
{
    switch(operation) {
        case TR_OP_BOOT_ANIMATION:
        case TR_OP_WAKE_ANIMATION:
        case TR_OP_SLEEP_FADE:
        case TR_OP_POWER_OFF_FADE:
        case TR_OP_BLE_FEEDBACK:
        case TR_OP_ADD_FEEDBACK:
        case TR_OP_TARE_FEEDBACK:
        case TR_OP_FINISH_FEEDBACK:
        case TR_OP_LOW_BATTERY_FEEDBACK:
            return true;
        default:
            return false;
    }
}

static ui_view_model_t preview_after_animation(const tr_state_t *state)
{
    tr_state_t preview = *state;
    if(operation_finishes_with_animation(preview.operation)) {
        tr_event_t done = tr_event_make(TR_EVENT_ANIMATION_FINISHED);
        done.token = preview.scene_token;
        (void)tr_dispatch(&preview, &done);
    }
    ui_view_model_t view = view_from_state(&preview);
    if(state->operation == TR_OP_FINISH_FEEDBACK) {
        view.total_weight_g = 0.0;
        view.total_nutrient = 0.0;
        view.current_weight_g = 0.0;
        view.current_nutrient = 0.0;
        view.goal_ratio = 0.0;
    }
    return view;
}

static uint32_t ui_transition_duration(ui_transition_t transition)
{
    switch(transition) {
        case UI_TRANSITION_BOOT: return 9000U;
        case UI_TRANSITION_WAKE: return 1300U;
        case UI_TRANSITION_SLEEP: return 800U;
        case UI_TRANSITION_RECOGNIZE: return 2200U;
        case UI_TRANSITION_FOOD_OVERRIDE: return 1160U;
        case UI_TRANSITION_ADD: return 2730U;
        case UI_TRANSITION_CONNECTED:
        case UI_TRANSITION_TARE_SUCCESS:
        case UI_TRANSITION_RECORD_SUCCESS: return 3000U;
        case UI_TRANSITION_PAIRING_ENTER: return 1200U;
        case UI_TRANSITION_PAIRING_TIMEOUT: return 700U;
        case UI_TRANSITION_GUIDE_ENTER: return 1200U;
        case UI_TRANSITION_GUIDE_TIMEOUT:
        case UI_TRANSITION_GUIDE_TO_FOOD:
        case UI_TRANSITION_NUTRIENT: return 700U;
        case UI_TRANSITION_UNIT: return 1800U;
        case UI_TRANSITION_ALERT_ENTER: return 1300U;
        case UI_TRANSITION_ALERT_EXIT: return 1100U;
        case UI_TRANSITION_LOW_BATTERY: return 6500U;
        case UI_TRANSITION_CHARGING_ENTER: return 2300U;
        case UI_TRANSITION_CHARGING_EXIT: return 900U;
        case UI_TRANSITION_FINISH_UPLOAD: return 1800U;
        case UI_TRANSITION_CRITICAL_SHUTDOWN: return 7750U;
        default: return 0U;
    }
}

static void start_transition(
    ui_transition_t id,
    const ui_view_model_t *before,
    const ui_view_model_t *after,
    bool finish_domain)
{
    g_app.transition.id = id;
    g_app.transition.t0_ms = lv_tick_get();
    g_app.transition.duration_ms = ui_transition_duration(id);
    g_app.transition.revision++;
    g_app.transition.before = *before;
    g_app.transition.after = *after;
    g_app.transition_active = true;
    g_app.transition_finishes_domain = finish_domain;
    g_app.transition_scene_token = g_app.domain.scene_token;
    update_stage_status();
}

static void begin_pairing_prompt(void)
{
    ui_view_model_t before = view_from_state(&g_app.domain);
    ui_view_model_t after = before;
    after.primary = UI_PRIMARY_PAIRING;
    g_app.pairing_prompt_active = true;
    start_transition(UI_TRANSITION_PAIRING_ENTER, &before, &after, false);
    g_app.pairing_deadline_ms = g_app.transition.t0_ms + 5000U;
}

static void complete_transition(void)
{
    ui_transition_t completed = g_app.transition.id;
    if(g_app.transition_finishes_domain) {
        tr_event_t done = tr_event_make(TR_EVENT_ANIMATION_FINISHED);
        done.token = g_app.transition_scene_token;
        (void)tr_dispatch(&g_app.domain, &done);
    }

    if(completed == UI_TRANSITION_RECORD_SUCCESS) {
        g_app.total_weight_g = 0.0;
        g_app.current_weight_g = 0.0;
        memset(g_app.total_nutrients, 0, sizeof(g_app.total_nutrients));
        memset(g_app.current_nutrients, 0, sizeof(g_app.current_nutrients));
    }

    g_app.transition_active = false;
    g_app.visible = view_from_state(&g_app.domain);
    update_stage_status();

    if(completed == UI_TRANSITION_BOOT && !g_app.domain.app_session_ready) {
        begin_pairing_prompt();
    }
}

static void start_feedback_for_operation(const ui_view_model_t *before)
{
    ui_transition_t transition = UI_TRANSITION_NONE;
    switch(g_app.domain.operation) {
        case TR_OP_BLE_FEEDBACK: transition = UI_TRANSITION_CONNECTED; break;
        case TR_OP_ADD_FEEDBACK: transition = UI_TRANSITION_ADD; break;
        case TR_OP_TARE_FEEDBACK: transition = UI_TRANSITION_TARE_SUCCESS; break;
        case TR_OP_FINISH_FEEDBACK: transition = UI_TRANSITION_RECORD_SUCCESS; break;
        case TR_OP_LOW_BATTERY_FEEDBACK: transition = UI_TRANSITION_LOW_BATTERY; break;
        case TR_OP_SLEEP_FADE:
        case TR_OP_POWER_OFF_FADE: transition = UI_TRANSITION_SLEEP; break;
        case TR_OP_WAKE_ANIMATION: transition = UI_TRANSITION_WAKE; break;
        default: break;
    }
    if(transition != UI_TRANSITION_NONE) {
        ui_view_model_t after = preview_after_animation(&g_app.domain);
        start_transition(transition, before, &after, true);
    }
}

static void schedule_ack(pending_ack_t type, uint32_t token, uint32_t delay_ms)
{
    g_app.pending_ack = type;
    g_app.pending_command_token = token;
    g_app.pending_ack_due_ms = lv_tick_get() + delay_ms;
}

static void cancel_scheduled_ack(void)
{
    g_app.pending_ack = ACK_NONE;
    g_app.pending_command_token = 0U;
    g_app.pending_ack_due_ms = 0U;
}

static void request_add(void)
{
    ui_view_model_t before = view_from_state(&g_app.domain);
    tr_event_t event = tr_event_make(TR_EVENT_ADD_REQUEST);
    tr_result_t result = tr_dispatch(&g_app.domain, &event);
    if(result.command != TR_COMMAND_COMMIT_ADD) {
        set_hint("当前没有可添加的稳定食材", 1800U);
        return;
    }
    schedule_ack(ACK_ADD, result.command_token, 180U);
    set_hint("已发出添加命令，等待本地写入确认", 1000U);
    g_app.visible = before;
}

static void request_tare(void)
{
    if(g_app.current_weight_g < 5.0) {
        set_hint("空秤左滑只唤醒，不执行去皮", 1800U);
        return;
    }
    ui_view_model_t before = view_from_state(&g_app.domain);
    tr_event_t event = tr_event_make(TR_EVENT_TARE_REQUEST);
    event.qualified = true;
    tr_result_t result = tr_dispatch(&g_app.domain, &event);
    if(result.command != TR_COMMAND_SET_TARE) {
        set_hint("当前状态不能去皮", 1800U);
        return;
    }
    schedule_ack(ACK_TARE, result.command_token, 640U);
    set_hint("手指已释放：等待秤体回弹并复核基线", 1200U);
    g_app.visible = before;
}

static void process_pending_ack(uint32_t now_ms)
{
    if(g_app.pending_ack == ACK_NONE || (int32_t)(now_ms - g_app.pending_ack_due_ms) < 0) return;

    pending_ack_t kind = g_app.pending_ack;
    uint32_t token = g_app.pending_command_token;
    g_app.pending_ack = ACK_NONE;
    g_app.pending_command_token = 0U;

    ui_view_model_t before = g_app.visible;
    tr_event_t event;
    if(kind == ACK_ADD) {
        event = tr_event_make(TR_EVENT_ADD_ACK_SUCCESS);
    }
    else if(kind == ACK_TARE) {
        event = tr_event_make(TR_EVENT_TARE_ACK_SUCCESS);
    }
    else {
        event = tr_event_make(TR_EVENT_FINISH_ACK_SUCCESS);
    }
    event.token = token;
    tr_result_t result = tr_dispatch(&g_app.domain, &event);
    if(result.status != TR_STATUS_APPLIED) {
        set_hint("确认已过期，界面保持真实状态", 1800U);
        return;
    }
    if(kind == ACK_ADD) {
        for(unsigned i = 0; i < UI_NUTRIENT_COUNT; ++i) {
            g_app.total_nutrients[i] += g_app.current_nutrients[i];
        }
        g_app.total_weight_g += g_app.current_weight_g;
        g_app.current_weight_g = 0.0;
        memset(g_app.current_nutrients, 0, sizeof(g_app.current_nutrients));
    }
    else if(kind == ACK_TARE) {
        g_app.current_weight_g = 0.0;
        memset(g_app.current_nutrients, 0, sizeof(g_app.current_nutrients));
    }
    start_feedback_for_operation(&before);
}

static void begin_finish_request(void)
{
    tr_event_t event = tr_event_make(TR_EVENT_FINISH_REQUEST);
    tr_result_t result = tr_dispatch(&g_app.domain, &event);
    if(result.command != TR_COMMAND_COMMIT_FINISH) return;

    ui_view_model_t before = g_app.visible;
    ui_view_model_t after = view_from_state(&g_app.domain);
    after.primary = UI_PRIMARY_UPLOAD;
    after.right = UI_RIGHT_UPLOAD;
    start_transition(UI_TRANSITION_FINISH_UPLOAD, &before, &after, false);
    g_app.finish_request_sent = true;
    g_app.right_pressed = false;

    if(g_app.phone_responds) {
        schedule_ack(ACK_FINISH, result.command_token, 3000U);
        set_hint("本地已封账，正在等待手机保存确认", 1800U);
    }
    else {
        g_app.pending_ack = ACK_NONE;
        g_app.pending_command_token = result.command_token;
        set_hint("手机无响应：保持上传中，不清空餐次", 3000U);
    }
}

static void update_right_hold(uint32_t now_ms)
{
    if(!g_app.right_pressed || g_app.transition_active || g_app.boot_playing) return;
    uint32_t held_ms = now_ms - g_app.right_pressed_ms;

    if(held_ms >= 800U && !g_app.finish_hold_started && tr_can_finish(&g_app.domain)) {
        tr_event_t begin = tr_event_make(TR_EVENT_FINISH_HOLD_BEGIN);
        if(tr_dispatch(&g_app.domain, &begin).status == TR_STATUS_APPLIED) {
            g_app.finish_hold_started = true;
            g_app.visible = view_from_state(&g_app.domain);
        }
    }

    if(g_app.finish_hold_started && !g_app.finish_request_sent) {
        double ratio = held_ms <= 800U ? 0.0 : (double)(held_ms - 800U) / 1200.0;
        if(ratio > 1.0) ratio = 1.0;
        g_app.visible = view_from_state(&g_app.domain);
        g_app.visible.hold_ratio = ratio;
        if(held_ms >= 2000U) begin_finish_request();
    }
}

static void right_touch_event(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    uint32_t now_ms = lv_tick_get();
    if(code == LV_EVENT_PRESSED) {
        if(g_app.boot_playing || g_app.transition_active) return;
        g_app.right_pressed = true;
        g_app.finish_hold_started = false;
        g_app.finish_request_sent = false;
        g_app.right_pressed_ms = now_ms;
        return;
    }
    if(code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) return;
    if(!g_app.right_pressed) return;

    uint32_t held_ms = now_ms - g_app.right_pressed_ms;
    g_app.right_pressed = false;
    if(g_app.finish_request_sent) return;

    if(g_app.finish_hold_started) {
        tr_event_t cancel = tr_event_make(TR_EVENT_FINISH_HOLD_CANCEL);
        (void)tr_dispatch(&g_app.domain, &cancel);
        g_app.visible = view_from_state(&g_app.domain);
        set_hint("长按未满 2 秒：取消结束，也不会误添加", 1800U);
        return;
    }
    if(held_ms < 800U) request_add();
}

static int pointer_pad(lv_obj_t *target, const lv_point_t *point)
{
    lv_area_t area;
    lv_obj_get_coords(target, &area);
    int relative_x = point->x - area.x1;
    if(relative_x < 0) relative_x = 0;
    if(relative_x >= UI_LEFT_W) relative_x = UI_LEFT_W - 1;
    return relative_x * 4 / UI_LEFT_W + 1;
}

static void left_touch_event(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *target = lv_event_get_target_obj(event);
    lv_indev_t *indev = lv_indev_active();
    if(!indev) return;

    if(code == LV_EVENT_PRESSED) {
        g_app.left_pressed = true;
        g_app.left_pressed_ms = lv_tick_get();
        lv_indev_get_point(indev, &g_app.left_start);
        return;
    }
    if(code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) return;
    if(!g_app.left_pressed) return;
    g_app.left_pressed = false;

    lv_point_t end;
    lv_indev_get_point(indev, &end);
    uint32_t duration = lv_tick_get() - g_app.left_pressed_ms;
    int start_pad = pointer_pad(target, &g_app.left_start);
    int end_pad = pointer_pad(target, &end);
    int dx = end.x - g_app.left_start.x;

    /*
     * The PC pointer proxy accepts a deliberate cross-pad swipe up to 1.2 s.
     * Firmware qualification remains in tare_engine.c: stable pre-window,
     * touch-pressure exclusion, release guard and post-release baseline check.
     */
    if(dx <= -48 && end_pad < start_pad && duration <= 1200U) {
        request_tare();
        return;
    }

    if(dx > -18 && dx < 18 && duration < 500U) {
        tr_event_t domain_event;
        if(g_app.domain.candidates_open) {
            domain_event = tr_event_make(TR_EVENT_SELECT_CANDIDATE);
            if(tr_dispatch(&g_app.domain, &domain_event).status == TR_STATUS_APPLIED) {
                ui_view_model_t before = g_app.visible;
                ui_view_model_t after = view_from_state(&g_app.domain);
                start_transition(UI_TRANSITION_FOOD_OVERRIDE, &before, &after, false);
                update_stage_status();
            }
        }
        else if(start_pad == 1 && g_app.domain.food_active) {
            domain_event = tr_event_make(TR_EVENT_OPEN_CANDIDATES);
            if(tr_dispatch(&g_app.domain, &domain_event).status == TR_STATUS_APPLIED) {
                g_app.visible = view_from_state(&g_app.domain);
                set_hint("四个候选与四个透明触点一一对应", 1800U);
                update_stage_status();
            }
        }
    }
}

static void perform_action(simulator_action_t action)
{
    if(g_app.boot_playing) {
        set_hint("开机动画 9 秒不可跳过", 1200U);
        return;
    }

    ui_view_model_t before = g_app.visible;
    tr_event_t event;
    tr_result_t result;
    switch(action) {
        case ACTION_LOAD:
            if(g_app.domain.operation != TR_OP_NONE) break;
            if(g_app.current_weight_g <= 0.0) g_app.current_weight_g = 50.0;
            event = tr_event_make(TR_EVENT_WEIGHT_STATUS);
            event.weight_positive = true;
            event.weight_stable = true;
            (void)tr_dispatch(&g_app.domain, &event);
            event = tr_event_make(TR_EVENT_FIRST_WEIGHT_STABLE);
            result = tr_dispatch(&g_app.domain, &event);
            if(result.status == TR_STATUS_APPLIED) {
                recalculate_current_nutrients();
                ui_view_model_t after = view_from_state(&g_app.domain);
                start_transition(UI_TRANSITION_GUIDE_ENTER, &before, &after, false);
                g_app.guide_deadline_ms = g_app.transition.t0_ms + 5000U;
            }
            break;

        case ACTION_RECOGNIZE:
            if(g_app.domain.operation != TR_OP_NONE) break;
            event = tr_event_make(TR_EVENT_AI_RESULT_STABLE);
            result = tr_dispatch(&g_app.domain, &event);
            if(result.status == TR_STATUS_APPLIED) {
                bool from_guide = before.primary == UI_PRIMARY_GUIDE;
                g_app.guide_deadline_ms = 0U;
                g_app.food_name = "苹果";
                recalculate_current_nutrients();
                ui_view_model_t after = view_from_state(&g_app.domain);
                start_transition(from_guide ? UI_TRANSITION_GUIDE_TO_FOOD : UI_TRANSITION_RECOGNIZE,
                    &before, &after, false);
            }
            break;

        case ACTION_WEIGHT_PLUS:
            if(g_app.current_weight_g <= 0.0) g_app.current_weight_g = 50.0;
            else g_app.current_weight_g += 50.0;
            event = tr_event_make(TR_EVENT_WEIGHT_STATUS);
            event.weight_positive = true;
            event.weight_stable = true;
            (void)tr_dispatch(&g_app.domain, &event);
            recalculate_current_nutrients();
            g_app.visible = view_from_state(&g_app.domain);
            set_hint("实时重量直接更新，不播放装饰性数字动画", 1600U);
            break;

        case ACTION_ADD:
            request_add();
            break;

        case ACTION_TARE:
            request_tare();
            break;

        case ACTION_NUTRIENT: {
            g_app.nutrient = (ui_nutrient_t)((g_app.nutrient + 1) % UI_NUTRIENT_COUNT);
            ui_view_model_t after = view_from_state(&g_app.domain);
            start_transition(UI_TRANSITION_NUTRIENT, &before, &after, false);
            break;
        }

        case ACTION_UNIT: {
            g_app.unit = (ui_weight_unit_t)((g_app.unit + 1) % 4);
            ui_view_model_t after = view_from_state(&g_app.domain);
            start_transition(UI_TRANSITION_UNIT, &before, &after, false);
            break;
        }

        case ACTION_BLE_LINK:
            event = tr_event_make(g_app.domain.ble_link_up ? TR_EVENT_BLE_DISCONNECTED : TR_EVENT_BLE_LINK_UP);
            (void)tr_dispatch(&g_app.domain, &event);
            g_app.visible = view_from_state(&g_app.domain);
            set_hint(g_app.domain.ble_link_up ? "仅蓝牙链路建立：暂不显示连接成功" : "蓝牙已断开：本地称重仍可用", 2200U);
            break;

        case ACTION_APP_READY:
            if(!g_app.domain.ble_link_up) {
                event = tr_event_make(TR_EVENT_BLE_LINK_UP);
                (void)tr_dispatch(&g_app.domain, &event);
            }
            event = tr_event_make(TR_EVENT_APP_SESSION_READY);
            event.goal_valid = true;
            result = tr_dispatch(&g_app.domain, &event);
            if(result.status == TR_STATUS_APPLIED && g_app.domain.operation == TR_OP_BLE_FEEDBACK) {
                g_app.pairing_prompt_active = false;
                g_app.pairing_deadline_ms = 0U;
                ui_view_model_t actual_before = before;
                ui_view_model_t after = preview_after_animation(&g_app.domain);
                start_transition(UI_TRANSITION_CONNECTED, &actual_before, &after, true);
            }
            break;

        case ACTION_OVERLOAD:
            event = tr_event_make(g_app.domain.overloaded ? TR_EVENT_OVERLOAD_CLEAR : TR_EVENT_OVERLOAD_ENTER);
            result = tr_dispatch(&g_app.domain, &event);
            if(result.status == TR_STATUS_APPLIED) {
                if(g_app.domain.overloaded) {
                    cancel_scheduled_ack();
                    ui_view_model_t after = view_from_state(&g_app.domain);
                    start_transition(UI_TRANSITION_ALERT_ENTER, &before, &after, false);
                }
                else {
                    g_app.current_weight_g = 0.0;
                    memset(g_app.current_nutrients, 0, sizeof(g_app.current_nutrients));
                    ui_view_model_t after = view_from_state(&g_app.domain);
                    start_transition(UI_TRANSITION_ALERT_EXIT, &before, &after, false);
                }
            }
            break;

        case ACTION_LOW_BATTERY:
            g_app.battery_percent = 20;
            event = tr_event_make(TR_EVENT_LOW_BATTERY);
            result = tr_dispatch(&g_app.domain, &event);
            if(result.status == TR_STATUS_APPLIED && g_app.domain.operation == TR_OP_LOW_BATTERY_FEEDBACK) {
                start_feedback_for_operation(&before);
            }
            break;

        case ACTION_CRITICAL_BATTERY:
            g_app.battery_percent = 2;
            event = tr_event_make(TR_EVENT_CRITICAL_BATTERY);
            result = tr_dispatch(&g_app.domain, &event);
            if(result.status == TR_STATUS_APPLIED && g_app.domain.operation == TR_OP_CRITICAL_BATTERY) {
                cancel_scheduled_ack();
                ui_view_model_t after = view_from_state(&g_app.domain);
                start_transition(UI_TRANSITION_CRITICAL_SHUTDOWN, &before, &after, false);
            }
            break;

        case ACTION_CHARGE:
            event = tr_event_make(g_app.domain.charger_connected
                ? TR_EVENT_CHARGER_DISCONNECTED
                : TR_EVENT_CHARGER_CONNECTED);
            result = tr_dispatch(&g_app.domain, &event);
            if(result.status == TR_STATUS_APPLIED) {
                if(g_app.domain.charger_connected) cancel_scheduled_ack();
                ui_view_model_t after = view_from_state(&g_app.domain);
                start_transition(g_app.domain.charger_connected
                    ? UI_TRANSITION_CHARGING_ENTER
                    : UI_TRANSITION_CHARGING_EXIT, &before, &after, false);
            }
            break;

        case ACTION_SLEEP_WAKE:
            if(g_app.domain.mode == TR_MODE_SLEEPING || g_app.domain.operation == TR_OP_SLEEP_FADE) {
                event = tr_event_make(TR_EVENT_WAKE_REQUEST);
                event.wake_source = TR_WAKE_UNIT_KEY;
                result = tr_dispatch(&g_app.domain, &event);
                if(result.status == TR_STATUS_APPLIED) start_feedback_for_operation(&before);
            }
            else {
                event = tr_event_make(TR_EVENT_SLEEP_TIMEOUT);
                result = tr_dispatch(&g_app.domain, &event);
                if(result.status == TR_STATUS_APPLIED) start_feedback_for_operation(&before);
            }
            break;

        case ACTION_PHONE_RESPONSE:
            g_app.phone_responds = !g_app.phone_responds;
            lv_label_set_text(g_app.phone_button_label, g_app.phone_responds ? "手机响应 ON" : "手机响应 OFF");
            if(g_app.phone_responds && g_app.domain.operation == TR_OP_FINISH_WAIT_ACK &&
               g_app.pending_command_token != 0U && g_app.pending_ack == ACK_NONE) {
                schedule_ack(ACK_FINISH, g_app.pending_command_token, 600U);
            }
            set_hint(g_app.phone_responds ? "手机恢复响应" : "手机无响应时上传动画会持续，但数据不清空", 2200U);
            break;

        case ACTION_FINISH_DEMO:
            if(!tr_can_finish(&g_app.domain)) {
                set_hint("请先添加当前食材；未提交重量不能被结束操作跳过", 2200U);
                break;
            }
            event = tr_event_make(TR_EVENT_FINISH_HOLD_BEGIN);
            if(tr_dispatch(&g_app.domain, &event).status == TR_STATUS_APPLIED) {
                g_app.right_pressed = true;
                g_app.finish_hold_started = true;
                g_app.finish_request_sent = false;
                g_app.right_pressed_ms = lv_tick_get() - 800U;
                g_app.visible = view_from_state(&g_app.domain);
                g_app.visible.hold_ratio = 0.0;
                set_hint("设计验收：模拟右屏已按住 0.8 秒，请继续观察至提交", 2200U);
            }
            break;
    }
    update_stage_status();
}

static void action_button_event(lv_event_t *event)
{
    if(lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    simulator_action_t action = (simulator_action_t)(uintptr_t)lv_event_get_user_data(event);
    perform_action(action);
}

static lv_obj_t *create_action_button(
    lv_obj_t *parent,
    int x,
    int y,
    int w,
    const char *label_text,
    simulator_action_t action)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, w, 40);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x2A2E36), LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x383E49),
        (lv_style_selector_t)LV_PART_MAIN | (lv_style_selector_t)LV_STATE_PRESSED);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, lv_color_hex(0x464C57), LV_PART_MAIN);
    lv_obj_set_style_radius(button, 12, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(button, action_button_event, LV_EVENT_CLICKED, (void *)(uintptr_t)action);
    lv_obj_t *label = make_stage_label(button, 0, 9, w, 24, label_text, &font_ui_18,
        UI_COLOR_WHITE, LV_TEXT_ALIGN_CENTER);
    if(action == ACTION_PHONE_RESPONSE) g_app.phone_button_label = label;
    return button;
}

static void create_controller(lv_obj_t *parent)
{
    make_stage_label(parent, 32, 318, 1056, 30,
        "真实事件预演｜屏内无调试层，所有按钮只向状态机发事件",
        &font_ui_18, lv_color_hex(0xAEB5C2), LV_TEXT_ALIGN_LEFT);
    g_app.state_label = make_stage_label(parent, 32, 350, 1056, 28, "", &font_ui_18,
        UI_COLOR_WHITE, LV_TEXT_ALIGN_LEFT);

    static const struct {
        const char *label;
        simulator_action_t action;
    } row1[] = {
        {"放入食材", ACTION_LOAD}, {"识别苹果", ACTION_RECOGNIZE},
        {"重量 +50g", ACTION_WEIGHT_PLUS}, {"短按添加", ACTION_ADD},
        {"安全去皮", ACTION_TARE}, {"切换营养", ACTION_NUTRIENT},
        {"切换单位", ACTION_UNIT}, {"蓝牙链路", ACTION_BLE_LINK}
    };
    static const struct {
        const char *label;
        simulator_action_t action;
    } row2[] = {
        {"APP 握手", ACTION_APP_READY}, {"超重 / 解除", ACTION_OVERLOAD},
        {"低电量 20%", ACTION_LOW_BATTERY}, {"极低电量", ACTION_CRITICAL_BATTERY},
        {"充电 / 拔线", ACTION_CHARGE}, {"休眠 / 唤醒", ACTION_SLEEP_WAKE},
        {"手机响应 ON", ACTION_PHONE_RESPONSE}, {"长按完成", ACTION_FINISH_DEMO}
    };

    const int start_x = 32;
    const int gap = 8;
    const int button_w = 124;
    for(unsigned i = 0; i < sizeof(row1) / sizeof(row1[0]); ++i) {
        create_action_button(parent, start_x + (int)i * (button_w + gap), 390, button_w,
            row1[i].label, row1[i].action);
        create_action_button(parent, start_x + (int)i * (button_w + gap), 440, button_w,
            row2[i].label, row2[i].action);
    }

    g_app.hint_label = make_stage_label(parent, 32, 500, 1056, 30,
        "右屏短按添加；按住 0.8 秒进入紫色结束意图，满 2 秒才提交",
        &font_ui_18, lv_color_hex(0x9DFF9B), LV_TEXT_ALIGN_LEFT);
    make_stage_label(parent, 32, 540, 1056, 58,
        "左屏透明膜：点击最左区打开候选；从任意右侧触点向左滑过 2–3 区可提出去皮。\n按压期间的重量波动不参与稳定判断，松手回弹并复核基线后才会 ACK。",
        &font_ui_18, lv_color_hex(0x858D9B), LV_TEXT_ALIGN_LEFT);
}

static void create_boot_layers(void)
{
    g_app.boot_left = lv_gif_create(g_app.left_frame);
    lv_gif_set_color_format(g_app.boot_left, LV_COLOR_FORMAT_ARGB8888);
    lv_gif_set_src(g_app.boot_left, "A:assets/fixed/boot_left.gif");
    lv_obj_set_pos(g_app.boot_left, 0, 0);
    lv_gif_set_loop_count(g_app.boot_left, 1);

    g_app.boot_right = lv_gif_create(g_app.right_frame);
    lv_gif_set_color_format(g_app.boot_right, LV_COLOR_FORMAT_ARGB8888);
    lv_gif_set_src(g_app.boot_right, "A:assets/fixed/boot_right.gif");
    lv_obj_set_pos(g_app.boot_right, 0, 0);
    lv_gif_set_loop_count(g_app.boot_right, 1);

    lv_gif_restart(g_app.boot_left);
    lv_gif_restart(g_app.boot_right);
    g_app.boot_playing = true;
    g_app.boot_assets_ready = true;
    g_app.boot_started_ms = lv_tick_get();
}

static void deferred_boot_start(lv_timer_t *timer)
{
    create_boot_layers();
    lv_timer_delete(timer);
}

static void complete_boot(void)
{
    lv_gif_pause(g_app.boot_left);
    lv_gif_pause(g_app.boot_right);
    lv_obj_add_flag(g_app.boot_left, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_app.boot_right, LV_OBJ_FLAG_HIDDEN);
    g_app.boot_playing = false;

    tr_event_t done = tr_event_make(TR_EVENT_ANIMATION_FINISHED);
    done.token = g_app.domain.scene_token;
    (void)tr_dispatch(&g_app.domain, &done);
    g_app.visible = view_from_state(&g_app.domain);
    update_stage_status();
    begin_pairing_prompt();
}

static void app_tick(lv_timer_t *timer)
{
    (void)timer;
    uint32_t now_ms = lv_tick_get();

    if(g_app.boot_playing) {
        if(g_app.boot_assets_ready && now_ms - g_app.boot_started_ms >= 9000U) complete_boot();
        return;
    }

    update_right_hold(now_ms);
    process_pending_ack(now_ms);

    if(g_app.pairing_prompt_active && g_app.pairing_deadline_ms != 0U &&
       (int32_t)(now_ms - g_app.pairing_deadline_ms) >= 0 && !g_app.domain.app_session_ready) {
        ui_view_model_t before = g_app.visible;
        ui_view_model_t after = view_from_state(&g_app.domain);
        g_app.pairing_prompt_active = false;
        g_app.pairing_deadline_ms = 0U;
        start_transition(UI_TRANSITION_PAIRING_TIMEOUT, &before, &after, false);
    }

    if(g_app.guide_deadline_ms != 0U && (int32_t)(now_ms - g_app.guide_deadline_ms) >= 0 &&
       g_app.domain.first_guide_active) {
        ui_view_model_t before = g_app.visible;
        tr_event_t timeout = tr_event_make(TR_EVENT_FIRST_GUIDE_TIMEOUT);
        if(tr_dispatch(&g_app.domain, &timeout).status == TR_STATUS_APPLIED) {
            ui_view_model_t after = view_from_state(&g_app.domain);
            g_app.guide_deadline_ms = 0U;
            start_transition(UI_TRANSITION_GUIDE_TIMEOUT, &before, &after, false);
        }
    }

    if(g_app.transition_active) {
        uint32_t elapsed = now_ms - g_app.transition.t0_ms;
        ui_left_screen_render(g_app.left, &g_app.transition.before, &g_app.transition.after,
            g_app.transition.id, elapsed);
        right_screen_render(g_app.right, &g_app.transition.before, &g_app.transition.after,
            g_app.transition.id, elapsed);
        if(elapsed >= g_app.transition.duration_ms) complete_transition();
    }
    else {
        g_app.visible = view_from_state(&g_app.domain);
        if(g_app.pairing_prompt_active && !g_app.domain.app_session_ready) {
            g_app.visible.primary = UI_PRIMARY_PAIRING;
        }
        if(g_app.finish_hold_started && !g_app.finish_request_sent && g_app.right_pressed) {
            uint32_t held_ms = now_ms - g_app.right_pressed_ms;
            g_app.visible.hold_ratio = held_ms <= 800U ? 0.0 : (double)(held_ms - 800U) / 1200.0;
            if(g_app.visible.hold_ratio > 1.0) g_app.visible.hold_ratio = 1.0;
        }
        ui_left_screen_render(g_app.left, &g_app.visible, &g_app.visible, UI_TRANSITION_NONE, now_ms);
        right_screen_render(g_app.right, &g_app.visible, &g_app.visible, UI_TRANSITION_NONE, now_ms);
    }

    if(g_app.hint_clear_ms != 0U && (int32_t)(now_ms - g_app.hint_clear_ms) >= 0) {
        lv_label_set_text(g_app.hint_label,
            "右屏短按添加；按住 0.8 秒进入紫色结束意图，满 2 秒才提交");
        g_app.hint_clear_ms = 0U;
    }
}

void ui_app_create(lv_display_t *display)
{
    (void)display;
    memset(&g_app, 0, sizeof(g_app));
    g_app.nutrient = UI_NUTRIENT_CALORIES;
    g_app.unit = UI_UNIT_G;
    g_app.food_name = "苹果";
    g_app.battery_percent = 76;
    g_app.charge_minutes = 48;
    g_app.phone_responds = true;
    g_app.nutrient_goals[UI_NUTRIENT_CALORIES] = 2000.0;
    g_app.nutrient_goals[UI_NUTRIENT_PROTEIN] = 60.0;
    g_app.nutrient_goals[UI_NUTRIENT_FAT] = 60.0;
    g_app.nutrient_goals[UI_NUTRIENT_CARBS] = 250.0;
    g_app.nutrient_goals[UI_NUTRIENT_SODIUM] = 2000.0;

    lv_obj_t *root = lv_screen_active();
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    make_stage_label(root, 32, 14, 720, 32, "卡路里秤 · 双屏真实场景预演", &font_ui_24,
        UI_COLOR_WHITE, LV_TEXT_ALIGN_LEFT);
    make_stage_label(root, 800, 17, 288, 28, "ESP32-S3 N16R8 · LVGL 9.5", &font_ui_18,
        lv_color_hex(0x7E8795), LV_TEXT_ALIGN_RIGHT);

    make_stage_rect(root, 30, 58, 652, 204, lv_color_hex(0x30343B), 7);
    g_app.left_frame = make_stage_rect(root, 32, 60, UI_LEFT_W, UI_LEFT_H, UI_COLOR_BLACK, 0);
    make_stage_rect(root, 820, 38, 244, 244, lv_color_hex(0x30343B), 122);
    g_app.right_frame = make_stage_rect(root, 822, 40, UI_RIGHT_W, UI_RIGHT_H, UI_COLOR_BLACK, 120);
    lv_obj_set_style_clip_corner(g_app.left_frame, true, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(g_app.right_frame, true, LV_PART_MAIN);

    g_app.left = ui_left_screen_create(g_app.left_frame);
    g_app.right = right_screen_create(g_app.right_frame);
    lv_obj_add_event_cb(ui_left_screen_touch_target(g_app.left), left_touch_event, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_right_screen_touch_target(g_app.right), right_touch_event, LV_EVENT_ALL, NULL);

    create_controller(root);
    tr_state_init(&g_app.domain);
    tr_event_t power_on = tr_event_make(TR_EVENT_POWER_ON);
    (void)tr_dispatch(&g_app.domain, &power_on);
    g_app.visible = view_from_state(&g_app.domain);
    /* Paint the host and two black TFTs before decoding the two GIFs. */
    g_app.boot_playing = true;
    g_app.boot_assets_ready = false;
    lv_timer_create(deferred_boot_start, 80U, NULL);
    update_stage_status();
    lv_timer_create(app_tick, 16, NULL);
}
