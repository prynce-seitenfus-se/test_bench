#include "unity.h"
#include "profiler_port.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_profiler_port_win_init_and_ticks(void)
{
    profiler_port_init();

    uint32_t t1 = profiler_port_ticks();
    uint32_t t2 = profiler_port_ticks();

    /* Monotonic non-decreasing check */
    TEST_ASSERT_TRUE(t2 >= t1);
}

void test_profiler_port_win_critical_section(void)
{
    uint32_t state = profiler_port_enter_critical();
    /* Should succeed without deadlocking */
    profiler_port_exit_critical(state);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_profiler_port_win_init_and_ticks);
    RUN_TEST(test_profiler_port_win_critical_section);
    return UNITY_END();
}
