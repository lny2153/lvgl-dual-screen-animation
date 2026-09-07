#ifndef CALORIE_SCALE_UI_VIEW_MODEL_H
#define CALORIE_SCALE_UI_VIEW_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#include "tokens.h"

typedef enum {
    UI_PRIMARY_EYES = 0,
    UI_PRIMARY_FOOD,
    UI_PRIMARY_PAIRING,
    UI_PRIMARY_GUIDE,
    UI_PRIMARY_PICKER,
    UI_PRIMARY_UPLOAD,
    UI_PRIMARY_ALERT,
    UI_PRIMARY_STATUS,
    UI_PRIMARY_CHARGING,
    UI_PRIMARY_OFF
} ui_primary_mode_t;

typedef enum {
    UI_RIGHT_PLUS_ONLY = 0,
    UI_RIGHT_PLUS_RING,
    UI_RIGHT_FINISH_HOLD,
    UI_RIGHT_UPLOAD,
    UI_RIGHT_SAFETY,
    UI_RIGHT_CHARGE,
    UI_RIGHT_OFF
} ui_right_mode_t;

typedef enum {
    UI_STATUS_CONNECTED = 0,
    UI_STATUS_TARE_DONE,
    UI_STATUS_RECORDED
} ui_status_kind_t;

typedef enum {
    UI_ALERT_OVERLOAD = 0,
    UI_ALERT_LOW_BATTERY,
    UI_ALERT_CRITICAL_BATTERY
} ui_alert_kind_t;

typedef enum {
    UI_UNIT_G = 0,
    UI_UNIT_OZ,
    UI_UNIT_LB_OZ,
    UI_UNIT_ML
} ui_weight_unit_t;

typedef enum {
    UI_TRANSITION_NONE = 0,
    UI_TRANSITION_BOOT,
    UI_TRANSITION_WAKE,
    UI_TRANSITION_SLEEP,
    UI_TRANSITION_RECOGNIZE,
    UI_TRANSITION_FOOD_OVERRIDE,
    UI_TRANSITION_ADD,
    UI_TRANSITION_CONNECTED,
    UI_TRANSITION_TARE_SUCCESS,
    UI_TRANSITION_RECORD_SUCCESS,
    UI_TRANSITION_PAIRING_ENTER,
    UI_TRANSITION_PAIRING_TIMEOUT,
    UI_TRANSITION_GUIDE_ENTER,
    UI_TRANSITION_GUIDE_TIMEOUT,
    UI_TRANSITION_GUIDE_TO_FOOD,
    UI_TRANSITION_NUTRIENT,
    UI_TRANSITION_UNIT,
    UI_TRANSITION_ALERT_ENTER,
    UI_TRANSITION_ALERT_EXIT,
    UI_TRANSITION_LOW_BATTERY,
    UI_TRANSITION_CHARGING_ENTER,
    UI_TRANSITION_CHARGING_EXIT,
    UI_TRANSITION_FINISH_UPLOAD,
    UI_TRANSITION_CRITICAL_SHUTDOWN
} ui_transition_t;

typedef struct {
    ui_primary_mode_t primary;
    ui_right_mode_t right;
    ui_nutrient_t nutrient;
    ui_weight_unit_t unit;
    ui_status_kind_t status;
    ui_alert_kind_t alert;

    double total_weight_g;
    double current_weight_g;
    double total_nutrient;
    double current_nutrient;
    double goal_ratio;
    double hold_ratio;
    int battery_percent;
    int charge_minutes;

    bool app_ready;
    bool has_food;
    bool has_committed_items;
    bool estimated_charge_time_valid;
    const char *food_name;
} ui_view_model_t;

typedef struct {
    ui_transition_t id;
    uint32_t t0_ms;
    uint32_t duration_ms;
    uint32_t revision;
    ui_view_model_t before;
    ui_view_model_t after;
} ui_transition_snapshot_t;

const char *ui_nutrient_name(ui_nutrient_t nutrient);
const char *ui_nutrient_unit(ui_nutrient_t nutrient);
const char *ui_weight_unit_name(ui_weight_unit_t unit);

#endif
