#include "unity.h"
#include "profiler.h"
#include "profiler_port_stub.h"
#include "stream.h"
#include <string.h>

#define FN_F ((void*)0x6000U)
#define FN_G ((void*)0x6100U)
#define FN_H ((void*)0x6200U)
#define CTX_A ((const void*)0xA000U)
#define CTX_B ((const void*)0xB000U)

/* Inspect telemetry, not private implementation structures. */
typedef struct TestRecord {
    uint32_t address;
    uint32_t calls;
    uint64_t total;
    uint32_t min;
    uint32_t max;
} TestRecord;

static uint8_t s_output[8192U];
static size_t s_length;

extern uint32_t profiler_test_target_add(uint32_t a, uint32_t b);
extern uint32_t profiler_test_target_sub(uint32_t a, uint32_t b);

static size_t write_output(void* context, const uint8_t* buffer, size_t size)
{
    (void)context;
    if (size > sizeof(s_output) - s_length) {
        return 0U;
    }
    (void)memcpy(&s_output[s_length], buffer, size);
    s_length += size;
    return size;
}

static uint16_t read_u16(size_t offset)
{
    uint16_t value;
    (void)memcpy(&value, &s_output[offset], sizeof(value));
    return value;
}

static void dump(void)
{
    Stream stream = stream_init(NULL, write_output, NULL, NULL);
    s_length = 0U;
    TEST_ASSERT_TRUE(profiler_dump(&stream, PROFILER_DUMP_BIN));
    TEST_ASSERT_EQUAL_UINT16(PROFILER_BIN_VERSION, read_u16(4U));
    TEST_ASSERT_EQUAL_UINT32(28U + (24U * read_u16(6U)) + 4U, s_length);
}

static TestRecord metric(const void* fn)
{
    TestRecord record = { 0U, 0U, 0ULL, 0U, 0U };
    dump();
    for (size_t i = 0U; i < read_u16(6U); ++i) {
        (void)memcpy(&record, &s_output[28U + (24U * i)], 24U);
        if (record.address == (uint32_t)(uintptr_t)fn) {
            return record;
        }
    }
    TEST_FAIL_MESSAGE("Function absent from PROF-BIN telemetry");
    return record;
}

static void enter_at(const void* ctx, void* fn, uint32_t ticks)
{
    profiler_port_stub_set_context(ctx);
    profiler_port_stub_set_ticks(ticks);
    __cyg_profile_func_enter(fn, NULL);
}

static void exit_at(const void* ctx, void* fn, uint32_t ticks)
{
    profiler_port_stub_set_context(ctx);
    profiler_port_stub_set_ticks(ticks);
    __cyg_profile_func_exit(fn, NULL);
}

void setUp(void)
{
    TEST_ASSERT_TRUE(profiler_init(1000000U, NULL));
    profiler_port_stub_set_auto_ticks(false);
    s_length = 0U;
}

void tearDown(void)
{
    profiler_stop();
}

static void test_validation_and_disabled_hooks(void)
{
    TEST_ASSERT_FALSE(profiler_init(0U, NULL));
    TEST_ASSERT_EQUAL_UINT32(1000000U, profiler_frequency());
    enter_at(NULL, FN_F, 10U);
    exit_at(NULL, FN_F, 20U);
    dump();
    TEST_ASSERT_EQUAL_UINT16(0U, read_u16(6U));
    TEST_ASSERT_FALSE(profiler_enabled());
    TEST_ASSERT_FALSE(profiler_dump(NULL, PROFILER_DUMP_BIN));
}

static void interrupt_during_calibration(void)
{
    TEST_ASSERT_FALSE(profiler_enabled());
    profiler_start();
    profiler_port_stub_set_context(CTX_B);
    __cyg_profile_func_enter(FN_F, NULL);
    __cyg_profile_func_exit(FN_F, NULL);
    profiler_port_stub_set_context(NULL);
}

static void test_calibration_releases_mask_between_samples(void)
{
    profiler_port_stub_set_auto_ticks(true);
    profiler_port_stub_on_unlock(interrupt_during_calibration);
    profiler_start();
    TEST_ASSERT_TRUE(profiler_enabled());
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(34U, profiler_port_stub_unlock_count());
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(16U, profiler_port_stub_max_critical_ticks());
    dump();
    TEST_ASSERT_EQUAL_UINT16(0U, read_u16(6U));
    TEST_ASSERT_EQUAL_UINT16(0U, read_u16(16U));
}

static void test_stop_and_reset_cancel_pending_calibration(void)
{
    profiler_port_stub_set_auto_ticks(true);
    profiler_port_stub_on_unlock(profiler_stop);
    profiler_start();
    TEST_ASSERT_FALSE(profiler_enabled());
    profiler_port_stub_on_unlock(profiler_reset);
    profiler_start();
    TEST_ASSERT_FALSE(profiler_enabled());
    profiler_start();
    TEST_ASSERT_TRUE(profiler_enabled());
    dump();
    TEST_ASSERT_EQUAL_UINT16(0U, read_u16(6U));
}

static void test_single_nested_and_recursive_calls(void)
{
    profiler_start();
    enter_at(NULL, FN_F, 10U);
    enter_at(NULL, FN_G, 20U);
    exit_at(NULL, FN_G, 25U);
    exit_at(NULL, FN_F, 40U);
    enter_at(NULL, FN_F, 50U);
    enter_at(NULL, FN_F, 55U);
    exit_at(NULL, FN_F, 60U);
    exit_at(NULL, FN_F, 70U);
    TestRecord f = metric(FN_F);
    TestRecord g = metric(FN_G);
    TEST_ASSERT_EQUAL_UINT32(3U, f.calls);
    TEST_ASSERT_EQUAL_UINT64(55ULL, f.total);
    TEST_ASSERT_EQUAL_UINT32(5U, f.min);
    TEST_ASSERT_EQUAL_UINT32(30U, f.max);
    TEST_ASSERT_EQUAL_UINT32(5U, g.max);
}

static void test_stop_reset_and_null_hooks(void)
{
    profiler_start();
    __cyg_profile_func_enter(NULL, NULL);
    __cyg_profile_func_exit(NULL, NULL);
    enter_at(NULL, FN_F, 10U);
    exit_at(NULL, FN_F, 20U);
    profiler_stop();
    enter_at(NULL, FN_G, 30U);
    exit_at(NULL, FN_G, 40U);
    dump();
    TEST_ASSERT_EQUAL_UINT16(1U, read_u16(6U));
    profiler_reset();
    dump();
    TEST_ASSERT_EQUAL_UINT16(0U, read_u16(6U));
}

static void test_mismatched_exit_and_stale_frames(void)
{
    profiler_start();
    exit_at(NULL, FN_H, 1U);
    enter_at(NULL, FN_F, 10U);
    enter_at(NULL, FN_G, 15U);
    exit_at(NULL, FN_F, 30U);
    TEST_ASSERT_EQUAL_UINT32(20U, metric(FN_F).max);
    TEST_ASSERT_EQUAL_UINT16(1U, read_u16(16U));
    TEST_ASSERT_EQUAL_UINT16(1U, read_u16(18U));
}

static void test_context_active_time_and_legacy_isolation(void)
{
    profiler_start();
    enter_at(NULL, FN_H, 1U);
    enter_at(CTX_A, FN_F, 10U);
    profiler_port_stub_set_ticks(20U);
    profiler_context_switch(CTX_A, CTX_B);
    enter_at(CTX_B, FN_G, 25U);
    exit_at(CTX_B, FN_G, 35U);
    profiler_port_stub_set_ticks(50U);
    profiler_context_switch(CTX_B, CTX_A);
    exit_at(CTX_A, FN_F, 60U);
    exit_at(NULL, FN_H, 70U);
    TEST_ASSERT_EQUAL_UINT32(20U, metric(FN_F).max);
    TEST_ASSERT_EQUAL_UINT32(10U, metric(FN_G).max);
    TEST_ASSERT_EQUAL_UINT32(69U, metric(FN_H).max);
}

static void test_wraparound_and_release(void)
{
    profiler_start();
    enter_at(CTX_A, FN_F, UINT32_MAX - 4U);
    exit_at(CTX_A, FN_F, 5U);
    TEST_ASSERT_EQUAL_UINT32(10U, metric(FN_F).max);
    enter_at(CTX_A, FN_G, 10U);
    profiler_context_release(CTX_A);
    exit_at(CTX_A, FN_G, 20U);
    dump();
    TEST_ASSERT_EQUAL_UINT16(1U, read_u16(16U));
}

static void test_capacity_diagnostics(void)
{
    profiler_start();
    for (size_t i = 0U; i < 17U; ++i) {
        enter_at(NULL, (void*)(uintptr_t)(0x7000U + i), (uint32_t)i);
    }

    dump();
    TEST_ASSERT_EQUAL_UINT16(1U, read_u16(14U));
    profiler_reset();
    profiler_start();
    for (size_t i = 0U; i < 129U; ++i) {
        void* fn = (void*)(uintptr_t)(0x8000U + i);
        enter_at(NULL, fn, (uint32_t)(i * 2U));
        exit_at(NULL, fn, (uint32_t)((i * 2U) + 1U));
    }
    dump();
    TEST_ASSERT_EQUAL_UINT16(128U, read_u16(6U));
    TEST_ASSERT_EQUAL_UINT16(1U, read_u16(12U));
    profiler_reset();
    profiler_start();
    for (size_t i = 0U; i < 9U; ++i) {
        enter_at((const void*)(uintptr_t)(0x9000U + i), FN_F, (uint32_t)i);
    }
    dump();
    TEST_ASSERT_EQUAL_UINT16(1U, read_u16(20U));
}

static void test_recursive_overflow_preserves_outer_frames(void)
{
    profiler_start();
    for (size_t i = 0U; i < 18U; ++i) {
        enter_at(NULL, FN_F, (uint32_t)i);
    }
    for (size_t i = 0U; i < 18U; ++i) {
        exit_at(NULL, FN_F, (uint32_t)(20U + i));
    }
    TEST_ASSERT_EQUAL_UINT32(16U, metric(FN_F).calls);
    TEST_ASSERT_EQUAL_UINT16(2U, read_u16(14U));
    TEST_ASSERT_EQUAL_UINT16(0U, read_u16(16U));
    TEST_ASSERT_EQUAL_UINT16(0U, read_u16(18U));
}

static void test_calibration_and_nested_subtraction(void)
{
    profiler_port_stub_set_auto_ticks(true);
    profiler_start();
    dump();
    uint16_t probe = read_u16(24U);
    uint16_t nested = read_u16(26U);
    TEST_ASSERT_EQUAL_UINT16(1U, probe);
    TEST_ASSERT_EQUAL_UINT16(2U, nested);
    enter_at(NULL, FN_F, 100U);
    enter_at(NULL, FN_G, 110U);
    exit_at(NULL, FN_G, 120U);
    exit_at(NULL, FN_F, 140U);
    TEST_ASSERT_EQUAL_UINT32(34U, metric(FN_F).max);
    TEST_ASSERT_EQUAL_UINT32(8U, metric(FN_G).max);
}

static void test_real_compiler_instrumentation(void)
{
    profiler_port_stub_set_auto_ticks(true);
    profiler_start();
    TEST_ASSERT_EQUAL_UINT32(12U, profiler_test_target_add(7U, 5U));
    TEST_ASSERT_EQUAL_UINT32(2U, profiler_test_target_sub(7U, 5U));
    TEST_ASSERT_EQUAL_UINT32(1U, metric((const void*)(uintptr_t)profiler_test_target_add).calls);
    TEST_ASSERT_EQUAL_UINT32(1U, metric((const void*)(uintptr_t)profiler_test_target_sub).calls);
}

static void test_binary_crc_and_output_formats(void)
{
    profiler_start();
    enter_at(NULL, FN_F, 10U);
    exit_at(NULL, FN_F, 20U);
    dump();
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0U; i < s_length - 4U; ++i) {
        crc ^= s_output[i];
        for (size_t bit = 0U; bit < 8U; ++bit) {
            crc = (crc >> 1U) ^ (((crc & 1U) != 0U) ? 0xEDB88320U : 0U);
        }
    }
    uint32_t actual;
    (void)memcpy(&actual, &s_output[s_length - 4U], sizeof(actual));
    TEST_ASSERT_EQUAL_HEX32(crc ^ UINT32_MAX, actual);
    Stream stream = stream_init(NULL, write_output, NULL, NULL);
    s_length = 0U;
    TEST_ASSERT_TRUE(profiler_dump(&stream, PROFILER_DUMP_CSV));
    s_output[s_length] = 0U;
    TEST_ASSERT_NOT_NULL(strstr((const char*)s_output, "0x00006000,1,10,10,10"));
    s_length = 0U;
    TEST_ASSERT_TRUE(profiler_dump(&stream, PROFILER_DUMP_TOP));
    s_output[s_length] = 0U;
    TEST_ASSERT_NOT_NULL(strstr((const char*)s_output, "0x00006000"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_validation_and_disabled_hooks);
    RUN_TEST(test_single_nested_and_recursive_calls);
    RUN_TEST(test_stop_reset_and_null_hooks);
    RUN_TEST(test_mismatched_exit_and_stale_frames);
    RUN_TEST(test_context_active_time_and_legacy_isolation);
    RUN_TEST(test_wraparound_and_release);
    RUN_TEST(test_capacity_diagnostics);
    RUN_TEST(test_recursive_overflow_preserves_outer_frames);
    RUN_TEST(test_calibration_and_nested_subtraction);
    RUN_TEST(test_calibration_releases_mask_between_samples);
    RUN_TEST(test_stop_and_reset_cancel_pending_calibration);
    RUN_TEST(test_real_compiler_instrumentation);
    RUN_TEST(test_binary_crc_and_output_formats);
    return UNITY_END();
}
