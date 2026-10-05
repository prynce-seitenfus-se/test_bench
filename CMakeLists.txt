cmake_minimum_required(VERSION 3.10)
project(RTOS_Test_Suite C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
enable_testing()

option(ENABLE_COVERAGE "Enable code coverage instrumentation (GCC/Clang)" OFF)

add_library(unity STATIC modules/Unity/src/unity.c)
target_include_directories(unity PUBLIC modules/Unity/src)

set(MODULE_INCLUDES 
    modules/atomic 
    modules/ring_buffer 
    modules/memory_pool
    modules/linked_list
    modules/bitmap
    modules/crc
    modules/fsm
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/sertos")
    set(SERTOS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/modules/sertos")
else()
    set(SERTOS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../sertos")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../profiler")
    set(PROFILER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../profiler")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/profiler")
    set(PROFILER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/modules/profiler")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/stream")
    set(STREAM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/modules/stream")
else()
    set(STREAM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../stream")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/hashmap")
    set(HASHMAP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/modules/hashmap")
else()
    set(HASHMAP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../hashmap")
endif()

list(APPEND MODULE_INCLUDES 
    ${SERTOS_DIR}/inc 
    ${SERTOS_DIR}/port
    ${PROFILER_DIR}
    ${STREAM_DIR}
    ${HASHMAP_DIR}
)

set(SERTOS_SOURCES
    ${SERTOS_DIR}/src/sertos_task.c
    ${SERTOS_DIR}/src/sertos_scheduler.c
    ${SERTOS_DIR}/src/sertos_stats.c
    ${SERTOS_DIR}/src/sertos_sem.c
    ${SERTOS_DIR}/src/sertos_mutex.c
    ${SERTOS_DIR}/src/sertos_queue.c
    ${SERTOS_DIR}/src/sertos_stream_buffer.c
    ${SERTOS_DIR}/src/sertos_timer.c
    ${SERTOS_DIR}/port/windows/port_windows.c
    modules/ring_buffer/ring_buffer.c
    modules/linked_list/linked_list.c
    modules/bitmap/bitmap.c
    modules/memory_pool/memory_pool.c
    modules/crc/crc.c
)

# --- Test Executables ---
# 1. Ring Buffer
add_executable(test_ring_buffer tests/test_ring_buffer.c modules/ring_buffer/ring_buffer.c)
target_link_libraries(test_ring_buffer PRIVATE unity)
target_include_directories(test_ring_buffer PRIVATE ${MODULE_INCLUDES})

# 2. Atomic (Header-only)
add_executable(test_atomic tests/test_atomic.c)
target_link_libraries(test_atomic PRIVATE unity)
target_include_directories(test_atomic PRIVATE ${MODULE_INCLUDES})

# 3. Memory Pool
add_executable(test_memory_pool tests/test_memory_pool.c modules/memory_pool/memory_pool.c)
target_link_libraries(test_memory_pool PRIVATE unity)
target_include_directories(test_memory_pool PRIVATE ${MODULE_INCLUDES})

# 4. Linked List
add_executable(test_linked_list tests/test_linked_list.c modules/linked_list/linked_list.c)
target_link_libraries(test_linked_list PRIVATE unity)
target_include_directories(test_linked_list PRIVATE ${MODULE_INCLUDES})

# 5. Bitmap
add_executable(test_bitmap tests/test_bitmap.c modules/bitmap/bitmap.c)
target_link_libraries(test_bitmap PRIVATE unity)
target_include_directories(test_bitmap PRIVATE ${MODULE_INCLUDES})

# 6. CRC
add_executable(test_crc tests/test_crc.c modules/crc/crc.c)
target_link_libraries(test_crc PRIVATE unity)
target_include_directories(test_crc PRIVATE ${MODULE_INCLUDES})

# 7. FSM
add_executable(test_fsm tests/test_fsm.c modules/fsm/fsm.c)
target_link_libraries(test_fsm PRIVATE unity)
target_include_directories(test_fsm PRIVATE ${MODULE_INCLUDES})

# 8. SertOS Task Management
add_executable(test_sertos_task tests/test_sertos_task.c ${SERTOS_SOURCES})
target_link_libraries(test_sertos_task PRIVATE unity)
target_include_directories(test_sertos_task PRIVATE ${MODULE_INCLUDES})

# 9. SertOS Preemptive Scheduler Core
add_executable(
    test_sertos_scheduler
    tests/test_sertos_scheduler.c
    ${SERTOS_SOURCES}
)
target_link_libraries(test_sertos_scheduler PRIVATE unity)
target_include_directories(test_sertos_scheduler PRIVATE ${MODULE_INCLUDES})

# 10. SertOS Semaphore Primitive
add_executable(test_sertos_sem tests/test_sertos_sem.c ${SERTOS_SOURCES})
target_link_libraries(test_sertos_sem PRIVATE unity)
target_include_directories(test_sertos_sem PRIVATE ${MODULE_INCLUDES})

# 11. SertOS Mutex (PIP) Primitive
add_executable(test_sertos_mutex tests/test_sertos_mutex.c ${SERTOS_SOURCES})
target_link_libraries(test_sertos_mutex PRIVATE unity)
target_include_directories(test_sertos_mutex PRIVATE ${MODULE_INCLUDES})

# 12. SertOS Message Queue Primitive
add_executable(test_sertos_queue tests/test_sertos_queue.c ${SERTOS_SOURCES})
target_link_libraries(test_sertos_queue PRIVATE unity)
target_include_directories(test_sertos_queue PRIVATE ${MODULE_INCLUDES})

# 13. SertOS Software Timers Primitive
add_executable(test_sertos_timer tests/test_sertos_timer.c ${SERTOS_SOURCES})
target_link_libraries(test_sertos_timer PRIVATE unity)
target_include_directories(test_sertos_timer PRIVATE ${MODULE_INCLUDES})

# 14. SertOS Execution Benchmarks (Latency, Jitter, Footprint)
add_executable(
    test_sertos_benchmark
    tests/test_sertos_benchmark.c
    ${SERTOS_SOURCES}
)
target_link_libraries(test_sertos_benchmark PRIVATE unity)
target_include_directories(test_sertos_benchmark PRIVATE ${MODULE_INCLUDES})

# 15. GCC Function Instrumentation Profiler
add_executable(test_profiler 
    tests/test_profiler.c 
    tests/test_profiler_targets.c 
    tests/profiler_port_stub.c
    ${PROFILER_DIR}/profiler.c
    ${HASHMAP_DIR}/hashmap.c
    ${STREAM_DIR}/stream.c
)
target_link_libraries(test_profiler PRIVATE unity)
target_include_directories(test_profiler PRIVATE ${MODULE_INCLUDES} "${PROFILER_DIR}/port")

# 15. Profiler transport contract and memory backend
add_executable(
    test_profiler_transport
    tests/test_profiler_transport.c
    tests/profiler_port_stub.c
    ${PROFILER_DIR}/profiler.c
    ${PROFILER_DIR}/port/profiler_transport_memory.c
    ${HASHMAP_DIR}/hashmap.c
    ${STREAM_DIR}/stream.c
)
target_link_libraries(test_profiler_transport PRIVATE unity)
target_include_directories(test_profiler_transport PRIVATE ${MODULE_INCLUDES} "${PROFILER_DIR}/port")

# Fake-HAL coverage for the UART DMA backend and timeout/error paths
add_executable(
    test_profiler_transport_uart
    tests/test_profiler_transport_uart.c
    tests/fakes/profiler_uart/fake_hal.c
    tests/profiler_port_stub.c
    ${PROFILER_DIR}/port/profiler_transport_uart_dma.c
    ${STREAM_DIR}/stream.c
)
target_compile_definitions(test_profiler_transport_uart PRIVATE STM32G070xx)
target_include_directories(test_profiler_transport_uart PRIVATE
    tests/fakes/profiler_uart
    ${MODULE_INCLUDES}
    "${PROFILER_DIR}/port"
)
target_link_libraries(test_profiler_transport_uart PRIVATE unity)

# Host coverage for the interrupt-driven HAL UART backend used by Renode.
add_executable(
    test_profiler_transport_uart_it
    tests/test_profiler_transport_uart_it.c
    tests/fakes/profiler_uart/fake_hal.c
    tests/profiler_port_stub.c
    ${PROFILER_DIR}/port/profiler_transport_uart_it.c
    ${STREAM_DIR}/stream.c
)
target_compile_definitions(test_profiler_transport_uart_it PRIVATE STM32G070xx)
target_include_directories(test_profiler_transport_uart_it PRIVATE
    tests/fakes/profiler_uart
    ${MODULE_INCLUDES}
    "${PROFILER_DIR}/port"
)
target_link_libraries(test_profiler_transport_uart_it PRIVATE unity)

# Port backends coverage
add_executable(
    test_profiler_port_win
    tests/test_profiler_port_win.c
    ${PROFILER_DIR}/port/profiler_port_win.c
)
target_include_directories(test_profiler_port_win PRIVATE
    ${MODULE_INCLUDES}
    "${PROFILER_DIR}/port"
)
target_link_libraries(test_profiler_port_win PRIVATE unity)

add_executable(
    test_profiler_port_16bit_it
    tests/test_profiler_port_16bit_it.c
    ${PROFILER_DIR}/port/profiler_port_16bit_it.c
)
target_include_directories(test_profiler_port_16bit_it PRIVATE
    ${MODULE_INCLUDES}
    "${PROFILER_DIR}/port"
)
target_link_libraries(test_profiler_port_16bit_it PRIVATE unity)

# Cortex-M SysTick clock-to-reload validation
add_executable(test_sertos_systick tests/test_sertos_systick.c)
target_link_libraries(test_sertos_systick PRIVATE unity)
target_include_directories(test_sertos_systick PRIVATE "${SERTOS_DIR}/port/arm")

# 16. SertOS Runtime Statistics (Telemetry)
add_executable(test_sertos_stats tests/test_sertos_stats.c ${SERTOS_SOURCES})
target_link_libraries(test_sertos_stats PRIVATE unity)
target_include_directories(test_sertos_stats PRIVATE ${MODULE_INCLUDES})
# The test supplies a deterministic fake sertos_port_runtime_counter() that
# overrides the host port's default; allow the duplicate and take the first
# (test) definition listed in the link line.
target_link_options(test_sertos_stats PRIVATE -Wl,--allow-multiple-definition)

# 17. SertOS Stream Buffer Primitive
add_executable(test_sertos_stream_buffer tests/test_sertos_stream_buffer.c ${SERTOS_SOURCES})
target_link_libraries(test_sertos_stream_buffer PRIVATE unity)
target_include_directories(test_sertos_stream_buffer PRIVATE ${MODULE_INCLUDES})

# Selectively instrument target application functions only
set_source_files_properties(
    tests/test_profiler_targets.c
    PROPERTIES COMPILE_OPTIONS "-finstrument-functions"
)

# 18. Stream Transport Layer
add_executable(test_stream tests/test_stream.c ${STREAM_DIR}/stream.c)
target_link_libraries(test_stream PRIVATE unity)
target_include_directories(test_stream PRIVATE ${MODULE_INCLUDES})

# 19. HashMap Module
add_executable(test_hashmap tests/test_hashmap.c ${HASHMAP_DIR}/hashmap.c)
target_link_libraries(test_hashmap PRIVATE unity)
target_include_directories(test_hashmap PRIVATE ${MODULE_INCLUDES})

set(ALL_TEST_TARGETS 
    test_ring_buffer 
    test_atomic 
    test_memory_pool 
    test_linked_list 
    test_bitmap 
    test_crc 
    test_fsm
    test_stream
    test_hashmap
    test_sertos_task
    test_sertos_scheduler
    test_sertos_sem
    test_sertos_mutex
    test_sertos_queue
    test_sertos_stream_buffer
    test_sertos_timer
    test_sertos_benchmark
    test_profiler
    test_profiler_transport
    test_profiler_transport_uart
    test_profiler_transport_uart_it
    test_sertos_systick
    test_sertos_stats
)

if(WIN32)
    set(SERTOS_TEST_TARGETS
        test_sertos_task
        test_sertos_scheduler
        test_sertos_sem
        test_sertos_mutex
        test_sertos_queue
        test_sertos_stream_buffer
        test_sertos_timer
        test_sertos_benchmark
        test_sertos_stats
    )
    foreach(TARGET ${SERTOS_TEST_TARGETS})
        target_link_libraries(${TARGET} PRIVATE winmm)
    endforeach()
endif()

foreach(TARGET ${ALL_TEST_TARGETS})
    target_compile_options(${TARGET} PRIVATE -Wall -Wextra -pedantic)
    if(ENABLE_COVERAGE)
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${TARGET} PRIVATE -fprofile-arcs -ftest-coverage -O0)
            target_link_options(${TARGET} PRIVATE -fprofile-arcs -ftest-coverage)
        else()
            message(WARNING "Coverage requested, but compiler is not GNU or Clang.")
        endif()
    endif()
endforeach()

# --- Cyclomatic Complexity & Maintainability Warnings (Lizard) ---
option(ENABLE_LIZARD_GATE "Report functions exceeding CCN 10, NLOC 75, or 5 parameters without failing the build" ON)

if(ENABLE_LIZARD_GATE)
    find_program(
        LIZARD_BIN
        NAMES lizard lizard.exe
        HINTS
            "$ENV{LOCALAPPDATA}/Python/pythoncore-3.14-64/Scripts"
            "$ENV{LOCALAPPDATA}/Programs/Python/Python*/Scripts"
            "$ENV{APPDATA}/Python/Scripts"
    )

    if(NOT LIZARD_BIN)
        find_program(PYTHON_BIN NAMES python3 python py)
        if(PYTHON_BIN)
            execute_process(
                COMMAND ${PYTHON_BIN} -m lizard --version
                RESULT_VARIABLE LIZARD_CHECK_RESULT
                OUTPUT_QUIET ERROR_QUIET
            )
            if(LIZARD_CHECK_RESULT EQUAL 0)
                set(LIZARD_BIN ${PYTHON_BIN} -m lizard)
            endif()
        endif()
    endif()

    if(NOT LIZARD_BIN)
        message(WARNING "Lizard code analyzer not found! Install via: pip install lizard. Disabling complexity gate.")
        set(ENABLE_LIZARD_GATE OFF)
    endif()

    if(LIZARD_BIN)
        set(COMPLEXITY_SCAN_DIRS)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src")
            list(APPEND COMPLEXITY_SCAN_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src")
        endif()
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules")
            list(APPEND COMPLEXITY_SCAN_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/modules")
        endif()
        if(EXISTS "${SERTOS_DIR}/src")
            list(APPEND COMPLEXITY_SCAN_DIRS "${SERTOS_DIR}/src")
        endif()
        if(EXISTS "${SERTOS_DIR}/port")
            list(APPEND COMPLEXITY_SCAN_DIRS "${SERTOS_DIR}/port")
        endif()
        if(EXISTS "${STREAM_DIR}")
            list(APPEND COMPLEXITY_SCAN_DIRS "${STREAM_DIR}")
        endif()
        if(EXISTS "${HASHMAP_DIR}")
            list(APPEND COMPLEXITY_SCAN_DIRS "${HASHMAP_DIR}")
        endif()
        if(EXISTS "${PROFILER_DIR}/port")
            list(APPEND COMPLEXITY_SCAN_DIRS "${PROFILER_DIR}/port")
        endif()
        if(NOT COMPLEXITY_SCAN_DIRS)
            set(COMPLEXITY_SCAN_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")
        endif()

        add_custom_target(
            check_complexity
            COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_CURRENT_SOURCE_DIR}"
                    ${LIZARD_BIN}
                    -C 10
                    -L 75
                    -a 5
                    -w
                    -i -1
                    -W whitelizard.txt
                    -x "*/Unity/*"
                    -x "*/build/*"
                    -x "*/CMakeFiles/*"
                    ${COMPLEXITY_SCAN_DIRS}
            COMMENT "Checking RTOS quality metrics (warnings only: Max CCN 10 | Max NLOC 75 | Max Params 5)..."
            VERBATIM
        )

        foreach(TARGET ${ALL_TEST_TARGETS})
            add_dependencies(${TARGET} check_complexity)
        endforeach()
    endif()
endif()

add_test(NAME Validation_RingBuffer COMMAND test_ring_buffer)
add_test(NAME Validation_Atomic COMMAND test_atomic)
add_test(NAME Validation_MemoryPool COMMAND test_memory_pool)
add_test(NAME Validation_LinkedList COMMAND test_linked_list)
add_test(NAME Validation_Bitmap COMMAND test_bitmap)
add_test(NAME Validation_CRC COMMAND test_crc)
add_test(NAME Validation_FSM COMMAND test_fsm)
add_test(NAME Validation_SertOSTask COMMAND test_sertos_task)
add_test(NAME Validation_SertOSScheduler COMMAND test_sertos_scheduler)
add_test(NAME Validation_SertOSSem COMMAND test_sertos_sem)
add_test(NAME Validation_SertOSMutex COMMAND test_sertos_mutex)
add_test(NAME Validation_SertOSQueue COMMAND test_sertos_queue)
add_test(NAME Validation_SertOSStreamBuffer COMMAND test_sertos_stream_buffer)
add_test(NAME Validation_SertOSTimer COMMAND test_sertos_timer)
add_test(NAME Validation_SertOSBenchmark COMMAND test_sertos_benchmark)
add_test(NAME Validation_Profiler COMMAND test_profiler)
add_test(NAME Validation_ProfilerPortWin COMMAND test_profiler_port_win)
add_test(NAME Validation_ProfilerPort16BitIt COMMAND test_profiler_port_16bit_it)
add_test(NAME Validation_ProfilerTransport COMMAND test_profiler_transport)
add_test(NAME Validation_ProfilerTransportUart COMMAND test_profiler_transport_uart)
add_test(NAME Validation_ProfilerTransportUartIt COMMAND test_profiler_transport_uart_it)
add_test(NAME Validation_SertOSSysTick COMMAND test_sertos_systick)
add_test(NAME Validation_SertOSStats COMMAND test_sertos_stats)
add_test(NAME Validation_Stream COMMAND test_stream)
add_test(NAME Validation_HashMap COMMAND test_hashmap)