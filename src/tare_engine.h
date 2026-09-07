#ifndef CALORIE_SCALE_TARE_ENGINE_H
#define CALORIE_SCALE_TARE_ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 40 samples cover the 450 ms pre-touch window at the intended 20–50 Hz. */
#define TARE_WEIGHT_HISTORY 40U

typedef enum {
    TARE_STATE_IDLE = 0,
    TARE_STATE_TRACKING,
    TARE_STATE_SETTLING,
    TARE_STATE_REQUESTED,
    TARE_STATE_SUCCESS,
    TARE_STATE_CANCELLED
} tare_state_t;

typedef enum {
    TARE_REASON_NONE = 0,
    TARE_REASON_NO_LOAD,
    TARE_REASON_BASELINE_UNSTABLE,
    TARE_REASON_TOO_SHORT,
    TARE_REASON_TOO_LONG,
    TARE_REASON_NOT_ENOUGH_PADS,
    TARE_REASON_TWO_PAD_NO_DWELL,
    TARE_REASON_WRONG_DIRECTION,
    TARE_REASON_NON_ADJACENT,
    TARE_REASON_WEIGHT_CHANGED,
    TARE_REASON_SETTLE_TIMEOUT,
    TARE_REASON_READY,
    TARE_REASON_ACKNOWLEDGED
} tare_reason_t;

typedef struct {
    /* Bench-start values; the real enclosure/load-cell assembly must tune them. */
    uint32_t pre_stable_ms;
    uint32_t min_gesture_ms;
    uint32_t max_gesture_ms;
    uint32_t two_pad_endpoint_dwell_ms;
    uint32_t release_guard_ms;
    uint32_t post_stable_ms;
    uint32_t settle_timeout_ms;
    float stable_range_g;
    float return_tolerance_g;
    float changed_weight_g;
    float min_loaded_weight_g;
} tare_config_t;

typedef struct {
    uint32_t at_ms;
    float grams;
} tare_weight_sample_t;

typedef struct {
    tare_config_t config;
    tare_state_t state;
    tare_reason_t reason;

    tare_weight_sample_t history[TARE_WEIGHT_HISTORY];
    size_t history_count;
    size_t history_head;

    float live_weight_g;
    float baseline_g;
    float frozen_display_g;
    float qualified_weight_g;
    float touch_peak_delta_g;
    bool baseline_valid;

    bool touch_active;
    uint8_t start_pad;
    uint8_t last_pad;
    uint8_t visited_mask;
    uint8_t distinct_pad_count;
    bool wrong_direction;
    bool non_adjacent;
    uint32_t touch_started_ms;
    uint32_t last_new_pad_ms;
    uint32_t released_ms;
} tare_engine_t;

tare_config_t tare_default_config(void);
void tare_engine_init(tare_engine_t *engine, const tare_config_t *config);
void tare_engine_reset(tare_engine_t *engine);

/*
 * Feed filtered load-cell samples with one monotonic millisecond clock.
 * While a finger is down, samples are deliberately excluded from all
 * stability decisions; touch pressure is diagnostics only.
 */
void tare_engine_feed_weight(tare_engine_t *engine, float grams, uint32_t now_ms);

/* PEDOT pads are numbered P1..P4 from left to right. */
bool tare_engine_touch_begin(tare_engine_t *engine, uint8_t pad, uint32_t now_ms);
void tare_engine_touch_pad(tare_engine_t *engine, uint8_t pad, uint32_t now_ms);
bool tare_engine_touch_end(tare_engine_t *engine, uint32_t now_ms);

/* Call only after the load-cell driver has actually accepted the zero point. */
void tare_engine_acknowledge(tare_engine_t *engine, uint32_t now_ms);

bool tare_engine_baseline_is_stable(const tare_engine_t *engine, uint32_t now_ms);
bool tare_engine_is_terminal(const tare_engine_t *engine);
const char *tare_state_name(tare_state_t state);
const char *tare_reason_name(tare_reason_t reason);

#ifdef __cplusplus
}
#endif

#endif
