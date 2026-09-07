#include "tare_engine.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int failures = 0;

#define EXPECT_TRUE(value)                                                            \
    do {                                                                              \
        if(!(value)) {                                                                \
            printf("FAIL %s:%d expected true: %s\n", __FILE__, __LINE__, #value);   \
            ++failures;                                                               \
        }                                                                             \
    } while(0)

#define EXPECT_FALSE(value) EXPECT_TRUE(!(value))

#define EXPECT_EQ(actual, expected)                                                   \
    do {                                                                              \
        if((actual) != (expected)) {                                                  \
            printf("FAIL %s:%d expected %d, got %d\n",                              \
                   __FILE__, __LINE__, (int)(expected), (int)(actual));               \
            ++failures;                                                               \
        }                                                                             \
    } while(0)

#define EXPECT_NEAR(actual, expected, tolerance)                                      \
    do {                                                                              \
        if(fabsf((actual) - (expected)) > (tolerance)) {                              \
            printf("FAIL %s:%d expected %.3f +/- %.3f, got %.3f\n",                 \
                   __FILE__, __LINE__, (double)(expected), (double)(tolerance),       \
                   (double)(actual));                                                 \
            ++failures;                                                               \
        }                                                                             \
    } while(0)

static uint32_t feed_stable(
    tare_engine_t *engine,
    uint32_t start_ms,
    float grams)
{
    uint32_t now_ms = start_ms;
    for(int i = 0; i < 30; ++i) {
        const float noise_g =
            (i % 3 == 0) ? 0.20f : ((i % 3 == 1) ? -0.20f : 0.0f);
        tare_engine_feed_weight(engine, grams + noise_g, now_ms);
        now_ms += 20U;
    }
    return now_ms;
}

static void feed_after_release(
    tare_engine_t *engine,
    uint32_t released_ms,
    float grams,
    uint32_t through_ms)
{
    for(uint32_t offset_ms = 20U; offset_ms <= through_ms; offset_ms += 20U) {
        const float noise_g = (offset_ms % 40U == 0U) ? 0.15f : -0.15f;
        tare_engine_feed_weight(engine, grams + noise_g, released_ms + offset_ms);
    }
}

static uint32_t begin_loaded(tare_engine_t *engine, float grams, uint8_t pad)
{
    uint32_t now_ms;
    tare_engine_init(engine, NULL);
    now_ms = feed_stable(engine, 0U, grams);
    EXPECT_TRUE(tare_engine_touch_begin(engine, pad, now_ms));
    return now_ms;
}

static void test_default_thresholds_match_rehearsal_contract(void)
{
    const tare_config_t config = tare_default_config();
    EXPECT_EQ(config.pre_stable_ms, 450U);
    EXPECT_EQ(config.min_gesture_ms, 120U);
    EXPECT_EQ(config.max_gesture_ms, 650U);
    EXPECT_EQ(config.two_pad_endpoint_dwell_ms, 160U);
    EXPECT_EQ(config.release_guard_ms, 280U);
    EXPECT_EQ(config.post_stable_ms, 360U);
    EXPECT_EQ(config.settle_timeout_ms, 1800U);
    EXPECT_NEAR(config.stable_range_g, 1.2f, 0.001f);
    EXPECT_NEAR(config.return_tolerance_g, 2.5f, 0.001f);
    EXPECT_NEAR(config.changed_weight_g, 8.0f, 0.001f);
}

static void test_three_pad_swipe_ignores_all_touch_pressure(void)
{
    tare_engine_t engine;
    const uint32_t start_ms = begin_loaded(&engine, 128.0f, 4U);

    tare_engine_feed_weight(&engine, 150.0f, start_ms + 35U);
    tare_engine_touch_pad(&engine, 3U, start_ms + 80U);
    tare_engine_feed_weight(&engine, 93.0f, start_ms + 125U);
    tare_engine_touch_pad(&engine, 2U, start_ms + 170U);
    tare_engine_feed_weight(&engine, 161.0f, start_ms + 205U);
    EXPECT_TRUE(tare_engine_touch_end(&engine, start_ms + 240U));
    EXPECT_EQ(engine.state, TARE_STATE_SETTLING);
    EXPECT_TRUE(engine.touch_peak_delta_g > 30.0f);

    feed_after_release(&engine, start_ms + 240U, 128.0f, 760U);
    EXPECT_EQ(engine.state, TARE_STATE_REQUESTED);
    EXPECT_EQ(engine.reason, TARE_REASON_READY);
    EXPECT_NEAR(engine.qualified_weight_g, 128.0f, 0.25f);

    tare_engine_acknowledge(&engine, start_ms + 1100U);
    EXPECT_EQ(engine.state, TARE_STATE_SUCCESS);
}

static void test_two_pad_swipe_requires_natural_endpoint_dwell(void)
{
    tare_engine_t accepted;
    tare_engine_t rejected;
    uint32_t start_ms = begin_loaded(&accepted, 90.0f, 3U);

    tare_engine_touch_pad(&accepted, 2U, start_ms + 130U);
    EXPECT_TRUE(tare_engine_touch_end(&accepted, start_ms + 300U));
    EXPECT_EQ(accepted.state, TARE_STATE_SETTLING);

    start_ms = begin_loaded(&rejected, 90.0f, 3U);
    tare_engine_touch_pad(&rejected, 2U, start_ms + 100U);
    EXPECT_FALSE(tare_engine_touch_end(&rejected, start_ms + 230U));
    EXPECT_EQ(rejected.state, TARE_STATE_CANCELLED);
    EXPECT_EQ(rejected.reason, TARE_REASON_TWO_PAD_NO_DWELL);
}

static void test_single_pad_category_tap_never_tares(void)
{
    tare_engine_t engine;
    const uint32_t start_ms = begin_loaded(&engine, 128.0f, 1U);

    EXPECT_FALSE(tare_engine_touch_end(&engine, start_ms + 220U));
    EXPECT_EQ(engine.reason, TARE_REASON_NOT_ENOUGH_PADS);
}

static void test_empty_scale_swipe_is_wake_only(void)
{
    tare_engine_t engine;
    uint32_t now_ms;

    tare_engine_init(&engine, NULL);
    now_ms = feed_stable(&engine, 0U, 0.0f);
    EXPECT_FALSE(tare_engine_touch_begin(&engine, 4U, now_ms));
    EXPECT_EQ(engine.state, TARE_STATE_IDLE);
    EXPECT_EQ(engine.reason, TARE_REASON_NO_LOAD);
}

static void test_direction_reversal_is_rejected(void)
{
    tare_engine_t engine;
    const uint32_t start_ms = begin_loaded(&engine, 128.0f, 4U);

    tare_engine_touch_pad(&engine, 3U, start_ms + 80U);
    tare_engine_touch_pad(&engine, 4U, start_ms + 160U);
    tare_engine_touch_pad(&engine, 3U, start_ms + 220U);
    EXPECT_FALSE(tare_engine_touch_end(&engine, start_ms + 300U));
    EXPECT_EQ(engine.reason, TARE_REASON_WRONG_DIRECTION);
}

static void test_skipped_pad_is_rejected(void)
{
    tare_engine_t engine;
    const uint32_t start_ms = begin_loaded(&engine, 128.0f, 4U);

    tare_engine_touch_pad(&engine, 2U, start_ms + 100U);
    tare_engine_touch_pad(&engine, 1U, start_ms + 190U);
    EXPECT_FALSE(tare_engine_touch_end(&engine, start_ms + 280U));
    EXPECT_EQ(engine.reason, TARE_REASON_NON_ADJACENT);
}

static void test_long_press_and_too_fast_swipe_are_rejected(void)
{
    tare_engine_t engine;
    uint32_t start_ms = begin_loaded(&engine, 128.0f, 4U);

    tare_engine_touch_pad(&engine, 3U, start_ms + 30U);
    tare_engine_touch_pad(&engine, 2U, start_ms + 60U);
    EXPECT_FALSE(tare_engine_touch_end(&engine, start_ms + 100U));
    EXPECT_EQ(engine.reason, TARE_REASON_TOO_SHORT);

    start_ms = feed_stable(&engine, start_ms + 200U, 128.0f);
    EXPECT_TRUE(tare_engine_touch_begin(&engine, 4U, start_ms));
    tare_engine_touch_pad(&engine, 3U, start_ms + 180U);
    tare_engine_touch_pad(&engine, 2U, start_ms + 360U);
    EXPECT_FALSE(tare_engine_touch_end(&engine, start_ms + 660U));
    EXPECT_EQ(engine.reason, TARE_REASON_TOO_LONG);
}

static void test_release_guard_absorbs_lingering_finger_pressure(void)
{
    tare_engine_t engine;
    const uint32_t start_ms = begin_loaded(&engine, 75.0f, 4U);
    const uint32_t released_ms = start_ms + 250U;

    tare_engine_touch_pad(&engine, 3U, start_ms + 85U);
    tare_engine_touch_pad(&engine, 2U, start_ms + 170U);
    EXPECT_TRUE(tare_engine_touch_end(&engine, released_ms));

    for(uint32_t offset_ms = 20U; offset_ms < 280U; offset_ms += 20U) {
        tare_engine_feed_weight(&engine, 99.0f, released_ms + offset_ms);
    }
    EXPECT_EQ(engine.state, TARE_STATE_SETTLING);

    feed_after_release(&engine, released_ms + 260U, 75.0f, 760U);
    EXPECT_EQ(engine.state, TARE_STATE_REQUESTED);
}

static void test_changed_weight_after_guard_cancels(void)
{
    tare_engine_t engine;
    const uint32_t start_ms = begin_loaded(&engine, 128.0f, 4U);
    const uint32_t released_ms = start_ms + 260U;

    tare_engine_touch_pad(&engine, 3U, start_ms + 90U);
    tare_engine_touch_pad(&engine, 2U, start_ms + 180U);
    EXPECT_TRUE(tare_engine_touch_end(&engine, released_ms));

    tare_engine_feed_weight(&engine, 150.0f, released_ms + 260U);
    EXPECT_EQ(engine.state, TARE_STATE_SETTLING);
    tare_engine_feed_weight(&engine, 137.0f, released_ms + 280U);
    EXPECT_EQ(engine.state, TARE_STATE_CANCELLED);
    EXPECT_EQ(engine.reason, TARE_REASON_WEIGHT_CHANGED);
}

static void test_no_return_to_baseline_times_out(void)
{
    tare_engine_t engine;
    const uint32_t start_ms = begin_loaded(&engine, 128.0f, 4U);
    const uint32_t released_ms = start_ms + 260U;

    tare_engine_touch_pad(&engine, 3U, start_ms + 90U);
    tare_engine_touch_pad(&engine, 2U, start_ms + 180U);
    EXPECT_TRUE(tare_engine_touch_end(&engine, released_ms));

    for(uint32_t offset_ms = 20U; offset_ms <= 1800U; offset_ms += 20U) {
        /* Outside +/-2.5 g, but below the immediate >8 g change rejection. */
        tare_engine_feed_weight(&engine, 132.0f, released_ms + offset_ms);
    }
    EXPECT_EQ(engine.state, TARE_STATE_CANCELLED);
    EXPECT_EQ(engine.reason, TARE_REASON_SETTLE_TIMEOUT);
}

static void test_unstable_pre_touch_weight_blocks_gesture(void)
{
    tare_engine_t engine;
    uint32_t now_ms = 0U;

    tare_engine_init(&engine, NULL);
    for(int i = 0; i < 30; ++i) {
        tare_engine_feed_weight(&engine, (i % 2 == 0) ? 100.0f : 104.0f, now_ms);
        now_ms += 20U;
    }

    EXPECT_FALSE(tare_engine_touch_begin(&engine, 4U, now_ms));
    EXPECT_EQ(engine.state, TARE_STATE_IDLE);
    EXPECT_EQ(engine.reason, TARE_REASON_BASELINE_UNSTABLE);
}

int main(void)
{
    test_default_thresholds_match_rehearsal_contract();
    test_three_pad_swipe_ignores_all_touch_pressure();
    test_two_pad_swipe_requires_natural_endpoint_dwell();
    test_single_pad_category_tap_never_tares();
    test_empty_scale_swipe_is_wake_only();
    test_direction_reversal_is_rejected();
    test_skipped_pad_is_rejected();
    test_long_press_and_too_fast_swipe_are_rejected();
    test_release_guard_absorbs_lingering_finger_pressure();
    test_changed_weight_after_guard_cancels();
    test_no_return_to_baseline_times_out();
    test_unstable_pre_touch_weight_blocks_gesture();

    if(failures == 0) {
        printf("tare_engine_tests: 12/12 passed\n");
        return EXIT_SUCCESS;
    }

    printf("tare_engine_tests: %d failure(s)\n", failures);
    return EXIT_FAILURE;
}
