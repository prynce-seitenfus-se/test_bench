#include "unity.h"
#include "profiler_port.h"

extern volatile uint32_t profiler_hardware_diagnostics[5][3];
void __wrap_profiler_port_init(void);
uint32_t __wrap_profiler_port_enter_critical(void);
void __wrap_profiler_port_exit_critical(uint32_t state);
void __wrap_SysTick_Handler(void);
void __wrap_TIM6_IRQHandler(void);
void __wrap_TIM1_BRK_UP_TRG_COM_IRQHandler(void);
void __wrap_profiler_context_switch(const void* prev, const void* next);

static uint32_t s_ticks;
static uint32_t s_mask;

void __real_profiler_port_init(void)
{
}

uint32_t profiler_port_ticks(void)
{
    return s_ticks;
}

uint32_t __real_profiler_port_enter_critical(void)
{
    uint32_t previous = s_mask;
    s_mask = 1U;
    return previous;
}

void __real_profiler_port_exit_critical(uint32_t state)
{
    s_mask = state;
}

void __real_SysTick_Handler(void) { s_ticks += 13U; }
void __real_TIM6_IRQHandler(void) { s_ticks += 17U; }
void __real_TIM1_BRK_UP_TRG_COM_IRQHandler(void) { s_ticks += 19U; }
void __real_profiler_context_switch(const void* prev, const void* next)
{
    (void)prev;
    (void)next;
    s_ticks += 23U;
}

void setUp(void)
{
    s_ticks = 0U;
    s_mask = 0U;
    for (size_t row = 0U; row < 5U; row++) {
        for (size_t column = 0U; column < 3U; column++) {
            profiler_hardware_diagnostics[row][column] = 0U;
        }
    }
    __wrap_profiler_port_init();
}

void tearDown(void)
{
    TEST_ASSERT_EQUAL_UINT32(0U, s_mask);
}

static void test_nested_sections_record_outer_duration_and_restore_mask(void)
{
    uint32_t outer = __wrap_profiler_port_enter_critical();
    s_ticks += 7U;
    uint32_t inner = __wrap_profiler_port_enter_critical();
    s_ticks += 11U;
    __wrap_profiler_port_exit_critical(inner);
    TEST_ASSERT_EQUAL_UINT32(1U, s_mask);
    TEST_ASSERT_EQUAL_UINT32(0U, profiler_hardware_diagnostics[0][0]);
    s_ticks += 5U;
    __wrap_profiler_port_exit_critical(outer);
    TEST_ASSERT_EQUAL_UINT32(1U, profiler_hardware_diagnostics[0][0]);
    TEST_ASSERT_EQUAL_UINT32(23U, profiler_hardware_diagnostics[0][1]);
}

static void test_irq_and_switch_samples_do_not_change_mask(void)
{
    __wrap_SysTick_Handler();
    __wrap_TIM6_IRQHandler();
    __wrap_TIM1_BRK_UP_TRG_COM_IRQHandler();
    __wrap_profiler_context_switch(NULL, NULL);
    const uint32_t expected[4] = {13U, 17U, 19U, 23U};
    for (size_t row = 1U; row < 5U; row++) {
        TEST_ASSERT_EQUAL_UINT32(1U, profiler_hardware_diagnostics[row][0]);
        TEST_ASSERT_EQUAL_UINT32(expected[row - 1U], profiler_hardware_diagnostics[row][1]);
        TEST_ASSERT_EQUAL_UINT32(expected[row - 1U], profiler_hardware_diagnostics[row][2]);
    }
}

static void test_counter_wrap_and_maximum(void)
{
    s_ticks = UINT32_MAX - 4U;
    uint32_t state = __wrap_profiler_port_enter_critical();
    s_ticks += 10U;
    __wrap_profiler_port_exit_critical(state);
    state = __wrap_profiler_port_enter_critical();
    s_ticks += 3U;
    __wrap_profiler_port_exit_critical(state);
    TEST_ASSERT_EQUAL_UINT32(2U, profiler_hardware_diagnostics[0][0]);
    TEST_ASSERT_EQUAL_UINT32(10U, profiler_hardware_diagnostics[0][1]);
    TEST_ASSERT_EQUAL_UINT32(13U, profiler_hardware_diagnostics[0][2]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_nested_sections_record_outer_duration_and_restore_mask);
    RUN_TEST(test_irq_and_switch_samples_do_not_change_mask);
    RUN_TEST(test_counter_wrap_and_maximum);
    return UNITY_END();
}
