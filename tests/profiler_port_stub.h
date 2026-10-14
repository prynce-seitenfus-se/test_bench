#ifndef PROFILER_PORT_STUB_H
#define PROFILER_PORT_STUB_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Selects automatic (+1 per read) or manual timestamp generation.
 *
 * @param[in] enabled true for auto-increment (default), false for manual ticks.
 */
void profiler_port_stub_set_auto_ticks(bool enabled);

/**
 * @brief Sets the timestamp returned by the next profiler_port_ticks() calls.
 *
 * @param[in] ticks Timestamp value (manual mode) or base value (auto mode).
 */
void profiler_port_stub_set_ticks(uint32_t ticks);

/**
 * @brief Sets the identifier returned by profiler_port_context_id().
 *
 * @param[in] id Execution context identifier.
 */
void profiler_port_stub_set_context(const void* id);

/** @brief Invokes a one-shot callback when the outer critical section exits. */
void profiler_port_stub_on_unlock(void (*callback)(void));

/** @brief Returns the longest outer critical-section duration in fake ticks. */
uint32_t profiler_port_stub_max_critical_ticks(void);

/** @brief Returns the number of outer critical-section exits. */
uint32_t profiler_port_stub_unlock_count(void);

#endif /* PROFILER_PORT_STUB_H */
