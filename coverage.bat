@echo off
set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
pushd "%SCRIPT_DIR%"

:: Ensure CMake and the default MinGW toolchain are available in PATH
where cmake.exe >nul 2>nul
if errorlevel 1 (
    set "PATH=C:\Program Files\CMake\bin;%PATH%"
)
set "MINGW_BIN=C:\toolchains\mingw64\13.2.0\bin"
if exist "%MINGW_BIN%\gcc.exe" (
    set "PATH=%MINGW_BIN%;%PATH%"
)
where gcc.exe >nul 2>nul
if errorlevel 1 (
    echo [ERROR] MinGW GCC not found. Expected "%MINGW_BIN%\gcc.exe" or gcc.exe in PATH.
    popd
    exit /b 1
)
where gcov.exe >nul 2>nul
if errorlevel 1 (
    echo [ERROR] gcov.exe not found. Expected "%MINGW_BIN%\gcov.exe" or gcov.exe in PATH.
    popd
    exit /b 1
)

echo ============================================================
echo [COVERAGE] Configuring CMake with coverage enabled...
echo ============================================================
cmake -B build -S . -G "MinGW Makefiles" -DENABLE_COVERAGE=ON
if errorlevel 1 (
    echo [ERROR] CMake configuration failed.
    popd
    exit /b %errorlevel%
)

echo.
echo ============================================================
echo [COVERAGE] Building test suite with coverage instrumentation...
echo ============================================================
cmake --build build
if errorlevel 1 (
    echo [ERROR] Compilation failed.
    popd
    exit /b %errorlevel%
)

:: Clear stale execution counters before test run
del /s /q "build\*.gcda" >nul 2>nul

echo.
echo ============================================================
echo [COVERAGE] Running test suite...
echo ============================================================
build\test_ring_buffer.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )
build\test_atomic.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )
build\test_memory_pool.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )
build\test_linked_list.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )
build\test_bitmap.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )
build\test_crc.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )
build\test_fsm.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )
build\test_sertos_task.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )
build\test_sertos_scheduler.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )
build\test_sertos_sem.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )
build\test_sertos_mutex.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )
build\test_sertos_queue.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )
build\test_sertos_stream_buffer.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )
build\test_sertos_timer.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )
if exist "build\test_profiler.exe" (
    build\test_profiler.exe
    if errorlevel 1 ( popd & exit /b %errorlevel% )
)

echo.
echo ============================================================
echo [COVERAGE] Generating gcov coverage reports...
echo ============================================================
if not exist "coverage" mkdir "coverage"
pushd "coverage"

gcov -b -o "..\build\CMakeFiles\test_ring_buffer.dir\modules\ring_buffer\ring_buffer.c.obj" "..\modules\ring_buffer\ring_buffer.c"
gcov -b -o "..\build\CMakeFiles\test_memory_pool.dir\modules\memory_pool\memory_pool.c.obj" "..\modules\memory_pool\memory_pool.c"
gcov -b -o "..\build\CMakeFiles\test_linked_list.dir\modules\linked_list\linked_list.c.obj" "..\modules\linked_list\linked_list.c"
gcov -b -o "..\build\CMakeFiles\test_bitmap.dir\modules\bitmap\bitmap.c.obj" "..\modules\bitmap\bitmap.c"
gcov -b -o "..\build\CMakeFiles\test_atomic.dir\tests\test_atomic.c.obj" "..\modules\atomic\atomic.h"
gcov -b -o "..\build\CMakeFiles\test_crc.dir\modules\crc\crc.c.obj" "..\modules\crc\crc.c"
gcov -b -o "..\build\CMakeFiles\test_fsm.dir\modules\fsm\fsm.c.obj" "..\modules\fsm\fsm.c"

:: Profiler & Hashmap coverage
if exist "..\build\CMakeFiles\test_profiler.dir" (
    for /r "..\build\CMakeFiles\test_profiler.dir" %%O in (profiler.c.obj) do if exist "%%~fO" (
        gcov -b -o "%%~fO" "%SCRIPT_DIR%\..\profiler\profiler.c"
    )
    for /r "..\build\CMakeFiles\test_profiler.dir" %%O in (hashmap.c.obj) do if exist "%%~fO" (
        gcov -b -o "%%~fO" "%SCRIPT_DIR%\..\hashmap\hashmap.c"
    )
)
if exist "%SCRIPT_DIR%\modules\sertos\src\sertos_task.c" (
    set "SERTOS_SOURCE_DIR=%SCRIPT_DIR%\modules\sertos"
) else if exist "%SCRIPT_DIR%\..\sertos\src\sertos_task.c" (
    set "SERTOS_SOURCE_DIR=%SCRIPT_DIR%\..\sertos"
) else (
    echo [ERROR] SertOS source tree not found.
    goto :coverage_failed
)

call :gcov_sertos test_sertos_task sertos_task.c "%SERTOS_SOURCE_DIR%\src\sertos_task.c"
if errorlevel 1 goto :coverage_failed
call :gcov_sertos test_sertos_scheduler sertos_scheduler.c "%SERTOS_SOURCE_DIR%\src\sertos_scheduler.c"
if errorlevel 1 goto :coverage_failed
call :gcov_sertos test_sertos_sem sertos_sem.c "%SERTOS_SOURCE_DIR%\src\sertos_sem.c"
if errorlevel 1 goto :coverage_failed
call :gcov_sertos test_sertos_mutex sertos_mutex.c "%SERTOS_SOURCE_DIR%\src\sertos_mutex.c"
if errorlevel 1 goto :coverage_failed
call :gcov_sertos test_sertos_queue sertos_queue.c "%SERTOS_SOURCE_DIR%\src\sertos_queue.c"
if errorlevel 1 goto :coverage_failed
call :gcov_sertos test_sertos_stream_buffer sertos_stream_buffer.c "%SERTOS_SOURCE_DIR%\src\sertos_stream_buffer.c"
if errorlevel 1 goto :coverage_failed
call :gcov_sertos test_sertos_timer sertos_timer.c "%SERTOS_SOURCE_DIR%\src\sertos_timer.c"
if errorlevel 1 goto :coverage_failed

popd

echo.
echo ============================================================
echo [SUCCESS] Coverage analysis complete.
echo Reports generated in: coverage\
echo ============================================================
popd
endlocal
exit /b 0

:gcov_sertos
set "GCOV_OBJECT="
for /r "%SCRIPT_DIR%\build\CMakeFiles\%~1.dir" %%O in (%~2.obj) do if exist "%%~fO" set "GCOV_OBJECT=%%~fO"
if not defined GCOV_OBJECT (
    echo [ERROR] Coverage object not found for %~1.
    exit /b 1
)
gcov -b -o "%GCOV_OBJECT%" "%~3"
exit /b %ERRORLEVEL%

:coverage_failed
echo [ERROR] Coverage report generation failed.
popd
popd
exit /b 1
