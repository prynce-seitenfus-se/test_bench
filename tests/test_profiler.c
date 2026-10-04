#include "unity.h"
#include "profiler.h"
#include "stream.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Compiler instrumentation hooks tested directly */
extern void __cyg_profile_func_enter(void* this_fn, void* call_site);
extern void __cyg_profile_func_exit(void* this_fn, void* call_site);

#define TEST_MAP_CAPACITY     (16U)
#define TEST_METRICS_CAPACITY (12U)
#define TEST_STACK_DEPTH      (8U)

static HashMapEntry        s_map_entries[TEST_MAP_CAPACITY];
static ProfilerMetric      s_metrics[TEST_METRICS_CAPACITY];
static ProfilerStackFrame  s_stack_frames[TEST_STACK_DEPTH];

/* Mock stream transmission buffer */
#define MOCK_STREAM_BUF_SIZE (512U)
static uint8_t s_stream_buf[MOCK_STREAM_BUF_SIZE];
static size_t  s_stream_buf_len = 0U;

/* External declarations of instrumented functions */
extern uint32_t profiler_test_target_add(uint32_t a, uint32_t b);
extern uint32_t profiler_test_target_sub(uint32_t a, uint32_t b);

static size_t mock_stream_write(void* context, const uint8_t* buffer, size_t size)
{
    (void)context;
    if ((s_stream_buf_len + size) <= MOCK_STREAM_BUF_SIZE) {
        (void)memcpy(&s_stream_buf[s_stream_buf_len], buffer, size);
        s_stream_buf_len += size;
        return size;
    }
    return 0U;
}

void setUp(void)
{
    (void)memset(s_map_entries, 0, sizeof(s_map_entries));
    (void)memset(s_metrics, 0, sizeof(s_metrics));
    (void)memset(s_stack_frames, 0, sizeof(s_stack_frames));
    (void)memset(s_stream_buf, 0, sizeof(s_stream_buf));
    s_stream_buf_len = 0U;

    ProfilerConfig config = {
        .frequency        = 1000000U,
        .map_entries      = s_map_entries,
        .map_capacity     = TEST_MAP_CAPACITY,
        .metrics          = s_metrics,
        .metrics_capacity = TEST_METRICS_CAPACITY,
        .stack_frames     = s_stack_frames,
        .stack_depth      = TEST_STACK_DEPTH
    };
    (void)profiler_init(&config);
}

void tearDown(void)
{
    profiler_stop();
}

static void test_profiler_init_validation(void)
{
    ProfilerConfig cfg;

    /* NULL config */
    TEST_ASSERT_FALSE(profiler_init(NULL));
    TEST_ASSERT_EQUAL_UINT32(0U, profiler_frequency());
    TEST_ASSERT_FALSE(profiler_enabled());

    /* NULL map entries */
    cfg.frequency = 1000U;
    cfg.map_entries = NULL;
    cfg.map_capacity = TEST_MAP_CAPACITY;
    cfg.metrics = s_metrics;
    cfg.metrics_capacity = TEST_METRICS_CAPACITY;
    cfg.stack_frames = s_stack_frames;
    cfg.stack_depth = TEST_STACK_DEPTH;
    TEST_ASSERT_FALSE(profiler_init(&cfg));

    /* Non power of two map capacity */
    cfg.map_entries = s_map_entries;
    cfg.map_capacity = 15U;
    TEST_ASSERT_FALSE(profiler_init(&cfg));

    /* NULL metrics array */
    cfg.map_capacity = TEST_MAP_CAPACITY;
    cfg.metrics = NULL;
    TEST_ASSERT_FALSE(profiler_init(&cfg));

    /* NULL stack frames */
    cfg.metrics = s_metrics;
    cfg.stack_frames = NULL;
    TEST_ASSERT_FALSE(profiler_init(&cfg));

    /* Valid configuration */
    cfg.stack_frames = s_stack_frames;
    TEST_ASSERT_TRUE(profiler_init(&cfg));
    TEST_ASSERT_EQUAL_UINT32(1000U, profiler_frequency());
    TEST_ASSERT_EQUAL_UINT32(0U, profiler_tracked_count());
}

static void test_profiler_start_stop_gate(void)
{
    TEST_ASSERT_FALSE(profiler_enabled());

    /* Calls when disabled must not record anything */
    __cyg_profile_func_enter((void*)0x1234U, (void*)0x5678U);
    __cyg_profile_func_exit((void*)0x1234U, (void*)0x5678U);

    ProfilerMetric metric;
    TEST_ASSERT_FALSE(profiler_get_metric((void*)0x1234U, &metric));
    TEST_ASSERT_EQUAL_UINT32(0U, profiler_tracked_count());

    /* Enable and record */
    profiler_start();
    TEST_ASSERT_TRUE(profiler_enabled());

    __cyg_profile_func_enter((void*)0x1111U, (void*)0x2222U);
    __cyg_profile_func_exit((void*)0x1111U, (void*)0x2222U);

    TEST_ASSERT_TRUE(profiler_get_metric((void*)0x1111U, &metric));
    TEST_ASSERT_EQUAL_UINT32(1U, metric.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, profiler_tracked_count());

    /* Stop profiling */
    profiler_stop();
    TEST_ASSERT_FALSE(profiler_enabled());

    __cyg_profile_func_enter((void*)0x1111U, (void*)0x2222U);
    __cyg_profile_func_exit((void*)0x1111U, (void*)0x2222U);

    /* Call count unchanged */
    TEST_ASSERT_TRUE(profiler_get_metric((void*)0x1111U, &metric));
    TEST_ASSERT_EQUAL_UINT32(1U, metric.call_count);
}

static void test_profiler_metric_accumulation_and_reset(void)
{
    profiler_start();

    void* fn = (void*)0x2000U;

    /* Simulate multiple calls with simulated ticks */
    for (uint32_t i = 0U; i < 5U; ++i) {
        __cyg_profile_func_enter(fn, (void*)0x0U);
        __cyg_profile_func_exit(fn, (void*)0x0U);
    }

    ProfilerMetric metric;
    TEST_ASSERT_TRUE(profiler_get_metric(fn, &metric));
    TEST_ASSERT_EQUAL_UINT32(5U, metric.call_count);
    TEST_ASSERT_TRUE(metric.total_cycles > 0U);
    TEST_ASSERT_TRUE(metric.min_cycles > 0U);
    TEST_ASSERT_TRUE(metric.max_cycles >= metric.min_cycles);

    /* Reset profiler */
    profiler_reset();
    TEST_ASSERT_EQUAL_UINT32(0U, profiler_tracked_count());
    TEST_ASSERT_FALSE(profiler_get_metric(fn, &metric));
}

static void test_profiler_shadow_stack_overflow_handling(void)
{
    profiler_start();

    /* Exceed stack depth (TEST_STACK_DEPTH = 8) */
    for (uint32_t i = 0U; i < 12U; ++i) {
        __cyg_profile_func_enter((void*)(uintptr_t)(0x3000U + i), (void*)0x0U);
    }

    /* 4 calls beyond depth 8 should be registered as stack overflows */
    TEST_ASSERT_EQUAL_UINT16(4U, profiler_stack_overflow_count());

    /* Unwind all calls */
    for (int32_t i = 11; i >= 0; --i) {
        __cyg_profile_func_exit((void*)(uintptr_t)(0x3000U + (uint32_t)i), (void*)0x0U);
    }

    /* Only the 8 calls that fit on the shadow stack are recorded */
    TEST_ASSERT_EQUAL_UINT32(8U, profiler_tracked_count());
}

static void test_profiler_binary_dump(void)
{
    profiler_start();

    void* fn1 = (void*)0x4000U;
    void* fn2 = (void*)0x5000U;

    __cyg_profile_func_enter(fn1, (void*)0x0U);
    __cyg_profile_func_exit(fn1, (void*)0x0U);

    __cyg_profile_func_enter(fn2, (void*)0x0U);
    __cyg_profile_func_exit(fn2, (void*)0x0U);

    Stream stream = stream_init(NULL, mock_stream_write, NULL, NULL);

    TEST_ASSERT_TRUE(profiler_dump(&stream));

    /* Expected bytes: 16 (header) + 2 * 24 (records) + 4 (crc) = 68 bytes */
    size_t expected_len = sizeof(ProfilerBinHeader) + (2U * sizeof(ProfilerMetric)) + sizeof(uint32_t);
    TEST_ASSERT_EQUAL_UINT32(expected_len, s_stream_buf_len);

    const ProfilerBinHeader* hdr = (const ProfilerBinHeader*)s_stream_buf;
    TEST_ASSERT_EQUAL_HEX32(PROFILER_BIN_MAGIC, hdr->magic);
    TEST_ASSERT_EQUAL_UINT16(PROFILER_BIN_VERSION, hdr->version);
    TEST_ASSERT_EQUAL_UINT16(2U, hdr->record_count);
    TEST_ASSERT_EQUAL_UINT32(1000000U, hdr->frequency);
    TEST_ASSERT_EQUAL_UINT16(0U, hdr->dropped_functions);
    TEST_ASSERT_EQUAL_UINT16(0U, hdr->stack_overflows);
}

static void test_profiler_instrumented_targets(void)
{
    profiler_start();

    uint32_t sum = profiler_test_target_add(10U, 20U);
    TEST_ASSERT_EQUAL_UINT32(30U, sum);

    uint32_t diff = profiler_test_target_sub(50U, 15U);
    TEST_ASSERT_EQUAL_UINT32(35U, diff);

    profiler_stop();

    TEST_ASSERT_EQUAL_UINT32(2U, profiler_tracked_count());

    ProfilerMetric m_add;
    TEST_ASSERT_TRUE(profiler_get_metric((const void*)(uintptr_t)profiler_test_target_add, &m_add));
    TEST_ASSERT_EQUAL_UINT32(1U, m_add.call_count);
    TEST_ASSERT_TRUE(m_add.total_cycles > 0U);

    ProfilerMetric m_sub;
    TEST_ASSERT_TRUE(profiler_get_metric((const void*)(uintptr_t)profiler_test_target_sub, &m_sub));
    TEST_ASSERT_EQUAL_UINT32(1U, m_sub.call_count);
    TEST_ASSERT_TRUE(m_sub.total_cycles > 0U);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_profiler_init_validation);
    RUN_TEST(test_profiler_start_stop_gate);
    RUN_TEST(test_profiler_metric_accumulation_and_reset);
    RUN_TEST(test_profiler_shadow_stack_overflow_handling);
    RUN_TEST(test_profiler_binary_dump);
    RUN_TEST(test_profiler_instrumented_targets);
    return UNITY_END();
}
