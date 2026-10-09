#include "unity.h"
#include "profiler_port.h"

static uint16_t s_mock_timer_val = 0U;
static bool     s_mock_init_called = false;
static bool     s_mock_pending = false;

uint16_t profiler_port_hardware_timer16_read_low(void)
{
    return s_mock_timer_val;
}

bool profiler_port_hardware_timer16_overflow_pending(void)
{
    return s_mock_pending;
}

uint16_t profiler_port_hardware_timer16_read_high(void)
{
    return 0U;
}

void profiler_port_hardware_timer16_init(void)
{
    s_mock_init_called = true;
    s_mock_timer_val = 0U;
}

void setUp(void)
{
    s_mock_init_called = false;
    s_mock_timer_val = 0U;
    s_mock_pending = false;
}

void tearDown(void)
{
}

void test_profiler_port_16bit_it_normal_and_overflow(void)
{
    profiler_port_init();
    TEST_ASSERT_TRUE(s_mock_init_called);

    /* Initial state */
    s_mock_timer_val = 100U;
    TEST_ASSERT_EQUAL_HEX32(0x00000064U, profiler_port_ticks());

    /* Advance timer */
    s_mock_timer_val = 65530U;
    TEST_ASSERT_EQUAL_HEX32(0x0000FFFAU, profiler_port_ticks());

    /* Simulate timer overflow ISR */
    profiler_port_16bit_it_overflow_isr();
    s_mock_timer_val = 5U;
    TEST_ASSERT_EQUAL_HEX32(0x00010005U, profiler_port_ticks());

    /* Second overflow */
    profiler_port_16bit_it_overflow_isr();
    s_mock_timer_val = 42U;
    TEST_ASSERT_EQUAL_HEX32(0x0002002AU, profiler_port_ticks());
}

void test_profiler_port_16bit_it_pending_overflow_while_masked(void)
{
    profiler_port_init();

    /* Wrapped, ISR pending (interrupts masked): high word is compensated */
    s_mock_timer_val = 65530U;
    TEST_ASSERT_EQUAL_HEX32(0x0000FFFAU, profiler_port_ticks());
    s_mock_pending = true;
    s_mock_timer_val = 12U;
    TEST_ASSERT_EQUAL_HEX32(0x0001000CU, profiler_port_ticks());

    /* Sampled just before the wrap while the flag is already set: no compensation */
    s_mock_timer_val = 65535U;
    TEST_ASSERT_EQUAL_HEX32(0x0000FFFFU, profiler_port_ticks());

    /* ISR runs once unmasked: same timestamps, no double count */
    profiler_port_16bit_it_overflow_isr();
    s_mock_pending = false;
    s_mock_timer_val = 20U;
    TEST_ASSERT_EQUAL_HEX32(0x00010014U, profiler_port_ticks());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_profiler_port_16bit_it_normal_and_overflow);
    RUN_TEST(test_profiler_port_16bit_it_pending_overflow_while_masked);
    return UNITY_END();
}
