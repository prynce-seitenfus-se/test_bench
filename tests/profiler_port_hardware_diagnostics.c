#include "profiler.h"
#include "profiler_port.h"

/* Read this array through ST-LINK before issuing UART capture commands.
 * Rows: critical section, SysTick, TIM6, TIM1, context-switch callback.
 * Columns: completed samples, maximum ticks, accumulated ticks (modulo 2^32).
 * Wrappers perturb execution; IRQ samples exclude hardware exception entry/return.
 * Critical samples exclude mask/restore instructions and wrapper tail bookkeeping.
 */
volatile uint32_t profiler_hardware_diagnostics[5][3];

static bool s_timer_ready = false;
static uint32_t s_critical_depth = 0U;
static uint32_t s_critical_start = 0U;

NO_INST void __real_profiler_port_init(void);
NO_INST uint32_t __real_profiler_port_enter_critical(void);
NO_INST void __real_profiler_port_exit_critical(uint32_t state);
NO_INST void __real_SysTick_Handler(void);
NO_INST void __real_TIM6_IRQHandler(void);
NO_INST void __real_TIM1_BRK_UP_TRG_COM_IRQHandler(void);
NO_INST void __real_profiler_context_switch(const void* prev, const void* next);

NO_INST static void record_sample(size_t row, uint32_t ticks)
{
    uint32_t state = __real_profiler_port_enter_critical();
    profiler_hardware_diagnostics[row][0]++;
    if (ticks > profiler_hardware_diagnostics[row][1]) {
        profiler_hardware_diagnostics[row][1] = ticks;
    }
    profiler_hardware_diagnostics[row][2] += ticks;
    __real_profiler_port_exit_critical(state);
}

NO_INST void __wrap_profiler_port_init(void)
{
    __real_profiler_port_init();
    s_timer_ready = true;
}

NO_INST uint32_t __wrap_profiler_port_enter_critical(void)
{
    uint32_t state = __real_profiler_port_enter_critical();
    if (s_timer_ready) {
        if (s_critical_depth == 0U) {
            s_critical_start = profiler_port_ticks();
        }
        s_critical_depth++;
    }
    return state;
}

NO_INST void __wrap_profiler_port_exit_critical(uint32_t state)
{
    if (s_timer_ready && (s_critical_depth > 0U)) {
        s_critical_depth--;
        if (s_critical_depth == 0U) {
            uint32_t ticks = profiler_port_ticks() - s_critical_start;
            record_sample(0U, ticks);
        }
    }
    __real_profiler_port_exit_critical(state);
}

NO_INST void __wrap_SysTick_Handler(void)
{
    uint32_t start = profiler_port_ticks();
    __real_SysTick_Handler();
    if (s_timer_ready) {
        record_sample(1U, profiler_port_ticks() - start);
    }
}

NO_INST void __wrap_TIM6_IRQHandler(void)
{
    uint32_t start = profiler_port_ticks();
    __real_TIM6_IRQHandler();
    if (s_timer_ready) {
        record_sample(2U, profiler_port_ticks() - start);
    }
}

NO_INST void __wrap_TIM1_BRK_UP_TRG_COM_IRQHandler(void)
{
    uint32_t start = profiler_port_ticks();
    __real_TIM1_BRK_UP_TRG_COM_IRQHandler();
    if (s_timer_ready) {
        record_sample(3U, profiler_port_ticks() - start);
    }
}

NO_INST void __wrap_profiler_context_switch(const void* prev, const void* next)
{
    uint32_t start = profiler_port_ticks();
    __real_profiler_context_switch(prev, next);
    if (s_timer_ready) {
        record_sample(4U, profiler_port_ticks() - start);
    }
}
