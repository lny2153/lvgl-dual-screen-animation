#include "transition_rules.h"

#include <stddef.h>
#include <string.h>

enum {
    TR_PRIORITY_PASSIVE = 100,
    TR_PRIORITY_CONTENT = 300,
    TR_PRIORITY_FEEDBACK = 400,
    TR_PRIORITY_TRANSACTION = 500,
    TR_PRIORITY_POWER_FLOW = 600,
    TR_PRIORITY_LOW_BATTERY = 700,
    TR_PRIORITY_BOOT = 800,
    TR_PRIORITY_CHARGING = 850,
    TR_PRIORITY_OVERLOAD = 900,
    TR_PRIORITY_POWER_OFF = 1000
};

static tr_result_t make_result(const tr_state_t *state,
                               tr_status_t status,
                               tr_command_t command,
                               uint32_t command_token)
{
    tr_result_t result;
    result.status = status;
    result.command = command;
    result.command_token = command_token;
    result.revision = state != NULL ? state->revision : 0U;
    result.scene_token = state != NULL ? state->scene_token : 0U;
    return result;
}

static tr_result_t applied(tr_state_t *state, tr_command_t command, uint32_t command_token)
{
    state->revision += 1U;
    if (state->revision == 0U) {
        state->revision = 1U;
    }
    return make_result(state, TR_STATUS_APPLIED, command, command_token);
}

static void show_scenes(tr_state_t *state,
                        tr_left_scene_t left,
                        tr_right_scene_t right,
                        bool force_new_token)
{
    if (force_new_token || state->left_scene != left || state->right_scene != right) {
        state->left_scene = left;
        state->right_scene = right;
        state->scene_token += 1U;
        if (state->scene_token == 0U) {
            state->scene_token = 1U;
        }
    }
}

static tr_right_scene_t base_right_scene(const tr_state_t *state)
{
    return (state->app_session_ready && state->goal_valid)
               ? TR_RIGHT_PLUS_WITH_GOAL
               : TR_RIGHT_PLUS;
}

static void set_base_left(tr_state_t *state, tr_left_scene_t scene)
{
    state->food_active = false;
    state->candidates_open = false;
    state->first_guide_active = false;

    switch (scene) {
    case TR_LEFT_FOOD:
        state->food_active = true;
        break;
    case TR_LEFT_CANDIDATES:
        state->food_active = true;
        state->candidates_open = true;
        break;
    case TR_LEFT_FIRST_WEIGHT_GUIDE:
        state->first_guide_active = true;
        break;
    case TR_LEFT_EYE:
    default:
        scene = TR_LEFT_EYE;
        break;
    }
    state->base_left_scene = scene;
}

static void update_active_radios_and_camera(tr_state_t *state)
{
    state->touch_monitoring_enabled = true;
    state->weight_monitoring_enabled = true;
    state->wifi_enabled = true;
    state->camera_enabled = (state->base_left_scene == TR_LEFT_EYE ||
                             state->base_left_scene == TR_LEFT_FIRST_WEIGHT_GUIDE);
}

static void restore_base_scenes(tr_state_t *state)
{
    state->mode = TR_MODE_ACTIVE;
    update_active_radios_and_camera(state);
    show_scenes(state, state->base_left_scene, base_right_scene(state), false);
}

static void refresh_base_dependent_right_scene(tr_state_t *state)
{
    if (state->mode != TR_MODE_ACTIVE) {
        return;
    }
    switch (state->operation) {
    case TR_OP_NONE:
    case TR_OP_ADD_WAIT_ACK:
    case TR_OP_TARE_WAIT_ACK:
        show_scenes(state, state->base_left_scene, base_right_scene(state), false);
        break;
    case TR_OP_TARE_FEEDBACK:
        show_scenes(state, TR_LEFT_TARE_SUCCESS, base_right_scene(state), false);
        break;
    case TR_OP_LOW_BATTERY_FEEDBACK:
        show_scenes(state, TR_LEFT_LOW_BATTERY, base_right_scene(state), false);
        break;
    default:
        break;
    }
}

static uint32_t issue_command_token(tr_state_t *state)
{
    uint32_t token = state->next_command_token;
    state->next_command_token += 1U;
    if (token == 0U) {
        token = state->next_command_token;
        state->next_command_token += 1U;
    }
    if (state->next_command_token == 0U) {
        state->next_command_token = 1U;
    }
    state->pending_command_token = token;
    return token;
}

static void cancel_pending_command(tr_state_t *state)
{
    state->pending_command_token = 0U;
}

static bool ack_matches(const tr_state_t *state,
                        const tr_event_t *event,
                        tr_operation_t expected)
{
    return state->operation == expected &&
           state->pending_command_token != 0U &&
           event->token == state->pending_command_token;
}

static bool animation_matches(const tr_state_t *state, const tr_event_t *event)
{
    return event->token != 0U && event->token == state->scene_token;
}

static void capture_sleep_snapshot(tr_state_t *state)
{
    tr_sleep_snapshot_t *snapshot = &state->sleep_snapshot;
    snapshot->valid = true;
    snapshot->base_left = state->base_left_scene;
    snapshot->food_active = state->food_active;
    snapshot->candidates_open = state->candidates_open;
    snapshot->first_guide_active = state->first_guide_active;
    snapshot->current_weight_positive = state->current_weight_positive;
    snapshot->current_weight_stable = state->current_weight_stable;
    snapshot->camera_enabled = state->camera_enabled;
    snapshot->wifi_enabled = state->wifi_enabled;
    snapshot->meal_item_count = state->meal_item_count;
}

static void restore_sleep_snapshot(tr_state_t *state)
{
    const tr_sleep_snapshot_t *snapshot = &state->sleep_snapshot;
    if (snapshot->valid) {
        state->base_left_scene = snapshot->base_left;
        state->food_active = snapshot->food_active;
        state->candidates_open = snapshot->candidates_open;
        state->first_guide_active = snapshot->first_guide_active;
        state->current_weight_positive = snapshot->current_weight_positive;
        state->current_weight_stable = snapshot->current_weight_stable;
        state->meal_item_count = snapshot->meal_item_count;
    } else {
        set_base_left(state, TR_LEFT_EYE);
        state->current_weight_positive = false;
        state->current_weight_stable = false;
    }
    restore_base_scenes(state);
    state->sleep_snapshot.valid = false;
}

static void reset_runtime_for_boot(tr_state_t *state)
{
    state->mode = TR_MODE_BOOTING;
    state->operation = TR_OP_BOOT_ANIMATION;
    set_base_left(state, TR_LEFT_EYE);
    state->current_weight_positive = false;
    state->current_weight_stable = false;
    state->ble_link_up = false;
    state->app_session_ready = false;
    state->goal_valid = false;
    state->low_battery = false;
    state->critical_battery = false;
    state->charger_connected = false;
    state->overloaded = false;
    state->camera_enabled = false;
    state->wifi_enabled = false;
    state->touch_monitoring_enabled = true;
    state->weight_monitoring_enabled = true;
    state->meal_item_count = 0U;
    state->completed_item_count = 0U;
    state->pending_command_token = 0U;
    state->sleep_snapshot.valid = false;
    show_scenes(state, TR_LEFT_BOOT, TR_RIGHT_BOOT, true);
}

static void enter_power_off(tr_state_t *state)
{
    state->mode = TR_MODE_POWER_OFF;
    state->operation = TR_OP_NONE;
    set_base_left(state, TR_LEFT_EYE);
    state->current_weight_positive = false;
    state->current_weight_stable = false;
    state->app_session_ready = false;
    state->goal_valid = false;
    state->ble_link_up = false;
    state->charger_connected = false;
    state->overloaded = false;
    state->camera_enabled = false;
    state->wifi_enabled = false;
    state->touch_monitoring_enabled = false;
    state->weight_monitoring_enabled = false;
    state->meal_item_count = 0U;
    state->completed_item_count = 0U;
    cancel_pending_command(state);
    state->sleep_snapshot.valid = false;
    show_scenes(state, TR_LEFT_OFF, TR_RIGHT_OFF, false);
}

void tr_state_init(tr_state_t *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->mode = TR_MODE_POWER_OFF;
    state->operation = TR_OP_NONE;
    state->left_scene = TR_LEFT_OFF;
    state->right_scene = TR_RIGHT_OFF;
    state->base_left_scene = TR_LEFT_EYE;
    state->next_command_token = 1U;
}

tr_event_t tr_event_make(tr_event_type_t type)
{
    tr_event_t event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    return event;
}

bool tr_can_add(const tr_state_t *state)
{
    return state != NULL &&
           state->mode == TR_MODE_ACTIVE &&
           state->operation == TR_OP_NONE &&
           state->food_active &&
           !state->candidates_open &&
           state->current_weight_positive &&
           state->current_weight_stable &&
           !state->overloaded &&
           !state->critical_battery &&
           !state->charger_connected;
}

bool tr_can_finish(const tr_state_t *state)
{
    return state != NULL &&
           state->mode == TR_MODE_ACTIVE &&
           state->operation == TR_OP_NONE &&
           state->meal_item_count > 0U &&
           !state->current_weight_positive &&
           !state->overloaded &&
           !state->critical_battery &&
           !state->charger_connected;
}

tr_left_identity_t tr_left_identity(tr_left_scene_t scene)
{
    switch (scene) {
    case TR_LEFT_EYE:
        return TR_IDENTITY_EYE;
    case TR_LEFT_FOOD:
        return TR_IDENTITY_FOOD;
    case TR_LEFT_CANDIDATES:
        return TR_IDENTITY_CANDIDATES;
    case TR_LEFT_BOOT:
    case TR_LEFT_WAKE:
    case TR_LEFT_FIRST_WEIGHT_GUIDE:
    case TR_LEFT_ADD_FEEDBACK:
    case TR_LEFT_BLE_SUCCESS:
    case TR_LEFT_TARE_SUCCESS:
    case TR_LEFT_FINISH_SUCCESS:
    case TR_LEFT_OVERLOAD:
    case TR_LEFT_LOW_BATTERY:
    case TR_LEFT_CRITICAL_BATTERY:
    case TR_LEFT_CHARGING:
    case TR_LEFT_FADE_TO_BLACK:
        return TR_IDENTITY_STATUS;
    case TR_LEFT_OFF:
    default:
        return TR_IDENTITY_NONE;
    }
}

int tr_event_priority(tr_event_type_t type)
{
    switch (type) {
    case TR_EVENT_POWER_OFF:
        return TR_PRIORITY_POWER_OFF;
    case TR_EVENT_OVERLOAD_ENTER:
    case TR_EVENT_OVERLOAD_CLEAR:
        return TR_PRIORITY_OVERLOAD;
    case TR_EVENT_CRITICAL_BATTERY:
    case TR_EVENT_CHARGER_CONNECTED:
    case TR_EVENT_CHARGER_DISCONNECTED:
        return TR_PRIORITY_CHARGING;
    case TR_EVENT_POWER_ON:
        return TR_PRIORITY_BOOT;
    case TR_EVENT_LOW_BATTERY:
        return TR_PRIORITY_LOW_BATTERY;
    case TR_EVENT_SLEEP_TIMEOUT:
    case TR_EVENT_WAKE_REQUEST:
        return TR_PRIORITY_POWER_FLOW;
    case TR_EVENT_FINISH_HOLD_BEGIN:
    case TR_EVENT_FINISH_HOLD_CANCEL:
    case TR_EVENT_FINISH_REQUEST:
    case TR_EVENT_TARE_REQUEST:
        return TR_PRIORITY_TRANSACTION;
    case TR_EVENT_ADD_REQUEST:
    case TR_EVENT_APP_SESSION_READY:
        return TR_PRIORITY_FEEDBACK;
    case TR_EVENT_FIRST_WEIGHT_STABLE:
    case TR_EVENT_FIRST_GUIDE_TIMEOUT:
    case TR_EVENT_AI_RESULT_STABLE:
    case TR_EVENT_OPEN_CANDIDATES:
    case TR_EVENT_SELECT_CANDIDATE:
    case TR_EVENT_APP_FOOD_OVERRIDE:
        return TR_PRIORITY_CONTENT;
    case TR_EVENT_ANIMATION_FINISHED:
    case TR_EVENT_ADD_ACK_SUCCESS:
    case TR_EVENT_ADD_ACK_FAILURE:
    case TR_EVENT_FINISH_ACK_SUCCESS:
    case TR_EVENT_FINISH_ACK_FAILURE:
    case TR_EVENT_TARE_ACK_SUCCESS:
    case TR_EVENT_TARE_ACK_FAILURE:
        return TR_PRIORITY_POWER_OFF; /* Internal completion cannot be starved. */
    case TR_EVENT_BLE_LINK_UP:
    case TR_EVENT_BLE_DISCONNECTED:
    case TR_EVENT_WEIGHT_STATUS:
    default:
        return TR_PRIORITY_PASSIVE;
    }
}

int tr_operation_priority(tr_operation_t operation)
{
    switch (operation) {
    case TR_OP_POWER_OFF_FADE:
        return TR_PRIORITY_POWER_OFF;
    case TR_OP_OVERLOAD:
        return TR_PRIORITY_OVERLOAD;
    case TR_OP_CRITICAL_BATTERY:
    case TR_OP_CHARGING:
        return TR_PRIORITY_CHARGING;
    case TR_OP_BOOT_ANIMATION:
        return TR_PRIORITY_BOOT;
    case TR_OP_LOW_BATTERY_FEEDBACK:
        return TR_PRIORITY_LOW_BATTERY;
    case TR_OP_WAKE_ANIMATION:
    case TR_OP_SLEEP_FADE:
        return TR_PRIORITY_POWER_FLOW;
    case TR_OP_TARE_WAIT_ACK:
    case TR_OP_TARE_FEEDBACK:
    case TR_OP_FINISH_HOLD:
    case TR_OP_FINISH_WAIT_ACK:
    case TR_OP_FINISH_FEEDBACK:
        return TR_PRIORITY_TRANSACTION;
    case TR_OP_ADD_WAIT_ACK:
    case TR_OP_ADD_FEEDBACK:
    case TR_OP_BLE_FEEDBACK:
        return TR_PRIORITY_FEEDBACK;
    case TR_OP_NONE:
    default:
        return 0;
    }
}

static tr_result_t finish_animation(tr_state_t *state, const tr_event_t *event)
{
    if (!animation_matches(state, event)) {
        return make_result(state, TR_STATUS_STALE_TOKEN, TR_COMMAND_NONE, 0U);
    }

    switch (state->operation) {
    case TR_OP_BOOT_ANIMATION:
        state->operation = TR_OP_NONE;
        set_base_left(state, TR_LEFT_EYE);
        restore_base_scenes(state);
        return applied(state, TR_COMMAND_NONE, 0U);
    case TR_OP_WAKE_ANIMATION:
        state->operation = TR_OP_NONE;
        restore_sleep_snapshot(state);
        return applied(state, TR_COMMAND_NONE, 0U);
    case TR_OP_SLEEP_FADE:
        state->mode = TR_MODE_SLEEPING;
        state->operation = TR_OP_NONE;
        state->camera_enabled = false;
        state->wifi_enabled = false;
        state->touch_monitoring_enabled = true;
        state->weight_monitoring_enabled = true;
        show_scenes(state, TR_LEFT_OFF, TR_RIGHT_OFF, false);
        return applied(state, TR_COMMAND_NONE, 0U);
    case TR_OP_POWER_OFF_FADE:
        enter_power_off(state);
        return applied(state, TR_COMMAND_NONE, 0U);
    case TR_OP_BLE_FEEDBACK:
    case TR_OP_ADD_FEEDBACK:
    case TR_OP_TARE_FEEDBACK:
    case TR_OP_LOW_BATTERY_FEEDBACK:
        state->operation = TR_OP_NONE;
        restore_base_scenes(state);
        return applied(state, TR_COMMAND_NONE, 0U);
    case TR_OP_FINISH_FEEDBACK:
        state->operation = TR_OP_NONE;
        state->meal_item_count = 0U;
        state->completed_item_count = 0U;
        set_base_left(state, TR_LEFT_EYE);
        state->current_weight_positive = false;
        state->current_weight_stable = false;
        restore_base_scenes(state);
        return applied(state, TR_COMMAND_NONE, 0U);
    default:
        return make_result(state, TR_STATUS_REJECTED_PRECONDITION, TR_COMMAND_NONE, 0U);
    }
}

tr_result_t tr_dispatch(tr_state_t *state, const tr_event_t *event)
{
    uint32_t token;

    if (state == NULL || event == NULL || event->type >= TR_EVENT_COUNT) {
        return make_result(state, TR_STATUS_REJECTED_PRECONDITION, TR_COMMAND_NONE, 0U);
    }

    if (event->type == TR_EVENT_ANIMATION_FINISHED) {
        return finish_animation(state, event);
    }

    if (event->type == TR_EVENT_POWER_OFF) {
        if (state->mode == TR_MODE_POWER_OFF) {
            return make_result(state, TR_STATUS_IGNORED, TR_COMMAND_NONE, 0U);
        }
        cancel_pending_command(state);
        if (state->mode == TR_MODE_SLEEPING) {
            enter_power_off(state);
        } else {
            state->mode = TR_MODE_ACTIVE;
            state->operation = TR_OP_POWER_OFF_FADE;
            state->camera_enabled = false;
            state->wifi_enabled = false;
            show_scenes(state, TR_LEFT_FADE_TO_BLACK, TR_RIGHT_FADE_TO_BLACK, true);
        }
        return applied(state, TR_COMMAND_NONE, 0U);
    }

    if (event->type == TR_EVENT_POWER_ON) {
        if (state->mode != TR_MODE_POWER_OFF) {
            return make_result(state, TR_STATUS_IGNORED, TR_COMMAND_NONE, 0U);
        }
        reset_runtime_for_boot(state);
        return applied(state, TR_COMMAND_NONE, 0U);
    }

    /* A disconnected scale accepts no domain action other than POWER_ON. */
    if (state->mode == TR_MODE_POWER_OFF) {
        return make_result(state, TR_STATUS_REJECTED_PRECONDITION, TR_COMMAND_NONE, 0U);
    }

    switch (event->type) {
    case TR_EVENT_OVERLOAD_ENTER:
        if (state->overloaded) {
            return make_result(state, TR_STATUS_IGNORED, TR_COMMAND_NONE, 0U);
        }
        state->overloaded = true;
        cancel_pending_command(state);
        state->mode = TR_MODE_ACTIVE;
        state->operation = TR_OP_OVERLOAD;
        state->camera_enabled = false;
        state->wifi_enabled = true;
        state->touch_monitoring_enabled = true;
        state->weight_monitoring_enabled = true;
        show_scenes(state, TR_LEFT_OVERLOAD, TR_RIGHT_OVERLOAD, true);
        return applied(state, TR_COMMAND_NONE, 0U);

    case TR_EVENT_OVERLOAD_CLEAR:
        if (!state->overloaded) {
            return make_result(state, TR_STATUS_IGNORED, TR_COMMAND_NONE, 0U);
        }
        state->overloaded = false;
        state->current_weight_positive = false;
        state->current_weight_stable = false;
        set_base_left(state, TR_LEFT_EYE);
        if (state->charger_connected) {
            state->operation = TR_OP_CHARGING;
            state->camera_enabled = false;
            state->wifi_enabled = false;
            show_scenes(state, TR_LEFT_CHARGING, TR_RIGHT_CHARGING, true);
        } else if (state->critical_battery) {
            state->operation = TR_OP_CRITICAL_BATTERY;
            state->camera_enabled = false;
            state->wifi_enabled = false;
            show_scenes(state, TR_LEFT_CRITICAL_BATTERY, TR_RIGHT_CRITICAL_BATTERY, true);
        } else {
            state->operation = TR_OP_NONE;
            restore_base_scenes(state);
        }
        return applied(state, TR_COMMAND_NONE, 0U);

    case TR_EVENT_CHARGER_CONNECTED:
        state->charger_connected = true;
        if (state->overloaded) {
            return applied(state, TR_COMMAND_NONE, 0U);
        }
        cancel_pending_command(state);
        state->mode = TR_MODE_ACTIVE;
        state->operation = TR_OP_CHARGING;
        state->camera_enabled = false;
        state->wifi_enabled = false;
        state->touch_monitoring_enabled = true;
        state->weight_monitoring_enabled = true;
        show_scenes(state, TR_LEFT_CHARGING, TR_RIGHT_CHARGING, true);
        return applied(state, TR_COMMAND_NONE, 0U);

    case TR_EVENT_CHARGER_DISCONNECTED:
        if (!state->charger_connected) {
            return make_result(state, TR_STATUS_IGNORED, TR_COMMAND_NONE, 0U);
        }
        state->charger_connected = false;
        if (state->overloaded) {
            return applied(state, TR_COMMAND_NONE, 0U);
        }
        if (state->critical_battery) {
            state->mode = TR_MODE_ACTIVE;
            state->operation = TR_OP_CRITICAL_BATTERY;
            state->camera_enabled = false;
            state->wifi_enabled = false;
            show_scenes(state, TR_LEFT_CRITICAL_BATTERY, TR_RIGHT_CRITICAL_BATTERY, true);
        } else {
            state->operation = TR_OP_NONE;
            restore_base_scenes(state);
        }
        return applied(state, TR_COMMAND_NONE, 0U);

    case TR_EVENT_CRITICAL_BATTERY:
        state->critical_battery = true;
        state->low_battery = true;
        if (state->overloaded || state->charger_connected) {
            return applied(state, TR_COMMAND_NONE, 0U);
        }
        cancel_pending_command(state);
        state->mode = TR_MODE_ACTIVE;
        state->operation = TR_OP_CRITICAL_BATTERY;
        state->camera_enabled = false;
        state->wifi_enabled = false;
        show_scenes(state, TR_LEFT_CRITICAL_BATTERY, TR_RIGHT_CRITICAL_BATTERY, true);
        return applied(state, TR_COMMAND_NONE, 0U);

    default:
        break;
    }

    /* While a hard safety screen owns the displays, lower-priority visuals wait. */
    if (state->operation == TR_OP_OVERLOAD ||
        state->operation == TR_OP_CRITICAL_BATTERY ||
        state->operation == TR_OP_CHARGING ||
        state->operation == TR_OP_POWER_OFF_FADE) {
        if (event->type == TR_EVENT_ADD_ACK_SUCCESS ||
            event->type == TR_EVENT_ADD_ACK_FAILURE ||
            event->type == TR_EVENT_TARE_ACK_SUCCESS ||
            event->type == TR_EVENT_TARE_ACK_FAILURE ||
            event->type == TR_EVENT_FINISH_ACK_SUCCESS ||
            event->type == TR_EVENT_FINISH_ACK_FAILURE) {
            return make_result(state, TR_STATUS_STALE_TOKEN, TR_COMMAND_NONE, 0U);
        }
        if (event->type == TR_EVENT_BLE_DISCONNECTED) {
            state->ble_link_up = false;
            state->app_session_ready = false;
            state->goal_valid = false;
            return applied(state, TR_COMMAND_NONE, 0U);
        }
        if (event->type == TR_EVENT_BLE_LINK_UP) {
            state->ble_link_up = true;
            return applied(state, TR_COMMAND_NONE, 0U);
        }
        if (event->type == TR_EVENT_WEIGHT_STATUS) {
            state->current_weight_positive = event->weight_positive;
            state->current_weight_stable = event->weight_stable;
            return applied(state, TR_COMMAND_NONE, 0U);
        }
        return make_result(state, TR_STATUS_IGNORED, TR_COMMAND_NONE, 0U);
    }

    switch (event->type) {
    case TR_EVENT_SLEEP_TIMEOUT:
        if (state->mode != TR_MODE_ACTIVE || state->operation != TR_OP_NONE) {
            return make_result(state, TR_STATUS_REJECTED_PRECONDITION, TR_COMMAND_NONE, 0U);
        }
        capture_sleep_snapshot(state);
        state->operation = TR_OP_SLEEP_FADE;
        state->camera_enabled = false;
        state->wifi_enabled = false;
        show_scenes(state, TR_LEFT_FADE_TO_BLACK, TR_RIGHT_FADE_TO_BLACK, true);
        return applied(state, TR_COMMAND_NONE, 0U);

    case TR_EVENT_WAKE_REQUEST:
        if (event->wake_source == TR_WAKE_NONE ||
            (state->mode != TR_MODE_SLEEPING && state->operation != TR_OP_SLEEP_FADE)) {
            return make_result(state, TR_STATUS_REJECTED_PRECONDITION, TR_COMMAND_NONE, 0U);
        }
        state->mode = TR_MODE_ACTIVE;
        state->operation = TR_OP_WAKE_ANIMATION;
        state->camera_enabled = false;
        state->wifi_enabled = false;
        state->touch_monitoring_enabled = true;
        state->weight_monitoring_enabled = true;
        show_scenes(state, TR_LEFT_WAKE, TR_RIGHT_WAKE, true);
        return applied(state, TR_COMMAND_NONE, 0U);

    case TR_EVENT_WEIGHT_STATUS:
        state->current_weight_positive = event->weight_positive;
        state->current_weight_stable = event->weight_stable;
        return applied(state, TR_COMMAND_NONE, 0U);

    case TR_EVENT_BLE_LINK_UP:
        if (state->ble_link_up) {
            return make_result(state, TR_STATUS_IGNORED, TR_COMMAND_NONE, 0U);
        }
        state->ble_link_up = true;
        return applied(state, TR_COMMAND_NONE, 0U);

    case TR_EVENT_BLE_DISCONNECTED:
        if (!state->ble_link_up && !state->app_session_ready) {
            return make_result(state, TR_STATUS_IGNORED, TR_COMMAND_NONE, 0U);
        }
        state->ble_link_up = false;
        state->app_session_ready = false;
        state->goal_valid = false;
        if (state->operation == TR_OP_BLE_FEEDBACK) {
            state->operation = TR_OP_NONE;
            restore_base_scenes(state);
        } else {
            refresh_base_dependent_right_scene(state);
        }
        return applied(state, TR_COMMAND_NONE, 0U);

    case TR_EVENT_APP_SESSION_READY:
        if (!state->ble_link_up) {
            return make_result(state, TR_STATUS_REJECTED_PRECONDITION, TR_COMMAND_NONE, 0U);
        }
        if (state->app_session_ready && state->goal_valid == event->goal_valid) {
            return make_result(state, TR_STATUS_IGNORED, TR_COMMAND_NONE, 0U);
        }
        state->app_session_ready = true;
        state->goal_valid = event->goal_valid;
        if (state->mode == TR_MODE_ACTIVE && state->operation == TR_OP_NONE) {
            state->operation = TR_OP_BLE_FEEDBACK;
            state->camera_enabled = false;
            show_scenes(state, TR_LEFT_BLE_SUCCESS, TR_RIGHT_BLE_SUCCESS, true);
        } else {
            refresh_base_dependent_right_scene(state);
        }
        return applied(state, TR_COMMAND_NONE, 0U);

    case TR_EVENT_LOW_BATTERY:
        state->low_battery = true;
        if (state->mode == TR_MODE_ACTIVE && state->operation == TR_OP_NONE) {
            state->operation = TR_OP_LOW_BATTERY_FEEDBACK;
            state->camera_enabled = false;
            show_scenes(state, TR_LEFT_LOW_BATTERY, base_right_scene(state), true);
        }
        return applied(state, TR_COMMAND_NONE, 0U);

    default:
        break;
    }

    if (state->mode == TR_MODE_SLEEPING || state->mode == TR_MODE_BOOTING) {
        return make_result(state, TR_STATUS_REJECTED_PRECONDITION, TR_COMMAND_NONE, 0U);
    }

    /* ACKs are accepted only by the exact pending transaction token. */
    switch (event->type) {
    case TR_EVENT_ADD_ACK_SUCCESS:
    case TR_EVENT_ADD_ACK_FAILURE:
        if (!ack_matches(state, event, TR_OP_ADD_WAIT_ACK)) {
            return make_result(state, TR_STATUS_STALE_TOKEN, TR_COMMAND_NONE, 0U);
        }
        cancel_pending_command(state);
        if (event->type == TR_EVENT_ADD_ACK_FAILURE) {
            state->operation = TR_OP_NONE;
            restore_base_scenes(state);
            return applied(state, TR_COMMAND_NONE, 0U);
        }
        state->meal_item_count += 1U;
        state->operation = TR_OP_ADD_FEEDBACK;
        set_base_left(state, TR_LEFT_EYE);
        state->current_weight_positive = false;
        state->current_weight_stable = false;
        state->camera_enabled = false;
        show_scenes(state, TR_LEFT_ADD_FEEDBACK, TR_RIGHT_ADD_FEEDBACK, true);
        return applied(state, TR_COMMAND_NONE, 0U);

    case TR_EVENT_TARE_ACK_SUCCESS:
    case TR_EVENT_TARE_ACK_FAILURE:
        if (!ack_matches(state, event, TR_OP_TARE_WAIT_ACK)) {
            return make_result(state, TR_STATUS_STALE_TOKEN, TR_COMMAND_NONE, 0U);
        }
        cancel_pending_command(state);
        if (event->type == TR_EVENT_TARE_ACK_FAILURE) {
            state->operation = TR_OP_NONE;
            restore_base_scenes(state);
            return applied(state, TR_COMMAND_NONE, 0U);
        }
        state->operation = TR_OP_TARE_FEEDBACK;
        set_base_left(state, TR_LEFT_EYE);
        state->current_weight_positive = false;
        state->current_weight_stable = false;
        state->camera_enabled = false;
        show_scenes(state, TR_LEFT_TARE_SUCCESS, base_right_scene(state), true);
        return applied(state, TR_COMMAND_NONE, 0U);

    case TR_EVENT_FINISH_ACK_SUCCESS:
    case TR_EVENT_FINISH_ACK_FAILURE:
        if (!ack_matches(state, event, TR_OP_FINISH_WAIT_ACK)) {
            return make_result(state, TR_STATUS_STALE_TOKEN, TR_COMMAND_NONE, 0U);
        }
        cancel_pending_command(state);
        if (event->type == TR_EVENT_FINISH_ACK_FAILURE) {
            state->operation = TR_OP_NONE;
            restore_base_scenes(state);
            return applied(state, TR_COMMAND_NONE, 0U);
        }
        state->completed_item_count = state->meal_item_count;
        state->operation = TR_OP_FINISH_FEEDBACK;
        set_base_left(state, TR_LEFT_EYE);
        state->current_weight_positive = false;
        state->current_weight_stable = false;
        state->camera_enabled = false;
        show_scenes(state, TR_LEFT_FINISH_SUCCESS, TR_RIGHT_FINISH_SUCCESS, true);
        return applied(state, TR_COMMAND_NONE, 0U);

    default:
        break;
    }

    /* Ordinary content events never interrupt an active animation/transaction. */
    if (state->operation != TR_OP_NONE &&
        event->type != TR_EVENT_FINISH_HOLD_CANCEL &&
        event->type != TR_EVENT_FINISH_REQUEST) {
        return make_result(state, TR_STATUS_IGNORED, TR_COMMAND_NONE, 0U);
    }

    switch (event->type) {
    case TR_EVENT_FIRST_WEIGHT_STABLE:
        if (state->base_left_scene != TR_LEFT_EYE ||
            !state->current_weight_positive ||
            !state->current_weight_stable) {
            return make_result(state, TR_STATUS_REJECTED_PRECONDITION, TR_COMMAND_NONE, 0U);
        }
        set_base_left(state, TR_LEFT_FIRST_WEIGHT_GUIDE);
        restore_base_scenes(state);
        return applied(state, TR_COMMAND_NONE, 0U);

    case TR_EVENT_FIRST_GUIDE_TIMEOUT:
        if (!state->first_guide_active) {
            return make_result(state, TR_STATUS_REJECTED_PRECONDITION, TR_COMMAND_NONE, 0U);
        }
        set_base_left(state, TR_LEFT_EYE);
        restore_base_scenes(state);
        return applied(state, TR_COMMAND_NONE, 0U);

    case TR_EVENT_AI_RESULT_STABLE:
    case TR_EVENT_APP_FOOD_OVERRIDE:
        set_base_left(state, TR_LEFT_FOOD);
        restore_base_scenes(state);
        return applied(state, TR_COMMAND_NONE, 0U);

    case TR_EVENT_OPEN_CANDIDATES:
        if (!state->food_active || state->candidates_open) {
            return make_result(state, TR_STATUS_REJECTED_PRECONDITION, TR_COMMAND_NONE, 0U);
        }
        set_base_left(state, TR_LEFT_CANDIDATES);
        restore_base_scenes(state);
        return applied(state, TR_COMMAND_NONE, 0U);

    case TR_EVENT_SELECT_CANDIDATE:
        if (!state->candidates_open) {
            return make_result(state, TR_STATUS_REJECTED_PRECONDITION, TR_COMMAND_NONE, 0U);
        }
        set_base_left(state, TR_LEFT_FOOD);
        restore_base_scenes(state);
        return applied(state, TR_COMMAND_NONE, 0U);

    case TR_EVENT_ADD_REQUEST:
        if (!tr_can_add(state)) {
            return make_result(state, TR_STATUS_REJECTED_PRECONDITION, TR_COMMAND_NONE, 0U);
        }
        token = issue_command_token(state);
        state->operation = TR_OP_ADD_WAIT_ACK;
        state->camera_enabled = false;
        return applied(state, TR_COMMAND_COMMIT_ADD, token);

    case TR_EVENT_TARE_REQUEST:
        if (!event->qualified || !state->current_weight_positive ||
            state->operation != TR_OP_NONE) {
            return make_result(state, TR_STATUS_REJECTED_PRECONDITION, TR_COMMAND_NONE, 0U);
        }
        token = issue_command_token(state);
        state->operation = TR_OP_TARE_WAIT_ACK;
        state->camera_enabled = false;
        return applied(state, TR_COMMAND_SET_TARE, token);

    case TR_EVENT_FINISH_HOLD_BEGIN:
        if (!tr_can_finish(state)) {
            return make_result(state, TR_STATUS_REJECTED_PRECONDITION, TR_COMMAND_NONE, 0U);
        }
        state->operation = TR_OP_FINISH_HOLD;
        state->camera_enabled = false;
        show_scenes(state, state->base_left_scene, TR_RIGHT_FINISH_PROGRESS, true);
        return applied(state, TR_COMMAND_NONE, 0U);

    case TR_EVENT_FINISH_HOLD_CANCEL:
        if (state->operation != TR_OP_FINISH_HOLD) {
            return make_result(state, TR_STATUS_REJECTED_PRECONDITION, TR_COMMAND_NONE, 0U);
        }
        state->operation = TR_OP_NONE;
        restore_base_scenes(state);
        return applied(state, TR_COMMAND_NONE, 0U);

    case TR_EVENT_FINISH_REQUEST:
        if (state->operation != TR_OP_FINISH_HOLD || state->meal_item_count == 0U ||
            state->current_weight_positive) {
            return make_result(state, TR_STATUS_REJECTED_PRECONDITION, TR_COMMAND_NONE, 0U);
        }
        token = issue_command_token(state);
        state->operation = TR_OP_FINISH_WAIT_ACK;
        return applied(state, TR_COMMAND_COMMIT_FINISH, token);

    default:
        return make_result(state, TR_STATUS_IGNORED, TR_COMMAND_NONE, 0U);
    }
}

bool tr_state_is_valid(const tr_state_t *state)
{
    bool pending_expected;

    if (state == NULL ||
        state->mode > TR_MODE_SLEEPING ||
        state->left_scene >= TR_LEFT_SCENE_COUNT ||
        state->right_scene >= TR_RIGHT_SCENE_COUNT ||
        state->base_left_scene < TR_LEFT_EYE ||
        state->base_left_scene > TR_LEFT_FIRST_WEIGHT_GUIDE ||
        (state->goal_valid && !state->app_session_ready) ||
        (state->candidates_open && !state->food_active)) {
        return false;
    }

    switch (state->base_left_scene) {
    case TR_LEFT_EYE:
        if (state->food_active || state->candidates_open || state->first_guide_active) return false;
        break;
    case TR_LEFT_FOOD:
        if (!state->food_active || state->candidates_open || state->first_guide_active) return false;
        break;
    case TR_LEFT_CANDIDATES:
        if (!state->food_active || !state->candidates_open || state->first_guide_active) return false;
        break;
    case TR_LEFT_FIRST_WEIGHT_GUIDE:
        if (state->food_active || state->candidates_open || !state->first_guide_active) return false;
        break;
    default:
        return false;
    }

    pending_expected = state->operation == TR_OP_ADD_WAIT_ACK ||
                       state->operation == TR_OP_TARE_WAIT_ACK ||
                       state->operation == TR_OP_FINISH_WAIT_ACK;
    if ((state->pending_command_token != 0U) != pending_expected) {
        return false;
    }

    if (state->mode == TR_MODE_POWER_OFF) {
        return state->operation == TR_OP_NONE &&
               state->left_scene == TR_LEFT_OFF &&
               state->right_scene == TR_RIGHT_OFF &&
               !state->camera_enabled && !state->wifi_enabled &&
               !state->touch_monitoring_enabled && !state->weight_monitoring_enabled;
    }

    if (state->mode == TR_MODE_SLEEPING) {
        return state->operation == TR_OP_NONE &&
               state->left_scene == TR_LEFT_OFF &&
               state->right_scene == TR_RIGHT_OFF &&
               !state->camera_enabled && !state->wifi_enabled &&
               state->touch_monitoring_enabled && state->weight_monitoring_enabled;
    }

    if (state->mode == TR_MODE_BOOTING) {
        return state->operation == TR_OP_BOOT_ANIMATION &&
               state->left_scene == TR_LEFT_BOOT &&
               state->right_scene == TR_RIGHT_BOOT;
    }

    switch (state->operation) {
    case TR_OP_NONE:
    case TR_OP_ADD_WAIT_ACK:
    case TR_OP_TARE_WAIT_ACK:
        return state->left_scene == state->base_left_scene &&
               state->right_scene == base_right_scene(state);
    case TR_OP_WAKE_ANIMATION:
        return state->left_scene == TR_LEFT_WAKE && state->right_scene == TR_RIGHT_WAKE;
    case TR_OP_SLEEP_FADE:
    case TR_OP_POWER_OFF_FADE:
        return state->left_scene == TR_LEFT_FADE_TO_BLACK &&
               state->right_scene == TR_RIGHT_FADE_TO_BLACK;
    case TR_OP_BLE_FEEDBACK:
        return state->left_scene == TR_LEFT_BLE_SUCCESS &&
               state->right_scene == TR_RIGHT_BLE_SUCCESS;
    case TR_OP_ADD_FEEDBACK:
        return state->left_scene == TR_LEFT_ADD_FEEDBACK &&
               state->right_scene == TR_RIGHT_ADD_FEEDBACK;
    case TR_OP_TARE_FEEDBACK:
        return state->left_scene == TR_LEFT_TARE_SUCCESS &&
               state->right_scene == base_right_scene(state);
    case TR_OP_FINISH_HOLD:
    case TR_OP_FINISH_WAIT_ACK:
        return state->left_scene == state->base_left_scene &&
               state->right_scene == TR_RIGHT_FINISH_PROGRESS;
    case TR_OP_FINISH_FEEDBACK:
        return state->left_scene == TR_LEFT_FINISH_SUCCESS &&
               state->right_scene == TR_RIGHT_FINISH_SUCCESS;
    case TR_OP_LOW_BATTERY_FEEDBACK:
        return state->left_scene == TR_LEFT_LOW_BATTERY &&
               state->right_scene == base_right_scene(state);
    case TR_OP_OVERLOAD:
        return state->overloaded && state->left_scene == TR_LEFT_OVERLOAD &&
               state->right_scene == TR_RIGHT_OVERLOAD;
    case TR_OP_CRITICAL_BATTERY:
        return state->critical_battery && state->left_scene == TR_LEFT_CRITICAL_BATTERY &&
               state->right_scene == TR_RIGHT_CRITICAL_BATTERY;
    case TR_OP_CHARGING:
        return state->charger_connected && state->left_scene == TR_LEFT_CHARGING &&
               state->right_scene == TR_RIGHT_CHARGING;
    case TR_OP_BOOT_ANIMATION:
    default:
        return false;
    }
}
