#ifndef CALORIE_SCALE_TRANSITION_RULES_H
#define CALORIE_SCALE_TRANSITION_RULES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This module owns behavior only.  It never draws and never knows LVGL.
 * A screen receives exactly one scene enum at a time; composing EYE and FOOD
 * simultaneously is therefore impossible when the renderer follows this API.
 */

typedef enum {
    TR_MODE_POWER_OFF = 0,
    TR_MODE_BOOTING,
    TR_MODE_ACTIVE,
    TR_MODE_SLEEPING
} tr_mode_t;

typedef enum {
    TR_LEFT_OFF = 0,
    TR_LEFT_BOOT,
    TR_LEFT_WAKE,
    TR_LEFT_EYE,
    TR_LEFT_FOOD,
    TR_LEFT_CANDIDATES,
    TR_LEFT_FIRST_WEIGHT_GUIDE,
    TR_LEFT_ADD_FEEDBACK,
    TR_LEFT_BLE_SUCCESS,
    TR_LEFT_TARE_SUCCESS,
    TR_LEFT_FINISH_SUCCESS,
    TR_LEFT_OVERLOAD,
    TR_LEFT_LOW_BATTERY,
    TR_LEFT_CRITICAL_BATTERY,
    TR_LEFT_CHARGING,
    TR_LEFT_FADE_TO_BLACK,
    TR_LEFT_SCENE_COUNT
} tr_left_scene_t;

typedef enum {
    TR_RIGHT_OFF = 0,
    TR_RIGHT_BOOT,
    TR_RIGHT_WAKE,
    TR_RIGHT_PLUS,
    TR_RIGHT_PLUS_WITH_GOAL,
    TR_RIGHT_ADD_FEEDBACK,
    TR_RIGHT_FINISH_PROGRESS,
    TR_RIGHT_FINISH_SUCCESS,
    TR_RIGHT_BLE_SUCCESS,
    TR_RIGHT_OVERLOAD,
    TR_RIGHT_CRITICAL_BATTERY,
    TR_RIGHT_CHARGING,
    TR_RIGHT_FADE_TO_BLACK,
    TR_RIGHT_SCENE_COUNT
} tr_right_scene_t;

typedef enum {
    TR_IDENTITY_NONE = 0,
    TR_IDENTITY_EYE,
    TR_IDENTITY_FOOD,
    TR_IDENTITY_CANDIDATES,
    TR_IDENTITY_STATUS
} tr_left_identity_t;

typedef enum {
    TR_OP_NONE = 0,
    TR_OP_BOOT_ANIMATION,
    TR_OP_WAKE_ANIMATION,
    TR_OP_SLEEP_FADE,
    TR_OP_POWER_OFF_FADE,
    TR_OP_BLE_FEEDBACK,
    TR_OP_ADD_WAIT_ACK,
    TR_OP_ADD_FEEDBACK,
    TR_OP_TARE_WAIT_ACK,
    TR_OP_TARE_FEEDBACK,
    TR_OP_FINISH_HOLD,
    TR_OP_FINISH_WAIT_ACK,
    TR_OP_FINISH_FEEDBACK,
    TR_OP_LOW_BATTERY_FEEDBACK,
    TR_OP_OVERLOAD,
    TR_OP_CRITICAL_BATTERY,
    TR_OP_CHARGING
} tr_operation_t;

typedef enum {
    TR_WAKE_NONE = 0,
    TR_WAKE_PLUS,
    TR_WAKE_TOUCH,
    TR_WAKE_UNIT_KEY,
    TR_WAKE_WEIGHT_CHANGE
} tr_wake_source_t;

typedef enum {
    TR_EVENT_POWER_ON = 0,
    TR_EVENT_POWER_OFF,
    TR_EVENT_ANIMATION_FINISHED,
    TR_EVENT_SLEEP_TIMEOUT,
    TR_EVENT_WAKE_REQUEST,

    TR_EVENT_WEIGHT_STATUS,
    TR_EVENT_FIRST_WEIGHT_STABLE,
    TR_EVENT_FIRST_GUIDE_TIMEOUT,
    TR_EVENT_AI_RESULT_STABLE,
    TR_EVENT_OPEN_CANDIDATES,
    TR_EVENT_SELECT_CANDIDATE,
    TR_EVENT_APP_FOOD_OVERRIDE,

    TR_EVENT_BLE_LINK_UP,
    TR_EVENT_APP_SESSION_READY,
    TR_EVENT_BLE_DISCONNECTED,

    TR_EVENT_ADD_REQUEST,
    TR_EVENT_ADD_ACK_SUCCESS,
    TR_EVENT_ADD_ACK_FAILURE,
    TR_EVENT_FINISH_HOLD_BEGIN,
    TR_EVENT_FINISH_HOLD_CANCEL,
    TR_EVENT_FINISH_REQUEST,
    TR_EVENT_FINISH_ACK_SUCCESS,
    TR_EVENT_FINISH_ACK_FAILURE,
    TR_EVENT_TARE_REQUEST,
    TR_EVENT_TARE_ACK_SUCCESS,
    TR_EVENT_TARE_ACK_FAILURE,

    TR_EVENT_LOW_BATTERY,
    TR_EVENT_CRITICAL_BATTERY,
    TR_EVENT_CHARGER_CONNECTED,
    TR_EVENT_CHARGER_DISCONNECTED,
    TR_EVENT_OVERLOAD_ENTER,
    TR_EVENT_OVERLOAD_CLEAR,

    TR_EVENT_COUNT
} tr_event_type_t;

typedef enum {
    TR_STATUS_APPLIED = 0,
    TR_STATUS_IGNORED,
    TR_STATUS_REJECTED_PRECONDITION,
    TR_STATUS_STALE_TOKEN
} tr_status_t;

typedef enum {
    TR_COMMAND_NONE = 0,
    TR_COMMAND_COMMIT_ADD,
    TR_COMMAND_COMMIT_FINISH,
    TR_COMMAND_SET_TARE
} tr_command_t;

typedef struct {
    tr_event_type_t type;

    /* Required by ACK events and ANIMATION_FINISHED. */
    uint32_t token;

    /* WAKE_REQUEST. */
    tr_wake_source_t wake_source;

    /* WEIGHT_STATUS. */
    bool weight_positive;
    bool weight_stable;

    /* TARE_REQUEST: the gesture/baseline engine has fully qualified it. */
    bool qualified;

    /* APP_SESSION_READY: a validated daily target is present. */
    bool goal_valid;
} tr_event_t;

typedef struct {
    bool valid;
    tr_left_scene_t base_left;
    bool food_active;
    bool candidates_open;
    bool first_guide_active;
    bool current_weight_positive;
    bool current_weight_stable;
    bool camera_enabled;
    bool wifi_enabled;
    uint32_t meal_item_count;
} tr_sleep_snapshot_t;

typedef struct {
    tr_mode_t mode;
    tr_operation_t operation;

    tr_left_scene_t left_scene;
    tr_right_scene_t right_scene;
    tr_left_scene_t base_left_scene;

    bool food_active;
    bool candidates_open;
    bool first_guide_active;
    bool current_weight_positive;
    bool current_weight_stable;

    bool ble_link_up;
    bool app_session_ready;
    bool goal_valid;
    bool low_battery;
    bool critical_battery;
    bool charger_connected;
    bool overloaded;

    bool camera_enabled;
    bool wifi_enabled;
    bool touch_monitoring_enabled;
    bool weight_monitoring_enabled;

    uint32_t meal_item_count;
    uint32_t completed_item_count;

    /* Monotonic guards: ACKs and animation callbacks must echo their token. */
    uint32_t revision;
    uint32_t scene_token;
    uint32_t pending_command_token;
    uint32_t next_command_token;

    tr_sleep_snapshot_t sleep_snapshot;
} tr_state_t;

typedef struct {
    tr_status_t status;
    tr_command_t command;
    uint32_t command_token;
    uint32_t revision;
    uint32_t scene_token;
} tr_result_t;

void tr_state_init(tr_state_t *state);
tr_event_t tr_event_make(tr_event_type_t type);
tr_result_t tr_dispatch(tr_state_t *state, const tr_event_t *event);

bool tr_can_add(const tr_state_t *state);
bool tr_can_finish(const tr_state_t *state);
bool tr_state_is_valid(const tr_state_t *state);
tr_left_identity_t tr_left_identity(tr_left_scene_t scene);
int tr_event_priority(tr_event_type_t type);
int tr_operation_priority(tr_operation_t operation);

#ifdef __cplusplus
}
#endif

#endif
