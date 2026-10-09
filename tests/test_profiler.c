#include "unity.h"
#include "profiler.h"
#include "profiler_port_stub.h"
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
#define TEST_CONTEXTS         (4U)

static HashMapEntry        s_map_entries[TEST_MAP_CAPACITY];
static ProfilerMetric      s_metrics[TEST_METRICS_CAPACITY];
static ProfilerStackFrame  s_stack_frames[TEST_STACK_DEPTH];
static ProfilerStackFrame  s_ctx_frames[TEST_CONTEXTS * TEST_STACK_DEPTH];
static ProfilerContext     s_contexts[TEST_CONTEXTS];

/* Synthetic function addresses and execution context identifiers */
#define FN_F     ((void*)0x6000U)
#define FN_G     ((void*)0x6100U)
#define FN_H     ((void*)0x6200U)
#define CTX_A    ((const void*)0xA000U)
#define CTX_B    ((const void*)0xB000U)
#define CTX_C    ((const void*)0xC000U)

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

/* Re-initializes the profiler in per-context mode with manual timestamps */
static void init_per_context(ProfilerTimeMode mode, size_t capacity)
{
    (void)memset(s_contexts, 0, sizeof(s_contexts));
    (void)memset(s_ctx_frames, 0, sizeof(s_ctx_frames));

    ProfilerConfig config = {
        .frequency        = 1000000U,
        .map_entries      = s_map_entries,
        .map_capacity     = TEST_MAP_CAPACITY,
        .metrics          = s_metrics,
        .metrics_capacity = TEST_METRICS_CAPACITY,
        .stack_frames     = s_ctx_frames,
        .stack_depth      = TEST_STACK_DEPTH,
        .contexts         = s_contexts,
        .context_capacity = capacity,
        .time_mode        = mode
    };
    TEST_ASSERT_TRUE(profiler_init(&config));
    profiler_port_stub_set_auto_ticks(false);
    profiler_start();
}

static void enter_at(const void* ctx, void* fn, uint32_t ts)
{
    profiler_port_stub_set_context(ctx);
    profiler_port_stub_set_ticks(ts);
    __cyg_profile_func_enter(fn, NULL);
}

static void exit_at(const void* ctx, void* fn, uint32_t ts)
{
    profiler_port_stub_set_context(ctx);
    profiler_port_stub_set_ticks(ts);
    __cyg_profile_func_exit(fn, NULL);
}

static void switch_at(const void* prev, const void* next, uint32_t ts)
{
    profiler_port_stub_set_ticks(ts);
    profiler_context_switch(prev, next);
}

static ProfilerMetric get_metric(const void* fn)
{
    ProfilerMetric metric;
    (void)memset(&metric, 0, sizeof(metric));
    TEST_ASSERT_TRUE(profiler_get_metric(fn, &metric));
    return metric;
}

static void test_profiler_init_validation(void)
{
    ProfilerConfig cfg;
    (void)memset(&cfg, 0, sizeof(cfg));
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
    TEST_ASSERT_EQUAL(PROFILER_TIME_WALL, profiler_time_mode());

    /* Active mode requires per-context slots */
    cfg.time_mode = PROFILER_TIME_ACTIVE;
    TEST_ASSERT_FALSE(profiler_init(&cfg));

    /* Unknown time mode */
    cfg.time_mode = (ProfilerTimeMode)7;
    cfg.contexts = s_contexts;
    cfg.context_capacity = TEST_CONTEXTS;
    TEST_ASSERT_FALSE(profiler_init(&cfg));

    /* Frame pool size overflow */
    cfg.time_mode = PROFILER_TIME_WALL;
    cfg.context_capacity = SIZE_MAX;
    TEST_ASSERT_FALSE(profiler_init(&cfg));

    /* Valid per-context active configuration */
    cfg.stack_frames = s_ctx_frames;
    cfg.context_capacity = TEST_CONTEXTS;
    cfg.time_mode = PROFILER_TIME_ACTIVE;
    TEST_ASSERT_TRUE(profiler_init(&cfg));
    TEST_ASSERT_EQUAL(PROFILER_TIME_ACTIVE, profiler_time_mode());
    TEST_ASSERT_EQUAL_PTR(&s_ctx_frames[TEST_STACK_DEPTH], s_contexts[1].frames);
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

    /* Expected bytes: 24 (header) + 2 * 24 (records) + 4 (crc) = 76 bytes */
    size_t expected_len = sizeof(ProfilerBinHeader) + (2U * sizeof(ProfilerMetric)) + sizeof(uint32_t);
    TEST_ASSERT_EQUAL_UINT32(24U, sizeof(ProfilerBinHeader));
    TEST_ASSERT_EQUAL_UINT32(expected_len, s_stream_buf_len);

    const ProfilerBinHeader* hdr = (const ProfilerBinHeader*)s_stream_buf;
    TEST_ASSERT_EQUAL_HEX32(PROFILER_BIN_MAGIC, hdr->magic);
    TEST_ASSERT_EQUAL_UINT16(PROFILER_BIN_VERSION, hdr->version);
    TEST_ASSERT_EQUAL_UINT16(0x0003U, hdr->version);
    TEST_ASSERT_EQUAL_UINT16(2U, hdr->record_count);
    TEST_ASSERT_EQUAL_UINT32(1000000U, hdr->frequency);
    TEST_ASSERT_EQUAL_UINT16(0U, hdr->dropped_functions);
    TEST_ASSERT_EQUAL_UINT16(0U, hdr->stack_overflows);
    TEST_ASSERT_EQUAL_UINT16(0U, hdr->unmatched_exits);
    TEST_ASSERT_EQUAL_UINT16(0U, hdr->stale_frames);
    TEST_ASSERT_EQUAL_UINT16(0U, hdr->context_overflows);
    TEST_ASSERT_EQUAL_UINT8(PROFILER_TIME_WALL, hdr->time_mode);
    TEST_ASSERT_EQUAL_UINT8(0U, hdr->reserved);
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

/* T1: two contexts inside the same function at once */
static void test_profiler_context_concurrent_same_function(void)
{
    init_per_context(PROFILER_TIME_WALL, TEST_CONTEXTS);

    enter_at(CTX_A, FN_F, 0U);
    enter_at(CTX_B, FN_F, 10U);
    exit_at(CTX_A, FN_F, 100U);
    exit_at(CTX_B, FN_F, 200U);

    ProfilerMetric m = get_metric(FN_F);
    TEST_ASSERT_EQUAL_UINT32(2U, m.call_count);
    TEST_ASSERT_EQUAL_UINT32(100U, m.min_cycles);
    TEST_ASSERT_EQUAL_UINT32(190U, m.max_cycles);
    TEST_ASSERT_EQUAL_UINT64(290U, m.total_cycles);
    TEST_ASSERT_EQUAL_UINT16(0U, profiler_unmatched_exit_count());
    TEST_ASSERT_EQUAL_UINT16(0U, profiler_stale_frame_count());
}

/* Replays the sertos_stm32g070/1.csv scheduler_delay interleaving */
static void replay_delay_pattern(const void* p, const void* q)
{
    enter_at(p, FN_F, 0U);
    enter_at(q, FN_F, 5U);
    exit_at(p, FN_F, 160000U);
    enter_at(p, FN_F, 160100U);
    exit_at(q, FN_F, 160700U);
}

/* T2 (Phase 0 regression): per-context pairing yields the true delays */
static void test_profiler_context_delay_regression(void)
{
    init_per_context(PROFILER_TIME_WALL, TEST_CONTEXTS);
    replay_delay_pattern(CTX_A, CTX_B);

    ProfilerMetric m = get_metric(FN_F);
    TEST_ASSERT_EQUAL_UINT32(2U, m.call_count);
    TEST_ASSERT_EQUAL_UINT32(160000U, m.min_cycles);
    TEST_ASSERT_EQUAL_UINT32(160695U, m.max_cycles);
}

/* T2 counterpart: the legacy shared stack mis-pairs the same sequence */
static void test_profiler_legacy_delay_mispairing(void)
{
    profiler_port_stub_set_auto_ticks(false);
    profiler_start();
    replay_delay_pattern(CTX_A, CTX_B);

    ProfilerMetric m = get_metric(FN_F);
    TEST_ASSERT_EQUAL_UINT32(2U, m.call_count);
    TEST_ASSERT_EQUAL_UINT32(600U, m.min_cycles);
}

/* T3: frames entered before stop/start belong to the old epoch */
static void test_profiler_context_epoch_reset(void)
{
    init_per_context(PROFILER_TIME_WALL, TEST_CONTEXTS);

    enter_at(CTX_A, FN_F, 0U);
    enter_at(CTX_A, FN_G, 10U);
    profiler_stop();
    profiler_start();
    exit_at(CTX_A, FN_G, 20U);
    exit_at(CTX_A, FN_F, 30U);

    TEST_ASSERT_EQUAL_UINT16(2U, profiler_unmatched_exit_count());
    TEST_ASSERT_EQUAL_UINT32(0U, profiler_tracked_count());
}

/* T3 variant: a dump starts a new epoch as well */
static void test_profiler_context_dump_epoch_reset(void)
{
    init_per_context(PROFILER_TIME_WALL, TEST_CONTEXTS);
    Stream stream = stream_init(NULL, mock_stream_write, NULL, NULL);

    enter_at(CTX_A, FN_F, 0U);
    TEST_ASSERT_TRUE(profiler_dump(&stream));
    TEST_ASSERT_TRUE(profiler_enabled());
    exit_at(CTX_A, FN_F, 50U);

    TEST_ASSERT_EQUAL_UINT16(1U, profiler_unmatched_exit_count());
    TEST_ASSERT_EQUAL_UINT32(0U, profiler_tracked_count());
}

/* T4: a callee whose exit was lost is discarded as stale */
static void test_profiler_context_stale_frames(void)
{
    init_per_context(PROFILER_TIME_WALL, TEST_CONTEXTS);

    enter_at(CTX_A, FN_F, 0U);
    enter_at(CTX_A, FN_G, 10U);
    exit_at(CTX_A, FN_F, 40U);

    TEST_ASSERT_EQUAL_UINT32(40U, get_metric(FN_F).max_cycles);
    TEST_ASSERT_EQUAL_UINT16(1U, profiler_stale_frame_count());
    TEST_ASSERT_EQUAL_UINT32(0U, s_contexts[0].index);

    exit_at(CTX_A, FN_G, 50U);
    TEST_ASSERT_EQUAL_UINT16(1U, profiler_unmatched_exit_count());
}

/* T5: more contexts than slots */
static void test_profiler_context_overflow(void)
{
    init_per_context(PROFILER_TIME_WALL, 2U);

    enter_at(CTX_A, FN_F, 0U);
    enter_at(CTX_B, FN_F, 5U);
    enter_at(CTX_C, FN_F, 6U);
    exit_at(CTX_C, FN_F, 7U);
    TEST_ASSERT_EQUAL_UINT16(2U, profiler_context_overflow_count());

    exit_at(CTX_A, FN_F, 100U);
    exit_at(CTX_B, FN_F, 205U);

    ProfilerMetric m = get_metric(FN_F);
    TEST_ASSERT_EQUAL_UINT32(2U, m.call_count);
    TEST_ASSERT_EQUAL_UINT32(100U, m.min_cycles);
    TEST_ASSERT_EQUAL_UINT32(200U, m.max_cycles);
}

/* T6: depth overflow is confined to one context */
static void test_profiler_context_depth_overflow_isolated(void)
{
    init_per_context(PROFILER_TIME_WALL, TEST_CONTEXTS);

    for (uint32_t i = 0U; i < (TEST_STACK_DEPTH + 1U); ++i) {
        enter_at(CTX_A, (void*)(uintptr_t)(0x7000U + i), i);
    }
    TEST_ASSERT_EQUAL_UINT16(1U, profiler_stack_overflow_count());

    enter_at(CTX_B, FN_F, 100U);
    exit_at(CTX_B, FN_F, 130U);
    TEST_ASSERT_EQUAL_UINT32(30U, get_metric(FN_F).max_cycles);
    TEST_ASSERT_EQUAL_UINT32(TEST_STACK_DEPTH, s_contexts[0].index);
}

/* T7: active mode excludes the switched-out interval */
static void test_profiler_active_excludes_switched_out(void)
{
    init_per_context(PROFILER_TIME_ACTIVE, TEST_CONTEXTS);

    switch_at(NULL, CTX_A, 0U);
    enter_at(CTX_A, FN_F, 0U);
    switch_at(CTX_A, CTX_B, 10U);
    switch_at(CTX_B, CTX_A, 90U);
    exit_at(CTX_A, FN_F, 100U);

    TEST_ASSERT_EQUAL_UINT32(20U, get_metric(FN_F).max_cycles);
}

/* T8: nested frames both exclude a switch that happens inside the callee */
static void test_profiler_active_nested_switch(void)
{
    init_per_context(PROFILER_TIME_ACTIVE, TEST_CONTEXTS);

    switch_at(NULL, CTX_A, 0U);
    enter_at(CTX_A, FN_F, 0U);
    enter_at(CTX_A, FN_G, 10U);
    switch_at(CTX_A, CTX_B, 20U);
    enter_at(CTX_B, FN_H, 25U);
    exit_at(CTX_B, FN_H, 45U);
    switch_at(CTX_B, CTX_A, 70U);
    exit_at(CTX_A, FN_G, 80U);
    exit_at(CTX_A, FN_F, 100U);

    TEST_ASSERT_EQUAL_UINT32(20U, get_metric(FN_G).max_cycles);
    TEST_ASSERT_EQUAL_UINT32(50U, get_metric(FN_F).max_cycles);
    TEST_ASSERT_EQUAL_UINT32(20U, get_metric(FN_H).max_cycles);
}

/* Interrupt nested in a task frame */
static void run_isr_inside_task(void)
{
    enter_at(CTX_A, FN_F, 0U);
    enter_at(PROFILER_CONTEXT_ISR, FN_H, 10U);
    exit_at(PROFILER_CONTEXT_ISR, FN_H, 30U);
    exit_at(CTX_A, FN_F, 100U);
}

/* T9: ISR frames use their own stack; task frames stay intact */
static void test_profiler_context_isr_nested(void)
{
    init_per_context(PROFILER_TIME_WALL, TEST_CONTEXTS);
    run_isr_inside_task();

    TEST_ASSERT_EQUAL_UINT32(100U, get_metric(FN_F).max_cycles);
    TEST_ASSERT_EQUAL_UINT32(20U, get_metric(FN_H).max_cycles);
    TEST_ASSERT_EQUAL_UINT16(0U, profiler_stale_frame_count());
}

/* Phase 7: in active mode instrumented ISR time is charged to the preempted task */
static void test_profiler_active_excludes_isr_time(void)
{
    init_per_context(PROFILER_TIME_ACTIVE, TEST_CONTEXTS);
    switch_at(NULL, CTX_A, 0U);
    run_isr_inside_task();

    TEST_ASSERT_EQUAL_UINT32(80U, get_metric(FN_F).max_cycles);
    TEST_ASSERT_EQUAL_UINT32(20U, get_metric(FN_H).max_cycles);
}

/* T10: legacy mode keeps the single-stack results for nested calls */
static void test_profiler_legacy_nested_unchanged(void)
{
    profiler_port_stub_set_auto_ticks(false);
    profiler_start();

    enter_at(NULL, FN_F, 0U);
    enter_at(NULL, FN_G, 10U);
    exit_at(NULL, FN_G, 25U);
    exit_at(NULL, FN_F, 40U);

    TEST_ASSERT_EQUAL_UINT32(15U, get_metric(FN_G).max_cycles);
    TEST_ASSERT_EQUAL_UINT32(40U, get_metric(FN_F).max_cycles);
    TEST_ASSERT_EQUAL_UINT16(0U, profiler_unmatched_exit_count());
}

/* T11: reset unbinds every context and clears diagnostics */
static void test_profiler_context_reset(void)
{
    init_per_context(PROFILER_TIME_WALL, TEST_CONTEXTS);

    enter_at(CTX_A, FN_F, 0U);
    enter_at(CTX_B, FN_G, 0U);
    exit_at(CTX_A, FN_G, 1U);
    TEST_ASSERT_EQUAL_UINT16(1U, profiler_unmatched_exit_count());

    profiler_reset();
    for (size_t i = 0U; i < TEST_CONTEXTS; ++i) {
        TEST_ASSERT_FALSE(s_contexts[i].in_use);
        TEST_ASSERT_EQUAL_UINT32(0U, s_contexts[i].index);
    }
    TEST_ASSERT_EQUAL_UINT16(0U, profiler_unmatched_exit_count());
    TEST_ASSERT_EQUAL_UINT16(0U, profiler_stale_frame_count());
    TEST_ASSERT_EQUAL_UINT16(0U, profiler_context_overflow_count());
    TEST_ASSERT_EQUAL_UINT32(0U, profiler_tracked_count());
}

/* T12: release then rebind the same id gives a fresh stack */
static void test_profiler_context_release_rebind(void)
{
    init_per_context(PROFILER_TIME_WALL, TEST_CONTEXTS);

    enter_at(CTX_A, FN_F, 0U);
    TEST_ASSERT_TRUE(s_contexts[0].in_use);
    profiler_context_release(CTX_A);
    TEST_ASSERT_FALSE(s_contexts[0].in_use);
    profiler_context_release(CTX_B);

    exit_at(CTX_A, FN_F, 10U);
    TEST_ASSERT_EQUAL_UINT16(1U, profiler_unmatched_exit_count());
    TEST_ASSERT_TRUE(s_contexts[0].in_use);

    enter_at(CTX_A, FN_F, 20U);
    exit_at(CTX_A, FN_F, 35U);
    TEST_ASSERT_EQUAL_UINT32(15U, get_metric(FN_F).max_cycles);
}

/* Reference CRC-32 used to validate the dump trailer */
static uint32_t reference_crc32(const uint8_t* data, size_t length)
{
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0U; i < length; ++i) {
        crc ^= (uint32_t)data[i];
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            crc = ((crc & 1U) != 0U) ? ((crc >> 1U) ^ 0xEDB88320U) : (crc >> 1U);
        }
    }
    return crc ^ 0xFFFFFFFFU;
}

/* T13: v3 header carries the new diagnostics, time mode, and a valid CRC */
static void test_profiler_v3_dump_diagnostics(void)
{
    init_per_context(PROFILER_TIME_ACTIVE, 1U);
    Stream stream = stream_init(NULL, mock_stream_write, NULL, NULL);

    enter_at(CTX_A, FN_F, 0U);
    enter_at(CTX_A, FN_G, 1U);
    exit_at(CTX_A, FN_F, 2U);
    exit_at(CTX_A, FN_H, 3U);
    enter_at(CTX_B, FN_F, 4U);
    TEST_ASSERT_TRUE(profiler_dump(&stream));

    ProfilerBinHeader hdr;
    (void)memcpy(&hdr, s_stream_buf, sizeof(hdr));
    TEST_ASSERT_EQUAL_UINT16(1U, hdr.record_count);
    TEST_ASSERT_EQUAL_UINT16(1U, hdr.unmatched_exits);
    TEST_ASSERT_EQUAL_UINT16(1U, hdr.stale_frames);
    TEST_ASSERT_EQUAL_UINT16(1U, hdr.context_overflows);
    TEST_ASSERT_EQUAL_UINT8(PROFILER_TIME_ACTIVE, hdr.time_mode);

    size_t payload = sizeof(ProfilerBinHeader) + sizeof(ProfilerMetric);
    uint32_t trailer;
    (void)memcpy(&trailer, &s_stream_buf[payload], sizeof(trailer));
    TEST_ASSERT_EQUAL_HEX32(reference_crc32(s_stream_buf, payload), trailer);
}

/* T14: 32-bit timer wrap between entry and exit */
static void test_profiler_context_timer_wrap(void)
{
    init_per_context(PROFILER_TIME_WALL, TEST_CONTEXTS);

    enter_at(CTX_A, FN_F, 0xFFFFFFF0U);
    exit_at(CTX_A, FN_F, 0x00000010U);

    TEST_ASSERT_EQUAL_UINT32(0x20U, get_metric(FN_F).max_cycles);
}

/* Single NULL id (weak default port) behaves as one per-context stack */
static void test_profiler_context_null_id_single_context(void)
{
    init_per_context(PROFILER_TIME_WALL, TEST_CONTEXTS);

    enter_at(NULL, FN_F, 0U);
    exit_at(NULL, FN_F, 9U);

    TEST_ASSERT_EQUAL_UINT32(9U, get_metric(FN_F).max_cycles);
    TEST_ASSERT_TRUE(s_contexts[0].in_use);
    TEST_ASSERT_FALSE(s_contexts[1].in_use);
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
    RUN_TEST(test_profiler_context_concurrent_same_function);
    RUN_TEST(test_profiler_context_delay_regression);
    RUN_TEST(test_profiler_legacy_delay_mispairing);
    RUN_TEST(test_profiler_context_epoch_reset);
    RUN_TEST(test_profiler_context_dump_epoch_reset);
    RUN_TEST(test_profiler_context_stale_frames);
    RUN_TEST(test_profiler_context_overflow);
    RUN_TEST(test_profiler_context_depth_overflow_isolated);
    RUN_TEST(test_profiler_active_excludes_switched_out);
    RUN_TEST(test_profiler_active_nested_switch);
    RUN_TEST(test_profiler_context_isr_nested);
    RUN_TEST(test_profiler_active_excludes_isr_time);
    RUN_TEST(test_profiler_legacy_nested_unchanged);
    RUN_TEST(test_profiler_context_reset);
    RUN_TEST(test_profiler_context_release_rebind);
    RUN_TEST(test_profiler_v3_dump_diagnostics);
    RUN_TEST(test_profiler_context_timer_wrap);
    RUN_TEST(test_profiler_context_null_id_single_context);
    return UNITY_END();
}
