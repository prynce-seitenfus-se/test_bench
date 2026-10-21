@echo off
set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
pushd "%SCRIPT_DIR%"

:: -----------------------------------------------------------------------------
:: Check for help request
:: -----------------------------------------------------------------------------
if "%~1"=="-h" goto :show_help
if "%~1"=="--help" goto :show_help
if "%~1"=="/?" goto :show_help

:: Ensure CMake/CTest and MinGW are available in PATH
where ctest.exe >nul 2>nul
if errorlevel 1 (
    set "PATH=C:\Program Files\CMake\bin;%PATH%"
)
set "MINGW_BIN=C:\toolchains\mingw64\13.2.0\bin"
if exist "%MINGW_BIN%\gcc.exe" (
    set "PATH=%MINGW_BIN%;%PATH%"
)

:: Build automatically if binary does not exist
if not exist "build\test_ring_buffer.exe" (
    echo [INFO] Test binary not found. Building first...
    call "build.bat"
    if errorlevel 1 (
        popd
        exit /b %errorlevel%
    )
)

:: Filter argument (e.g. run.bat profiler)
set "TEST_FILTER=%~1"

if not "%TEST_FILTER%"=="" (
    echo ============================================================
    echo [RUN] Running filtered test: %TEST_FILTER% via CTest...
    echo ============================================================
    ctest --test-dir build -R "%TEST_FILTER%" --output-on-failure
    if errorlevel 1 (
        ctest --test-dir build -R "Validation_%TEST_FILTER%" --output-on-failure
    )
    echo.
    echo ============================================================
    echo [RUN] Running %TEST_FILTER% binary directly...
    echo ============================================================
    if exist "build\test_%TEST_FILTER%.exe" (
        build\test_%TEST_FILTER%.exe
    ) else if exist "build\%TEST_FILTER%.exe" (
        build\%TEST_FILTER%.exe
    )
    popd
    exit /b %errorlevel%
)

echo ============================================================
echo [RUN] Running tests via CTest...
echo ============================================================
ctest --test-dir build --output-on-failure
if errorlevel 1 (
    echo [ERROR] CTest reported test failures.
    popd
    exit /b %errorlevel%
)

echo.
echo ============================================================
echo [RUN] Detailed Unity Test Output:
echo ============================================================
echo.
echo [1/14] Running Ring Buffer tests...
build\test_ring_buffer.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )

echo.
echo [2/14] Running Atomic tests...
build\test_atomic.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )

echo.
echo [3/14] Running Memory Pool tests...
build\test_memory_pool.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )

echo.
echo [4/14] Running Linked List tests...
build\test_linked_list.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )

echo.
echo [5/14] Running Bitmap tests...
build\test_bitmap.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )

echo.
echo [6/14] Running CRC tests...
build\test_crc.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )

echo.
echo [7/14] Running FSM tests...
build\test_fsm.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )

echo.
echo [8/14] Running SertOS Task tests...
build\test_sertos_task.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )

echo.
echo [9/14] Running SertOS Scheduler tests...
build\test_sertos_scheduler.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )

echo.
echo [10/14] Running SertOS Semaphore tests...
build\test_sertos_sem.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )

echo.
echo [11/14] Running SertOS Mutex tests...
build\test_sertos_mutex.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )

echo.
echo [12/14] Running SertOS Queue tests...
build\test_sertos_queue.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )

echo.
echo [13/14] Running SertOS Timer tests...
build\test_sertos_timer.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )

echo.
echo [14/15] Running SertOS Benchmark tests...
build\test_sertos_benchmark.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )

echo.
echo [15/15] Running Profiler tests...
build\test_profiler.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )

echo.
echo [16/17] Running SertOS Runtime Statistics tests...
build\test_sertos_stats.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )

echo.
echo [17/17] Running SertOS Stream Buffer tests...
build\test_sertos_stream_buffer.exe
if errorlevel 1 ( popd & exit /b %errorlevel% )

echo.
echo ============================================================
echo [SUCCESS] All unit test suites passed successfully.
echo ============================================================
popd
endlocal
exit /b 0

:show_help
echo ============================================================
echo [RUN] Centralized Embedded Test Suite Runner
echo Harness: CTest + Unity Test Framework
echo ============================================================
echo.
echo Usage: run.bat [filter^|options]
echo.
echo Options:
echo   -h, --help, /?        Show this help message and exit
echo   [filter]              Case-insensitive regex/name to run a single test suite
echo.
echo Examples:
echo   run.bat                 Run all 29 unit test suites across all submodules
echo   run.bat profiler        Run only profiler tests (test_profiler.exe)
echo   run.bat scheduler       Run only scheduler tests
echo   run.bat mutex           Run only mutex tests
echo.
popd
exit /b 0
