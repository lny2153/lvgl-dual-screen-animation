#include "domain/transition_rules.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(expr)                                                                  \
    do {                                                                             \
        if (!(expr)) {                                                               \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);          \
            exit(1);                                                                 \
        }                                                                            \
    } while (0)

static tr_result_t fire(tr_state_t *state, tr_event_type_t type)
{
    tr_event_t event = tr_event_make(type);
    return tr_dispatch(state, &event);
}

static tr_result_t finish_scene(tr_state_t *state)
{
    tr_event_t event = tr_event_make(TR_EVENT_ANIMATION_FINISHED);
    event.token = state->scene_token;
    return tr_dispatch(state, &event);
}

static void boot_to_active(tr_state_t *state)
{
    tr_result_t result;
    tr_state_init(state);
    CHECK(tr_state_is_valid(state));
    result = fire(state, TR_EVENT_POWER_ON);
    CHECK(result.status == TR_STATUS_APPLIED);
    CHECK(state->left_scene == TR_LEFT_BOOT);
    CHECK(state->right_scene == TR_RIGHT_BOOT);
    CHECK(tr_state_is_valid(state));
    result = finish_scene(state);
    CHECK(result.status == TR_STATUS_APPLIED);
    CHECK(state->mode == TR_MODE_ACTIVE);
    CHECK(state->left_scene == TR_LEFT_EYE);
    CHECK(state->right_scene == TR_RIGHT_PLUS);
    CHECK(tr_state_is_valid(state));
}

static void set_weight(tr_state_t *state, bool positive, bool stable)
{
    tr_event_t event = tr_event_make(TR_EVENT_WEIGHT_STATUS);
    event.weight_positive = positive;
    event.weight_stable = stable;
    CHECK(tr_dispatch(state, &event).status == TR_STATUS_APPLIED);
}

static void make_food_addable(tr_state_t *state)
{
    set_weight(state, true, true);
    CHECK(fire(state, TR_EVENT_AI_RESULT_STABLE).status == TR_STATUS_APPLIED);
    CHECK(state->left_scene == TR_LEFT_FOOD);
    CHECK(tr_can_add(state));
    CHECK(tr_state_is_valid(state));
}

static void commit_one_item(tr_state_t *state)
{
    tr_result_t request;
    tr_event_t ack;

    make_food_addable(state);
    request = fire(state, TR_EVENT_ADD_REQUEST);
    CHECK(request.status == TR_STATUS_APPLIED);
    CHECK(request.command == TR_COMMAND_COMMIT_ADD);
    CHECK(request.command_token != 0U);

    ack = tr_event_make(TR_EVENT_ADD_ACK_SUCCESS);
    ack.token = request.command_token;
    CHECK(tr_dispatch(state, &ack).status == TR_STATUS_APPLIED);
    CHECK(state->meal_item_count == 1U);
    CHECK(state->left_scene == TR_LEFT_ADD_FEEDBACK);
    CHECK(state->right_scene == TR_RIGHT_ADD_FEEDBACK);
    CHECK(tr_state_is_valid(state));

    CHECK(finish_scene(state).status == TR_STATUS_APPLIED);
    CHECK(state->left_scene == TR_LEFT_EYE);
    CHECK(tr_state_is_valid(state));
}

static void test_identity_slot_is_strictly_exclusive(void)
{
    tr_state_t state;
    boot_to_active(&state);

    CHECK(tr_left_identity(state.left_scene) == TR_IDENTITY_EYE);
    set_weight(&state, true, true);
    CHECK(fire(&state, TR_EVENT_AI_RESULT_STABLE).status == TR_STATUS_APPLIED);
    CHECK(state.left_scene == TR_LEFT_FOOD);
    CHECK(state.food_active);
    CHECK(!state.candidates_open);
    CHECK(tr_left_identity(state.left_scene) == TR_IDENTITY_FOOD);

    CHECK(fire(&state, TR_EVENT_OPEN_CANDIDATES).status == TR_STATUS_APPLIED);
    CHECK(state.left_scene == TR_LEFT_CANDIDATES);
    CHECK(state.food_active);
    CHECK(state.candidates_open);
    CHECK(tr_left_identity(state.left_scene) == TR_IDENTITY_CANDIDATES);

    CHECK(fire(&state, TR_EVENT_SELECT_CANDIDATE).status == TR_STATUS_APPLIED);
    CHECK(state.left_scene == TR_LEFT_FOOD);
    CHECK(state.food_active);
    CHECK(!state.candidates_open);
    CHECK(tr_state_is_valid(&state));
}

static void test_add_does_not_commit_before_matching_ack(void)
{
    tr_state_t state;
    tr_result_t request;
    tr_event_t ack;

    boot_to_active(&state);
    make_food_addable(&state);
    request = fire(&state, TR_EVENT_ADD_REQUEST);
    CHECK(request.command == TR_COMMAND_COMMIT_ADD);
    CHECK(state.meal_item_count == 0U);
    CHECK(state.left_scene == TR_LEFT_FOOD);
    CHECK(state.operation == TR_OP_ADD_WAIT_ACK);
    CHECK(tr_state_is_valid(&state));

    ack = tr_event_make(TR_EVENT_ADD_ACK_SUCCESS);
    ack.token = request.command_token + 1U;
    CHECK(tr_dispatch(&state, &ack).status == TR_STATUS_STALE_TOKEN);
    CHECK(state.meal_item_count == 0U);
    CHECK(state.left_scene == TR_LEFT_FOOD);

    ack = tr_event_make(TR_EVENT_ADD_ACK_FAILURE);
    ack.token = request.command_token;
    CHECK(tr_dispatch(&state, &ack).status == TR_STATUS_APPLIED);
    CHECK(state.meal_item_count == 0U);
    CHECK(state.left_scene == TR_LEFT_FOOD);
    CHECK(tr_can_add(&state));

    request = fire(&state, TR_EVENT_ADD_REQUEST);
    ack = tr_event_make(TR_EVENT_ADD_ACK_SUCCESS);
    ack.token = request.command_token;
    CHECK(tr_dispatch(&state, &ack).status == TR_STATUS_APPLIED);
    CHECK(state.meal_item_count == 1U);
    CHECK(state.left_scene == TR_LEFT_ADD_FEEDBACK);
    CHECK(tr_left_identity(state.left_scene) == TR_IDENTITY_STATUS);
    CHECK(finish_scene(&state).status == TR_STATUS_APPLIED);
    CHECK(state.left_scene == TR_LEFT_EYE);
    CHECK(state.camera_enabled);
    CHECK(tr_state_is_valid(&state));
}

static void test_finish_hold_cancel_and_ack_gate(void)
{
    tr_state_t state;
    tr_result_t request;
    tr_event_t ack;

    boot_to_active(&state);
    commit_one_item(&state);
    CHECK(tr_can_finish(&state));

    CHECK(fire(&state, TR_EVENT_FINISH_HOLD_BEGIN).status == TR_STATUS_APPLIED);
    CHECK(state.right_scene == TR_RIGHT_FINISH_PROGRESS);
    CHECK(state.meal_item_count == 1U);
    CHECK(fire(&state, TR_EVENT_FINISH_HOLD_CANCEL).status == TR_STATUS_APPLIED);
    CHECK(state.right_scene == TR_RIGHT_PLUS);
    CHECK(state.meal_item_count == 1U);

    CHECK(fire(&state, TR_EVENT_FINISH_HOLD_BEGIN).status == TR_STATUS_APPLIED);
    request = fire(&state, TR_EVENT_FINISH_REQUEST);
    CHECK(request.status == TR_STATUS_APPLIED);
    CHECK(request.command == TR_COMMAND_COMMIT_FINISH);
    CHECK(state.operation == TR_OP_FINISH_WAIT_ACK);
    CHECK(state.meal_item_count == 1U);

    ack = tr_event_make(TR_EVENT_FINISH_ACK_SUCCESS);
    ack.token = request.command_token;
    CHECK(tr_dispatch(&state, &ack).status == TR_STATUS_APPLIED);
    CHECK(state.operation == TR_OP_FINISH_FEEDBACK);
    CHECK(state.completed_item_count == 1U);
    CHECK(state.meal_item_count == 1U); /* Summary remains readable during feedback. */
    CHECK(state.left_scene == TR_LEFT_FINISH_SUCCESS);
    CHECK(state.right_scene == TR_RIGHT_FINISH_SUCCESS);

    CHECK(finish_scene(&state).status == TR_STATUS_APPLIED);
    CHECK(state.meal_item_count == 0U);
    CHECK(state.completed_item_count == 0U);
    CHECK(state.left_scene == TR_LEFT_EYE);
    CHECK(tr_state_is_valid(&state));
}

static void test_tare_requires_qualified_request_and_ack(void)
{
    tr_state_t state;
    tr_event_t request_event;
    tr_result_t request;
    tr_event_t ack;

    boot_to_active(&state);
    commit_one_item(&state);
    make_food_addable(&state);

    request_event = tr_event_make(TR_EVENT_TARE_REQUEST);
    request_event.qualified = false;
    CHECK(tr_dispatch(&state, &request_event).status == TR_STATUS_REJECTED_PRECONDITION);
    CHECK(state.left_scene == TR_LEFT_FOOD);

    request_event.qualified = true;
    request = tr_dispatch(&state, &request_event);
    CHECK(request.status == TR_STATUS_APPLIED);
    CHECK(request.command == TR_COMMAND_SET_TARE);
    CHECK(state.operation == TR_OP_TARE_WAIT_ACK);
    CHECK(state.left_scene == TR_LEFT_FOOD); /* No false success before hardware ACK. */
    CHECK(state.meal_item_count == 1U);

    ack = tr_event_make(TR_EVENT_TARE_ACK_SUCCESS);
    ack.token = request.command_token;
    CHECK(tr_dispatch(&state, &ack).status == TR_STATUS_APPLIED);
    CHECK(state.left_scene == TR_LEFT_TARE_SUCCESS);
    CHECK(state.right_scene == TR_RIGHT_PLUS);
    CHECK(state.meal_item_count == 1U); /* Tare never erases committed meal history. */
    CHECK(!state.current_weight_positive);

    CHECK(finish_scene(&state).status == TR_STATUS_APPLIED);
    CHECK(state.left_scene == TR_LEFT_EYE);
    CHECK(tr_state_is_valid(&state));
}

static void test_safety_priority_invalidates_pending_transaction(void)
{
    tr_state_t state;
    tr_result_t request;
    tr_event_t ack;

    boot_to_active(&state);
    make_food_addable(&state);
    request = fire(&state, TR_EVENT_ADD_REQUEST);
    CHECK(state.pending_command_token == request.command_token);

    CHECK(fire(&state, TR_EVENT_OVERLOAD_ENTER).status == TR_STATUS_APPLIED);
    CHECK(state.operation == TR_OP_OVERLOAD);
    CHECK(state.left_scene == TR_LEFT_OVERLOAD);
    CHECK(state.right_scene == TR_RIGHT_OVERLOAD);
    CHECK(state.pending_command_token == 0U);
    CHECK(tr_event_priority(TR_EVENT_OVERLOAD_ENTER) >
          tr_event_priority(TR_EVENT_ADD_REQUEST));

    ack = tr_event_make(TR_EVENT_ADD_ACK_SUCCESS);
    ack.token = request.command_token;
    CHECK(tr_dispatch(&state, &ack).status == TR_STATUS_STALE_TOKEN);
    CHECK(state.meal_item_count == 0U);
    CHECK(state.left_scene == TR_LEFT_OVERLOAD);

    CHECK(fire(&state, TR_EVENT_OVERLOAD_CLEAR).status == TR_STATUS_APPLIED);
    CHECK(state.operation == TR_OP_NONE);
    CHECK(state.left_scene == TR_LEFT_EYE);
    CHECK(state.meal_item_count == 0U);
    CHECK(tr_state_is_valid(&state));
}

static void test_sleep_snapshot_restores_identity_but_not_stale_goal_ring(void)
{
    tr_state_t state;
    tr_event_t ready;
    tr_event_t wake;

    boot_to_active(&state);
    CHECK(fire(&state, TR_EVENT_BLE_LINK_UP).status == TR_STATUS_APPLIED);
    ready = tr_event_make(TR_EVENT_APP_SESSION_READY);
    ready.goal_valid = true;
    CHECK(tr_dispatch(&state, &ready).status == TR_STATUS_APPLIED);
    CHECK(state.left_scene == TR_LEFT_BLE_SUCCESS);
    CHECK(finish_scene(&state).status == TR_STATUS_APPLIED);
    CHECK(state.right_scene == TR_RIGHT_PLUS_WITH_GOAL);

    make_food_addable(&state);
    CHECK(fire(&state, TR_EVENT_OPEN_CANDIDATES).status == TR_STATUS_APPLIED);
    CHECK(state.left_scene == TR_LEFT_CANDIDATES);

    CHECK(fire(&state, TR_EVENT_SLEEP_TIMEOUT).status == TR_STATUS_APPLIED);
    CHECK(state.operation == TR_OP_SLEEP_FADE);
    CHECK(state.sleep_snapshot.valid);
    CHECK(finish_scene(&state).status == TR_STATUS_APPLIED);
    CHECK(state.mode == TR_MODE_SLEEPING);
    CHECK(state.left_scene == TR_LEFT_OFF);
    CHECK(!state.camera_enabled);
    CHECK(!state.wifi_enabled);
    CHECK(state.touch_monitoring_enabled);
    CHECK(state.weight_monitoring_enabled);
    CHECK(tr_state_is_valid(&state));

    CHECK(fire(&state, TR_EVENT_BLE_DISCONNECTED).status == TR_STATUS_APPLIED);
    wake = tr_event_make(TR_EVENT_WAKE_REQUEST);
    wake.wake_source = TR_WAKE_UNIT_KEY;
    CHECK(tr_dispatch(&state, &wake).status == TR_STATUS_APPLIED);
    CHECK(state.left_scene == TR_LEFT_WAKE);
    CHECK(state.right_scene == TR_RIGHT_WAKE);
    CHECK(finish_scene(&state).status == TR_STATUS_APPLIED);

    CHECK(state.mode == TR_MODE_ACTIVE);
    CHECK(state.left_scene == TR_LEFT_CANDIDATES);
    CHECK(state.right_scene == TR_RIGHT_PLUS); /* Disconnection during sleep wins. */
    CHECK(state.food_active);
    CHECK(state.candidates_open);
    CHECK(!state.sleep_snapshot.valid);
    CHECK(tr_state_is_valid(&state));
}

static void test_charger_overrides_critical_battery_but_not_overload(void)
{
    tr_state_t state;
    boot_to_active(&state);

    CHECK(fire(&state, TR_EVENT_CRITICAL_BATTERY).status == TR_STATUS_APPLIED);
    CHECK(state.operation == TR_OP_CRITICAL_BATTERY);
    CHECK(state.left_scene == TR_LEFT_CRITICAL_BATTERY);

    CHECK(fire(&state, TR_EVENT_CHARGER_CONNECTED).status == TR_STATUS_APPLIED);
    CHECK(state.operation == TR_OP_CHARGING);
    CHECK(state.left_scene == TR_LEFT_CHARGING);

    CHECK(fire(&state, TR_EVENT_OVERLOAD_ENTER).status == TR_STATUS_APPLIED);
    CHECK(state.operation == TR_OP_OVERLOAD);
    CHECK(state.left_scene == TR_LEFT_OVERLOAD);

    CHECK(fire(&state, TR_EVENT_OVERLOAD_CLEAR).status == TR_STATUS_APPLIED);
    CHECK(state.operation == TR_OP_CHARGING);
    CHECK(state.left_scene == TR_LEFT_CHARGING);
    CHECK(tr_state_is_valid(&state));
}

static void test_connection_update_does_not_preempt_pending_add(void)
{
    tr_state_t state;
    tr_result_t request;
    tr_event_t ready;
    tr_event_t ack;

    boot_to_active(&state);
    make_food_addable(&state);
    request = fire(&state, TR_EVENT_ADD_REQUEST);
    CHECK(request.command == TR_COMMAND_COMMIT_ADD);

    CHECK(fire(&state, TR_EVENT_BLE_LINK_UP).status == TR_STATUS_APPLIED);
    ready = tr_event_make(TR_EVENT_APP_SESSION_READY);
    ready.goal_valid = true;
    CHECK(tr_dispatch(&state, &ready).status == TR_STATUS_APPLIED);
    CHECK(state.operation == TR_OP_ADD_WAIT_ACK);
    CHECK(state.left_scene == TR_LEFT_FOOD);
    CHECK(state.right_scene == TR_RIGHT_PLUS_WITH_GOAL);
    CHECK(tr_state_is_valid(&state));

    ack = tr_event_make(TR_EVENT_ADD_ACK_SUCCESS);
    ack.token = request.command_token;
    CHECK(tr_dispatch(&state, &ack).status == TR_STATUS_APPLIED);
    CHECK(state.meal_item_count == 1U);
    CHECK(state.operation == TR_OP_ADD_FEEDBACK);
    CHECK(tr_state_is_valid(&state));
}

static void test_finish_rejects_uncommitted_current_item(void)
{
    tr_state_t state;
    boot_to_active(&state);
    commit_one_item(&state);

    make_food_addable(&state);
    CHECK(!tr_can_finish(&state));
    CHECK(fire(&state, TR_EVENT_FINISH_HOLD_BEGIN).status == TR_STATUS_REJECTED_PRECONDITION);
    CHECK(state.meal_item_count == 1U);
    CHECK(state.current_weight_positive);
    CHECK(tr_state_is_valid(&state));
}

int main(void)
{
    test_identity_slot_is_strictly_exclusive();
    test_add_does_not_commit_before_matching_ack();
    test_finish_hold_cancel_and_ack_gate();
    test_tare_requires_qualified_request_and_ack();
    test_safety_priority_invalidates_pending_transaction();
    test_sleep_snapshot_restores_identity_but_not_stale_goal_ring();
    test_charger_overrides_critical_battery_but_not_overload();
    test_connection_update_does_not_preempt_pending_add();
    test_finish_rejects_uncommitted_current_item();

    printf("transition_rules_tests: 9/9 passed\n");
    return 0;
}
