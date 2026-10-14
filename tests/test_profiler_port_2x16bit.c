#include "unity.h"
#include "profiler_port.h"

static uint16_t s_high_before;
static uint16_t s_high_after;
static uint16_t s_low;
static uint32_t s_high_reads;
static uint32_t s_low_reads;

uint16_t profiler_port_hardware_timer16_read_high(void)
{
    s_high_reads++;
    return (s_high_reads == 1U) ? s_high_before : s_high_after;
}

uint16_t profiler_port_hardware_timer16_read_low(void)
{
    s_low_reads++;
    return (uint16_t)(s_low + (uint16_t)((s_low_reads - 1U) * 62U));
}

void profiler_port_hardware_timer16_init(void)
{
}

void setUp(void)
{
    s_high_before = 7U;
    s_high_after = 7U;
    s_low = 100U;
    s_high_reads = 0U;
    s_low_reads = 0U;
}

void tearDown(void)
{
    TEST_ASSERT_EQUAL_UINT32(1U, s_low_reads);
    TEST_ASSERT_EQUAL_UINT32(2U, s_high_reads);
}

static void test_normal_sample(void)
{
    TEST_ASSERT_EQUAL_HEX32(0x00070064U, profiler_port_ticks());
}

static void test_wrap_before_low_sample_keeps_sample_point(void)
{
    s_high_after = 8U;
    s_low = 5U;
    TEST_ASSERT_EQUAL_HEX32(0x00080005U, profiler_port_ticks());
}

static void test_wrap_after_low_sample_keeps_old_high_word(void)
{
    s_high_after = 8U;
    s_low = 65530U;
    TEST_ASSERT_EQUAL_HEX32(0x0007FFFAU, profiler_port_ticks());
}

static void test_high_word_wrap(void)
{
    s_high_before = 65535U;
    s_high_after = 0U;
    s_low = 5U;
    TEST_ASSERT_EQUAL_HEX32(0x00000005U, profiler_port_ticks());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_normal_sample);
    RUN_TEST(test_wrap_before_low_sample_keeps_sample_point);
    RUN_TEST(test_wrap_after_low_sample_keeps_old_high_word);
    RUN_TEST(test_high_word_wrap);
    return UNITY_END();
}
