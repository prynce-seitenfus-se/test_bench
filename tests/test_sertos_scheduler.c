/**
 * @file test_sertos_scheduler.c
 * @brief Unit tests for SertOS Preemptive Priority Scheduler Core.
 *
 * Validates O(1) bitmap priority selection, round-robin time slicing,
 * scheduler preemption lock nesting, monotonic ticks, and delay wakeups.
 */

#include "unity.h"
#include "sertos_scheduler.h"
#include "sertos_task.h"
#include "sertos_port.h"
#include "memory_pool.h"
#include <string.h>
#include <windows.h>

#define STACK_SIZE  (512U)

static uint8_t s_stack_a[STACK_SIZE] __attribute__((aligned(8)));
static uint8_t s_stack_b[STACK_SIZE] __attribute__((aligned(8)));
static uint8_t s_stack_c[STACK_SIZE] __attribute__((aligned(8)));

static SertosTaskControlBlock s_tcb_a;
static SertosTaskControlBlock s_tcb_b;
static SertosTaskControlBlock s_tcb_c;

static SertosTaskHandle s_handle_a;
static SertosTaskHandle s_handle_b;
static SertosTaskHandle s_handle_c;
static SertosTaskControlBlock* s_idle_tcb;
static LONG s_delay_status;
static LONG s_delay_elapsed_ticks;
static LONG s_delay_task_resumed;

static void task_entry_dummy(void* param)
{
    (void)param;
}

static void task_entry_delay_test(void* param)
{
    SertosTick start_tick;
    SertosStatus delay_status;

    (void)param;
    start_tick = sertos_scheduler_get_tick_count();
    delay_status = sertos_scheduler_delay(3U);

    InterlockedExchange(&s_delay_status, (LONG)delay_status);
    InterlockedExchange(&s_delay_elapsed_ticks,
                        (LONG)(sertos_scheduler_get_tick_count() - start_tick));
    InterlockedExchange(&s_delay_task_resumed,
                        ((sertos_scheduler_get_current_tcb() == &s_tcb_a) &&
                         (s_tcb_a.state == SERTOS_TASK_STATE_RUNNING)) ? 1L : 0L);

    sertos_scheduler_stop();
    (void)Sleep(INFINITE);
}

static DWORD WINAPI scheduler_stop_watchdog(LPVOID param)
{
    HANDLE cancel_event = (HANDLE)param;

    if (WaitForSingleObject(cancel_event, 5000U) == WAIT_TIMEOUT) {
        sertos_scheduler_stop();
    }

    return 0U;
}

static uint8_t s_test_mem_pool[64U * 1024U] __attribute__((aligned(8)));

void setUp(void)
{
    (void)memory_pool_init(s_test_mem_pool, sizeof(s_test_mem_pool));
    (void)sertos_scheduler_init();
    s_idle_tcb = sertos_scheduler_select_next_task();
    s_handle_a = NULL;
    s_handle_b = NULL;
    s_handle_c = NULL;

    (void)memset(s_stack_a, 0, sizeof(s_stack_a));
    (void)memset(s_stack_b, 0, sizeof(s_stack_b));
    (void)memset(s_stack_c, 0, sizeof(s_stack_c));
}

void tearDown(void)
{
    if (sertos_scheduler_is_running()) {
        sertos_scheduler_stop();
    }

    if ((s_handle_a != NULL) && (s_handle_a->state != SERTOS_TASK_STATE_TERMINATED)) {
        (void)sertos_task_delete(s_handle_a);
    }
    if ((s_handle_b != NULL) && (s_handle_b->state != SERTOS_TASK_STATE_TERMINATED)) {
        (void)sertos_task_delete(s_handle_b);
    }
    if ((s_handle_c != NULL) && (s_handle_c->state != SERTOS_TASK_STATE_TERMINATED)) {
        (void)sertos_task_delete(s_handle_c);
    }
    if ((s_idle_tcb != NULL) && (s_idle_tcb->state != SERTOS_TASK_STATE_TERMINATED)) {
        (void)sertos_task_delete(s_idle_tcb);
    }
}

void test_scheduler_init_creates_idle_task(void)
{
    SertosTaskControlBlock* selected;

    TEST_ASSERT_FALSE(sertos_scheduler_is_running());
    TEST_ASSERT_FALSE(sertos_scheduler_is_locked());
    TEST_ASSERT_EQUAL_UINT32(0U, sertos_scheduler_get_tick_count());

    /* Scheduler init creates Idle Task at Priority 0 */
    selected = sertos_scheduler_select_next_task();
    TEST_ASSERT_NOT_NULL(selected);
    TEST_ASSERT_EQUAL_UINT8(0U, selected->priority);
    TEST_ASSERT_EQUAL_STRING("Idle", selected->name);
}

void test_scheduler_select_highest_priority(void)
{
    SertosTaskConfig cfg_a;
    SertosTaskConfig cfg_b;
    SertosTaskConfig cfg_c;
    SertosTaskControlBlock* selected;

    cfg_a.name = "Low";
    cfg_a.entry_func = task_entry_dummy;
    cfg_a.param = NULL;
    cfg_a.priority = 2U;
    cfg_a.stack_buffer = s_stack_a;
    cfg_a.stack_size = sizeof(s_stack_a);

    cfg_b.name = "Mid";
    cfg_b.entry_func = task_entry_dummy;
    cfg_b.param = NULL;
    cfg_b.priority = 8U;
    cfg_b.stack_buffer = s_stack_b;
    cfg_b.stack_size = sizeof(s_stack_b);

    cfg_c.name = "High";
    cfg_c.entry_func = task_entry_dummy;
    cfg_c.param = NULL;
    cfg_c.priority = 25U;
    cfg_c.stack_buffer = s_stack_c;
    cfg_c.stack_size = sizeof(s_stack_c);

    (void)sertos_task_create_static(&cfg_a, &s_tcb_a, &s_handle_a);
    (void)sertos_task_create_static(&cfg_b, &s_tcb_b, &s_handle_b);
    (void)sertos_task_create_static(&cfg_c, &s_tcb_c, &s_handle_c);

    /* O(1) CLZ lookup must select Task C (priority 25) */
    selected = sertos_scheduler_select_next_task();
    TEST_ASSERT_NOT_NULL(selected);
    TEST_ASSERT_EQUAL_PTR(&s_tcb_c, selected);
    TEST_ASSERT_EQUAL_UINT8(25U, selected->priority);

    /* Remove Task C; Task B (priority 8) must now be selected */
    (void)sertos_scheduler_remove_ready(&s_tcb_c);
    selected = sertos_scheduler_select_next_task();
    TEST_ASSERT_NOT_NULL(selected);
    TEST_ASSERT_EQUAL_PTR(&s_tcb_b, selected);

    /* Remove Task B; Task A (priority 2) must now be selected */
    (void)sertos_scheduler_remove_ready(&s_tcb_b);
    selected = sertos_scheduler_select_next_task();
    TEST_ASSERT_NOT_NULL(selected);
    TEST_ASSERT_EQUAL_PTR(&s_tcb_a, selected);
}

void test_scheduler_round_robin_rotation(void)
{
    SertosTaskConfig cfg_a;
    SertosTaskConfig cfg_b;
    SertosTaskControlBlock* selected;

    cfg_a.name = "WorkerA";
    cfg_a.entry_func = task_entry_dummy;
    cfg_a.param = NULL;
    cfg_a.priority = 5U;
    cfg_a.stack_buffer = s_stack_a;
    cfg_a.stack_size = sizeof(s_stack_a);

    cfg_b.name = "WorkerB";
    cfg_b.entry_func = task_entry_dummy;
    cfg_b.param = NULL;
    cfg_b.priority = 5U;
    cfg_b.stack_buffer = s_stack_b;
    cfg_b.stack_size = sizeof(s_stack_b);

    (void)sertos_task_create_static(&cfg_a, &s_tcb_a, &s_handle_a);
    (void)sertos_task_create_static(&cfg_b, &s_tcb_b, &s_handle_b);

    /* First in queue is WorkerA */
    selected = sertos_scheduler_select_next_task();
    TEST_ASSERT_EQUAL_PTR(&s_tcb_a, selected);

    /* Simulate context switch to WorkerA */
    sertos_scheduler_set_current_tcb(&s_tcb_a);
    s_tcb_a.state = SERTOS_TASK_STATE_RUNNING;

    /* Yield voluntarily triggers time slicing rotation */
    sertos_scheduler_yield();

    /* After rotation, WorkerB should be next */
    selected = sertos_scheduler_select_next_task();
    TEST_ASSERT_EQUAL_PTR(&s_tcb_b, selected);
}

void test_scheduler_lock_and_nesting(void)
{
    TEST_ASSERT_FALSE(sertos_scheduler_is_locked());

    sertos_scheduler_lock();
    TEST_ASSERT_TRUE(sertos_scheduler_is_locked());

    /* Nested lock */
    sertos_scheduler_lock();
    TEST_ASSERT_TRUE(sertos_scheduler_is_locked());

    /* First unlock */
    sertos_scheduler_unlock();
    TEST_ASSERT_TRUE(sertos_scheduler_is_locked());

    /* Outer unlock */
    sertos_scheduler_unlock();
    TEST_ASSERT_FALSE(sertos_scheduler_is_locked());
}

void test_scheduler_delay_and_tick_wakeup(void)
{
    SertosTaskConfig cfg_a;
    HANDLE watchdog_cancel_event;
    HANDLE watchdog_thread;
    DWORD watchdog_result;

    cfg_a.name = "Sleeper";
    cfg_a.entry_func = task_entry_delay_test;
    cfg_a.param = NULL;
    cfg_a.priority = 10U;
    cfg_a.stack_buffer = s_stack_a;
    cfg_a.stack_size = sizeof(s_stack_a);

    (void)sertos_task_create_static(&cfg_a, &s_tcb_a, &s_handle_a);

    s_delay_status = (LONG)SERTOS_STATUS_ERROR_NOT_INITIALIZED;
    s_delay_elapsed_ticks = 0L;
    s_delay_task_resumed = 0L;
    watchdog_cancel_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    TEST_ASSERT_NOT_NULL(watchdog_cancel_event);

    watchdog_thread = CreateThread(NULL, 0U, scheduler_stop_watchdog,
                                   watchdog_cancel_event, 0U, NULL);
    if (watchdog_thread == NULL) {
        (void)CloseHandle(watchdog_cancel_event);
        TEST_FAIL_MESSAGE("Could not create scheduler stop watchdog");
        return;
    }

    sertos_scheduler_start();

    (void)SetEvent(watchdog_cancel_event);
    watchdog_result = WaitForSingleObject(watchdog_thread, INFINITE);
    (void)CloseHandle(watchdog_thread);
    (void)CloseHandle(watchdog_cancel_event);

    TEST_ASSERT_EQUAL_UINT32(WAIT_OBJECT_0, watchdog_result);
    TEST_ASSERT_EQUAL(SERTOS_STATUS_OK, (SertosStatus)InterlockedCompareExchange(&s_delay_status, 0L, 0L));
    TEST_ASSERT_TRUE(InterlockedCompareExchange(&s_delay_elapsed_ticks, 0L, 0L) >= 3L);
    TEST_ASSERT_EQUAL_INT(1, InterlockedCompareExchange(&s_delay_task_resumed, 0L, 0L));
}

void test_scheduler_parameter_validation_and_edge_cases(void)
{
    SertosTaskControlBlock invalid_tcb;
    (void)memset(&invalid_tcb, 0, sizeof(invalid_tcb));
    invalid_tcb.priority = SERTOS_CONFIG_MAX_PRIORITIES;

    TEST_ASSERT_EQUAL(SERTOS_STATUS_ERROR_INVALID_PARAM, sertos_scheduler_add_ready(NULL));
    TEST_ASSERT_EQUAL(SERTOS_STATUS_ERROR_INVALID_PARAM, sertos_scheduler_add_ready(&invalid_tcb));
    TEST_ASSERT_EQUAL(SERTOS_STATUS_ERROR_INVALID_PARAM, sertos_scheduler_remove_ready(NULL));
    TEST_ASSERT_EQUAL(SERTOS_STATUS_ERROR_INVALID_PARAM, sertos_scheduler_remove_ready(&invalid_tcb));

    /* Delay with 0 ticks yields */
    TEST_ASSERT_EQUAL(SERTOS_STATUS_OK, sertos_scheduler_delay(0U));

    /* Reschedule when not running is safe no-op */
    sertos_scheduler_reschedule();
}

static uint32_t s_custom_tick_hook_count = 0U;
static void custom_tick_hook(void)
{
    s_custom_tick_hook_count++;
}

static uint8_t s_custom_idle_stack[1024U] __attribute__((aligned(8)));

void test_scheduler_runtime_configuration(void)
{
    SertosConfig cfg;
    SertosStatus status;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.tick_rate_hz = 500U;
    cfg.enable_time_slicing = false;
    cfg.idle_task_stack = s_custom_idle_stack;
    cfg.idle_task_stack_size = sizeof(s_custom_idle_stack);
    cfg.tick_hook = custom_tick_hook;
    cfg.idle_hook = NULL;

    s_custom_tick_hook_count = 0U;
    status = sertos_scheduler_init_with_config(&cfg);
    TEST_ASSERT_EQUAL(SERTOS_STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT32(500U, sertos_scheduler_get_tick_rate_hz());
    TEST_ASSERT_FALSE(sertos_scheduler_is_time_slicing_enabled());

    /* Verify tick hook is invoked */
    sertos_scheduler_tick();
    TEST_ASSERT_EQUAL_UINT32(1U, s_custom_tick_hook_count);

    /* Re-init with defaults */
    status = sertos_scheduler_init();
    TEST_ASSERT_EQUAL(SERTOS_STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT32(SERTOS_CONFIG_TICK_RATE_HZ, sertos_scheduler_get_tick_rate_hz());
    TEST_ASSERT_TRUE(sertos_scheduler_is_time_slicing_enabled());
}

static void test_scheduler_time_conversions(void)
{
    SertosConfig cfg = {
        .tick_rate_hz = 500U,
        .enable_time_slicing = true,
        .idle_task_stack = NULL,
        .idle_task_stack_size = 0U,
        .tick_hook = NULL,
        .idle_hook = NULL
    };

    /* At 1000 Hz */
    (void)sertos_scheduler_init();
    TEST_ASSERT_EQUAL_UINT32(10U, (uint32_t)SERTOS_MS_TO_TICKS(10U));
    TEST_ASSERT_EQUAL_UINT32(1U, (uint32_t)SERTOS_MS_TO_TICKS(1U));
    TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)SERTOS_MS_TO_TICKS(0U));
    TEST_ASSERT_EQUAL_UINT32(10U, SERTOS_TICKS_TO_MS(10U));
    TEST_ASSERT_EQUAL_UINT32(1000U, SERTOS_TICKS_TO_MS(1000U));

    /* At 500 Hz (1 tick = 2 ms) */
    (void)sertos_scheduler_init_with_config(&cfg);
    TEST_ASSERT_EQUAL_UINT32(500U, sertos_scheduler_get_tick_rate_hz());
    /* 1 ms ceiling -> (1 * 500 + 999) / 1000 = 1499 / 1000 = 1 tick */
    TEST_ASSERT_EQUAL_UINT32(1U, (uint32_t)SERTOS_MS_TO_TICKS(1U));
    /* 2 ms -> (2 * 500 + 999) / 1000 = 1999 / 1000 = 1 tick */
    TEST_ASSERT_EQUAL_UINT32(1U, (uint32_t)SERTOS_MS_TO_TICKS(2U));
    /* 3 ms -> (3 * 500 + 999) / 1000 = 2499 / 1000 = 2 ticks */
    TEST_ASSERT_EQUAL_UINT32(2U, (uint32_t)SERTOS_MS_TO_TICKS(3U));
    /* 10 ms -> 5 ticks */
    TEST_ASSERT_EQUAL_UINT32(5U, (uint32_t)SERTOS_MS_TO_TICKS(10U));
    /* Ticks to ms */
    TEST_ASSERT_EQUAL_UINT32(10U, SERTOS_TICKS_TO_MS(5U));

    /* Test delay_ms validation when scheduler is not running */
    TEST_ASSERT_EQUAL(SERTOS_STATUS_ERROR_NOT_INITIALIZED, sertos_scheduler_delay_ms(50U));

    /* Re-init with defaults */
    (void)sertos_scheduler_init();
}

void test_scheduler_delay_until_validation_and_wraparound(void)
{
    SertosTick wake;
    SertosStatus status;

    /* NULL last_wake_time pointer is rejected. */
    status = sertos_scheduler_delay_until(NULL, 10U);
    TEST_ASSERT_EQUAL(SERTOS_STATUS_ERROR_NULL_PTR, status);

    /* Zero period is rejected. */
    wake = sertos_scheduler_get_tick_count();
    status = sertos_scheduler_delay_until(&wake, 0U);
    TEST_ASSERT_EQUAL(SERTOS_STATUS_ERROR_INVALID_PARAM, status);

    /* Deadline still in the future: the call decides to block. With the
       scheduler not running the underlying delay reports NOT_INITIALIZED, but
       the next wake deadline must still advance by exactly one period. */
    wake = sertos_scheduler_get_tick_count();
    status = sertos_scheduler_delay_until(&wake, 5U);
    TEST_ASSERT_EQUAL(SERTOS_STATUS_ERROR_NOT_INITIALIZED, status);
    TEST_ASSERT_EQUAL_UINT32(5U, wake);

    /* Overrun: the deadline has already elapsed under wrap-safe modular
       comparison (now - wake = 16 >= period), so the call yields without
       blocking and returns OK while realigning the deadline by one period. */
    wake = 0xFFFFFFF0U;
    status = sertos_scheduler_delay_until(&wake, 10U);
    TEST_ASSERT_EQUAL(SERTOS_STATUS_OK, status);
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFAU, wake);

    /* Deadline advancement wraps modulo 2^32 without misbehaving
       (now - wake = 6 < period, so the block path is taken). */
    wake = 0xFFFFFFFAU;
    status = sertos_scheduler_delay_until(&wake, 10U);
    TEST_ASSERT_EQUAL(SERTOS_STATUS_ERROR_NOT_INITIALIZED, status);
    TEST_ASSERT_EQUAL_UINT32(4U, wake);
}

static SertosTaskHandle s_switch_prev[4];
static SertosTaskHandle s_switch_next[4];
static uint32_t s_switch_hook_count;

static __attribute__((no_instrument_function)) void custom_switch_hook(SertosTaskHandle prev,
                                                                       SertosTaskHandle next)
{
    if (s_switch_hook_count < 4U) {
        s_switch_prev[s_switch_hook_count] = prev;
        s_switch_next[s_switch_hook_count] = next;
    }
    s_switch_hook_count++;
}

static void create_task(const char* name, uint8_t priority, uint8_t* stack,
                        SertosTaskControlBlock* tcb, SertosTaskHandle* handle)
{
    SertosTaskConfig cfg;

    cfg.name = name;
    cfg.entry_func = task_entry_dummy;
    cfg.param = NULL;
    cfg.priority = priority;
    cfg.stack_buffer = stack;
    cfg.stack_size = STACK_SIZE;
    TEST_ASSERT_EQUAL(SERTOS_STATUS_OK, sertos_task_create_static(&cfg, tcb, handle));
}

void test_scheduler_switch_hook(void)
{
    SertosConfig cfg;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.tick_rate_hz = 1000U;
    cfg.enable_time_slicing = true;
    cfg.switch_hook = custom_switch_hook;
    s_switch_hook_count = 0U;
    TEST_ASSERT_EQUAL(SERTOS_STATUS_OK, sertos_scheduler_init_with_config(&cfg));
    s_idle_tcb = sertos_scheduler_get_idle_tcb();

    create_task("A", 2U, s_stack_a, &s_tcb_a, &s_handle_a);

    /* First switch: prev is NULL */
    TEST_ASSERT_EQUAL_PTR(&s_tcb_a, sertos_scheduler_perform_switch());
    TEST_ASSERT_EQUAL_UINT32(1U, s_switch_hook_count);
    TEST_ASSERT_NULL(s_switch_prev[0]);
    TEST_ASSERT_EQUAL_PTR(s_handle_a, s_switch_next[0]);

    /* Re-selecting the running task is not a switch */
    TEST_ASSERT_EQUAL_PTR(&s_tcb_a, sertos_scheduler_perform_switch());
    TEST_ASSERT_EQUAL_UINT32(1U, s_switch_hook_count);

    /* Higher-priority task preempts: hook receives prev and next */
    create_task("B", 3U, s_stack_b, &s_tcb_b, &s_handle_b);
    TEST_ASSERT_EQUAL_PTR(&s_tcb_b, sertos_scheduler_perform_switch());
    TEST_ASSERT_EQUAL_UINT32(2U, s_switch_hook_count);
    TEST_ASSERT_EQUAL_PTR(s_handle_a, s_switch_prev[1]);
    TEST_ASSERT_EQUAL_PTR(s_handle_b, s_switch_next[1]);

    /* Default init clears the hook: switching stays safe and silent */
    (void)sertos_scheduler_init();
    s_idle_tcb = sertos_scheduler_get_idle_tcb();
    s_handle_a = NULL;
    s_handle_b = NULL;
    create_task("C", 2U, s_stack_c, &s_tcb_c, &s_handle_c);
    TEST_ASSERT_EQUAL_PTR(&s_tcb_c, sertos_scheduler_perform_switch());
    TEST_ASSERT_EQUAL_UINT32(2U, s_switch_hook_count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_scheduler_init_creates_idle_task);
    RUN_TEST(test_scheduler_select_highest_priority);
    RUN_TEST(test_scheduler_round_robin_rotation);
    RUN_TEST(test_scheduler_lock_and_nesting);
    RUN_TEST(test_scheduler_delay_and_tick_wakeup);
    RUN_TEST(test_scheduler_parameter_validation_and_edge_cases);
    RUN_TEST(test_scheduler_runtime_configuration);
    RUN_TEST(test_scheduler_time_conversions);
    RUN_TEST(test_scheduler_delay_until_validation_and_wraparound);
    RUN_TEST(test_scheduler_switch_hook);
    return UNITY_END();
}
