execute_process(
    COMMAND "${COMPILER}" -DPROFILER_SCOPES_ENABLED "-I${PROFILER_DIR}"
        -c "${CMAKE_CURRENT_LIST_DIR}/test_profiler_scope_removed.c"
        -o "${OUTPUT}"
    RESULT_VARIABLE result
    ERROR_VARIABLE error)
if(result EQUAL 0 OR NOT error MATCHES "Profiler scopes were removed")
    message(FATAL_ERROR "Expected scope migration diagnostic, got: ${result}: ${error}")
endif()
