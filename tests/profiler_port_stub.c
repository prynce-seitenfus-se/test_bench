#include "profiler_port.h"

static uint32_t s_test_timestamp;

void profiler_port_init(void)
{
    s_test_timestamp = 0U;
}

uint32_t profiler_port_ticks(void)
{
    s_test_timestamp++;
    return s_test_timestamp;
}

uint32_t profiler_port_enter_critical(void)
{
    return 0U;
}

void profiler_port_exit_critical(uint32_t state)
{
    (void)state;
}
