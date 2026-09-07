#include "tare_engine.h"

#include <math.h>
#include <string.h>

#define TARE_HISTORY_COVERAGE_SLACK_MS 25U

static void history_clear(tare_engine_t *engine)
{
    engine->history_count = 0U;
    engine->history_head = 0U;
}

static void history_push(tare_engine_t *engine, float grams, uint32_t now_ms)
{
    engine->history[engine->history_head].at_ms = now_ms;
    engine->history[engine->history_head].grams = grams;
    engine->history_head = (engine->history_head + 1U) % TARE_WEIGHT_HISTORY;
    if(engine->history_count < TARE_WEIGHT_HISTORY) {
        engine->history_count++;
    }
}

static bool history_window_stats(
    const tare_engine_t *engine,
    uint32_t now_ms,
    uint32_t window_ms,
    float *mean_out,
    float *range_out)
{
    float min_value = 0.0f;
    float max_value = 0.0f;
    float sum = 0.0f;
    size_t count = 0U;
    uint32_t oldest_ms = now_ms;

    for(size_t i = 0U; i < engine->history_count; ++i) {
        const size_t index =
            (engine->history_head + TARE_WEIGHT_HISTORY - 1U - i) % TARE_WEIGHT_HISTORY;
        const tare_weight_sample_t *sample = &engine->history[index];
        if(now_ms - sample->at_ms > window_ms) {
            break;
        }

        if(count == 0U) {
            min_value = sample->grams;
            max_value = sample->grams;
        }
        else {
            if(sample->grams < min_value) min_value = sample->grams;
            if(sample->grams > max_value) max_value = sample->grams;
        }

        sum += sample->grams;
        oldest_ms = sample->at_ms;
        ++count;
    }

    /* Accept one normal 20 ms sample interval around the exact time boundary. */
    if(count < 3U || now_ms - oldest_ms + TARE_HISTORY_COVERAGE_SLACK_MS < window_ms) {
        return false;
    }

    if(mean_out != NULL) *mean_out = sum / (float)count;
    if(range_out != NULL) *range_out = max_value - min_value;
    return true;
}

static void cancel(tare_engine_t *engine, tare_reason_t reason)
{
    engine->state = TARE_STATE_CANCELLED;
    engine->reason = reason;
    engine->touch_active = false;
}

static void prepare_next_attempt(tare_engine_t *engine)
{
    /* Keep the fresh idle history collected after the previous terminal result. */
    engine->state = TARE_STATE_IDLE;
    engine->reason = TARE_REASON_NONE;
    engine->baseline_valid = false;
    engine->touch_active = false;
    engine->start_pad = 0U;
    engine->last_pad = 0U;
    engine->visited_mask = 0U;
    engine->distinct_pad_count = 0U;
    engine->wrong_direction = false;
    engine->non_adjacent = false;
    engine->touch_started_ms = 0U;
    engine->last_new_pad_ms = 0U;
    engine->released_ms = 0U;
}

tare_config_t tare_default_config(void)
{
    const tare_config_t config = {
        .pre_stable_ms = 450U,
        .min_gesture_ms = 120U,
        .max_gesture_ms = 650U,
        .two_pad_endpoint_dwell_ms = 160U,
        .release_guard_ms = 280U,
        .post_stable_ms = 360U,
        .settle_timeout_ms = 1800U,
        .stable_range_g = 1.2f,
        .return_tolerance_g = 2.5f,
        .changed_weight_g = 8.0f,
        .min_loaded_weight_g = 5.0f,
    };
    return config;
}

void tare_engine_init(tare_engine_t *engine, const tare_config_t *config)
{
    if(engine == NULL) return;
    memset(engine, 0, sizeof(*engine));
    engine->config = config != NULL ? *config : tare_default_config();
    engine->state = TARE_STATE_IDLE;
}

void tare_engine_reset(tare_engine_t *engine)
{
    if(engine == NULL) return;
    const tare_config_t config = engine->config;
    const float live_weight_g = engine->live_weight_g;
    memset(engine, 0, sizeof(*engine));
    engine->config = config;
    engine->live_weight_g = live_weight_g;
    engine->state = TARE_STATE_IDLE;
}

bool tare_engine_baseline_is_stable(const tare_engine_t *engine, uint32_t now_ms)
{
    float range_g = 0.0f;
    if(engine == NULL) return false;
    if(!history_window_stats(
           engine,
           now_ms,
           engine->config.pre_stable_ms,
           NULL,
           &range_g)) {
        return false;
    }
    return range_g <= engine->config.stable_range_g;
}

void tare_engine_feed_weight(tare_engine_t *engine, float grams, uint32_t now_ms)
{
    float delta_g;
    uint32_t elapsed_ms;

    if(engine == NULL) return;
    engine->live_weight_g = grams;

    if(engine->touch_active) {
        /* Never let finger pressure contaminate either stability window. */
        delta_g = fabsf(grams - engine->baseline_g);
        if(delta_g > engine->touch_peak_delta_g) {
            engine->touch_peak_delta_g = delta_g;
        }
        return;
    }

    if(engine->state == TARE_STATE_SETTLING) {
        elapsed_ms = now_ms - engine->released_ms;

        /* Mechanical rebound and lingering touch force live only in this guard. */
        if(elapsed_ms < engine->config.release_guard_ms) return;

        delta_g = fabsf(grams - engine->baseline_g);
        if(delta_g > engine->config.changed_weight_g) {
            cancel(engine, TARE_REASON_WEIGHT_CHANGED);
            return;
        }

        if(delta_g <= engine->config.return_tolerance_g) {
            float mean_g = 0.0f;
            float range_g = 0.0f;
            history_push(engine, grams, now_ms);

            if(history_window_stats(
                   engine,
                   now_ms,
                   engine->config.post_stable_ms,
                   &mean_g,
                   &range_g) &&
               range_g <= engine->config.stable_range_g) {
                engine->qualified_weight_g = mean_g;
                engine->state = TARE_STATE_REQUESTED;
                engine->reason = TARE_REASON_READY;
                return;
            }
        }
        else {
            history_clear(engine);
        }

        if(elapsed_ms >= engine->config.settle_timeout_ms) {
            cancel(engine, TARE_REASON_SETTLE_TIMEOUT);
        }
        return;
    }

    if(engine->state == TARE_STATE_IDLE ||
       engine->state == TARE_STATE_CANCELLED ||
       engine->state == TARE_STATE_SUCCESS) {
        history_push(engine, grams, now_ms);
    }
}

bool tare_engine_touch_begin(tare_engine_t *engine, uint8_t pad, uint32_t now_ms)
{
    float baseline_g = 0.0f;
    float range_g = 0.0f;

    if(engine == NULL || pad < 1U || pad > 4U) return false;

    if(engine->state == TARE_STATE_CANCELLED || engine->state == TARE_STATE_SUCCESS) {
        prepare_next_attempt(engine);
    }
    if(engine->state != TARE_STATE_IDLE) return false;

    if(!history_window_stats(
           engine,
           now_ms,
           engine->config.pre_stable_ms,
           &baseline_g,
           &range_g) ||
       range_g > engine->config.stable_range_g) {
        engine->reason = TARE_REASON_BASELINE_UNSTABLE;
        return false;
    }

    if(baseline_g < engine->config.min_loaded_weight_g) {
        engine->reason = TARE_REASON_NO_LOAD;
        return false;
    }

    engine->baseline_g = baseline_g;
    engine->baseline_valid = true;
    engine->frozen_display_g = baseline_g;
    engine->qualified_weight_g = 0.0f;
    engine->touch_peak_delta_g = 0.0f;
    engine->touch_active = true;
    engine->start_pad = pad;
    engine->last_pad = pad;
    engine->visited_mask = (uint8_t)(1U << (pad - 1U));
    engine->distinct_pad_count = 1U;
    engine->wrong_direction = false;
    engine->non_adjacent = false;
    engine->touch_started_ms = now_ms;
    engine->last_new_pad_ms = now_ms;
    engine->state = TARE_STATE_TRACKING;
    engine->reason = TARE_REASON_NONE;
    return true;
}

void tare_engine_touch_pad(tare_engine_t *engine, uint8_t pad, uint32_t now_ms)
{
    int delta;
    uint8_t mask;

    if(engine == NULL || !engine->touch_active || engine->state != TARE_STATE_TRACKING) {
        return;
    }
    if(pad < 1U || pad > 4U || pad == engine->last_pad) return;

    delta = (int)pad - (int)engine->last_pad;
    if(delta > 0) {
        engine->wrong_direction = true;
    }
    else if(delta < -1) {
        engine->non_adjacent = true;
    }

    mask = (uint8_t)(1U << (pad - 1U));
    if((engine->visited_mask & mask) == 0U) {
        engine->visited_mask |= mask;
        ++engine->distinct_pad_count;
        engine->last_new_pad_ms = now_ms;
    }
    engine->last_pad = pad;
}

bool tare_engine_touch_end(tare_engine_t *engine, uint32_t now_ms)
{
    uint32_t duration_ms;

    if(engine == NULL || !engine->touch_active || engine->state != TARE_STATE_TRACKING) {
        return false;
    }
    engine->touch_active = false;
    duration_ms = now_ms - engine->touch_started_ms;

    if(engine->wrong_direction) {
        cancel(engine, TARE_REASON_WRONG_DIRECTION);
        return false;
    }
    if(engine->non_adjacent) {
        cancel(engine, TARE_REASON_NON_ADJACENT);
        return false;
    }
    if(duration_ms < engine->config.min_gesture_ms) {
        cancel(engine, TARE_REASON_TOO_SHORT);
        return false;
    }
    if(duration_ms > engine->config.max_gesture_ms) {
        cancel(engine, TARE_REASON_TOO_LONG);
        return false;
    }
    if(engine->distinct_pad_count < 2U) {
        cancel(engine, TARE_REASON_NOT_ENOUGH_PADS);
        return false;
    }

    if(engine->distinct_pad_count == 2U) {
        const bool adjacent = engine->start_pad == (uint8_t)(engine->last_pad + 1U);
        const uint32_t endpoint_dwell_ms = now_ms - engine->last_new_pad_ms;
        if(!adjacent || endpoint_dwell_ms < engine->config.two_pad_endpoint_dwell_ms) {
            cancel(engine, TARE_REASON_TWO_PAD_NO_DWELL);
            return false;
        }
    }

    engine->released_ms = now_ms;
    history_clear(engine);
    engine->state = TARE_STATE_SETTLING;
    engine->reason = TARE_REASON_NONE;
    return true;
}

void tare_engine_acknowledge(tare_engine_t *engine, uint32_t now_ms)
{
    (void)now_ms;
    if(engine == NULL || engine->state != TARE_STATE_REQUESTED) return;
    engine->state = TARE_STATE_SUCCESS;
    engine->reason = TARE_REASON_ACKNOWLEDGED;
}

bool tare_engine_is_terminal(const tare_engine_t *engine)
{
    return engine != NULL &&
           (engine->state == TARE_STATE_SUCCESS || engine->state == TARE_STATE_CANCELLED);
}

const char *tare_state_name(tare_state_t state)
{
    switch(state) {
        case TARE_STATE_IDLE: return "IDLE";
        case TARE_STATE_TRACKING: return "TRACKING";
        case TARE_STATE_SETTLING: return "SETTLING";
        case TARE_STATE_REQUESTED: return "REQUESTED";
        case TARE_STATE_SUCCESS: return "SUCCESS";
        case TARE_STATE_CANCELLED: return "CANCELLED";
        default: return "UNKNOWN";
    }
}

const char *tare_reason_name(tare_reason_t reason)
{
    switch(reason) {
        case TARE_REASON_NONE: return "NONE";
        case TARE_REASON_NO_LOAD: return "NO_LOAD";
        case TARE_REASON_BASELINE_UNSTABLE: return "BASELINE_UNSTABLE";
        case TARE_REASON_TOO_SHORT: return "TOO_SHORT";
        case TARE_REASON_TOO_LONG: return "TOO_LONG";
        case TARE_REASON_NOT_ENOUGH_PADS: return "NOT_ENOUGH_PADS";
        case TARE_REASON_TWO_PAD_NO_DWELL: return "TWO_PAD_NO_DWELL";
        case TARE_REASON_WRONG_DIRECTION: return "WRONG_DIRECTION";
        case TARE_REASON_NON_ADJACENT: return "NON_ADJACENT";
        case TARE_REASON_WEIGHT_CHANGED: return "WEIGHT_CHANGED";
        case TARE_REASON_SETTLE_TIMEOUT: return "SETTLE_TIMEOUT";
        case TARE_REASON_READY: return "READY";
        case TARE_REASON_ACKNOWLEDGED: return "ACKNOWLEDGED";
        default: return "UNKNOWN";
    }
}
