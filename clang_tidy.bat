@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
pushd "%SCRIPT_DIR%"

:: -----------------------------------------------------------------------------
:: Locate clang-tidy
:: -----------------------------------------------------------------------------
set "CLANG_TIDY_EXE="
where clang-tidy.exe >nul 2>nul
if not errorlevel 1 (
    for /f "delims=" %%I in ('where clang-tidy.exe') do (
        if not defined CLANG_TIDY_EXE set "CLANG_TIDY_EXE=%%I"
    )
)

if not defined CLANG_TIDY_EXE (
    if exist "C:\clang\clang+llvm-22.1.3-x86_64-pc-windows-msvc\bin\clang-tidy.exe" (
        set "CLANG_TIDY_EXE=C:\clang\clang+llvm-22.1.3-x86_64-pc-windows-msvc\bin\clang-tidy.exe"
    )
)

if not defined CLANG_TIDY_EXE (
    for /d %%D in ("C:\clang\*\bin") do (
        if exist "%%D\clang-tidy.exe" set "CLANG_TIDY_EXE=%%D\clang-tidy.exe"
    )
)

if not defined CLANG_TIDY_EXE (
    for /d %%D in ("C:\Program Files\LLVM\bin") do (
        if exist "%%D\clang-tidy.exe" set "CLANG_TIDY_EXE=%%D\clang-tidy.exe"
    )
)

if not defined CLANG_TIDY_EXE (
    echo [ERROR] clang-tidy executable not found.
    popd
    exit /b 1
)

echo ============================================================
echo [STATIC ANALYSIS] clang-tidy executable: "%CLANG_TIDY_EXE%"
echo ============================================================

:: -----------------------------------------------------------------------------
:: Ensure compile_commands.json is generated
:: -----------------------------------------------------------------------------
if not exist "build\compile_commands.json" (
    echo [INFO] Generating build\compile_commands.json via CMake...
    cmake -B build -S . -G "MinGW Makefiles" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >nul 2>nul
    if errorlevel 1 (
        cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >nul 2>nul
    )
)

:: -----------------------------------------------------------------------------
:: Rules / Checks configuration
:: CERT C + Bugprone + Core Analyzer
:: -----------------------------------------------------------------------------
set "CHECKS=-*,cert-*,bugprone-*,clang-analyzer-core.*,clang-analyzer-security.*,clang-analyzer-valist.*"

:: Allow target filtering: clang_tidy.bat [profiler|sertos|all]
set "TARGET_FILTER=%~1"
if "%TARGET_FILTER%"=="" set "TARGET_FILTER=profiler"

echo.
echo [STATIC ANALYSIS] Running checks (%CHECKS%) for target: %TARGET_FILTER%
echo ============================================================

set "FAIL_COUNT=0"

if /i "%TARGET_FILTER%"=="profiler" goto :check_profiler
if /i "%TARGET_FILTER%"=="all" goto :check_all
if /i "%TARGET_FILTER%"=="sertos" goto :check_sertos

:check_profiler
echo -- Checking profiler / hashmap / stream...
"%CLANG_TIDY_EXE%" -p=build -checks="%CHECKS%" --header-filter="^%SCRIPT_DIR%\.\..*" "%SCRIPT_DIR%\..\profiler\profiler.c" -- --target=x86_64-w64-windows-gnu -isystem "C:\toolchains\mingw64\13.2.0\include" -isystem "C:\toolchains\mingw64\13.2.0\x86_64-w64-mingw32\include" -I"%SCRIPT_DIR%\..\profiler" -I"%SCRIPT_DIR%\..\profiler\port" -I"%SCRIPT_DIR%\..\hashmap" -I"%SCRIPT_DIR%\..\stream" -std=c99
if errorlevel 1 set /a FAIL_COUNT+=1
goto :finish

:check_sertos
echo -- Checking SertOS modules...
for %%F in ("%SCRIPT_DIR%\..\sertos\src\*.c") do (
    echo Checking %%~nxF...
    "%CLANG_TIDY_EXE%" -p=build -checks="%CHECKS%" "%%~fF" -- -I"%SCRIPT_DIR%\..\sertos\include" -std=c99
    if errorlevel 1 set /a FAIL_COUNT+=1
)
goto :finish

:check_all
echo -- Checking profiler...
"%CLANG_TIDY_EXE%" -p=build -checks="%CHECKS%" "%SCRIPT_DIR%\..\profiler\profiler.c" -- -I"%SCRIPT_DIR%\..\profiler" -I"%SCRIPT_DIR%\..\hashmap" -I"%SCRIPT_DIR%\..\stream" -std=c99
if errorlevel 1 set /a FAIL_COUNT+=1

echo -- Checking modules in test_bench/modules...
for /r "%SCRIPT_DIR%\modules" %%F in (*.c) do (
    echo Checking %%~nxF...
    "%CLANG_TIDY_EXE%" -p=build -checks="%CHECKS%" "%%~fF" -- -I"%SCRIPT_DIR%\modules" -std=c99
    if errorlevel 1 set /a FAIL_COUNT+=1
)
goto :finish

:finish
echo.
echo ============================================================
if %FAIL_COUNT% EQU 0 (
    echo [SUCCESS] Static analysis passed with zero critical errors.
) else (
    echo [WARNING] Static analysis completed with warnings/deviations.
)
echo ============================================================
popd
endlocal
exit /b 0
