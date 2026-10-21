@echo off
set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
pushd "%SCRIPT_DIR%"

:: -----------------------------------------------------------------------------
:: Locate Lizard executable
:: -----------------------------------------------------------------------------
set "LIZARD_EXE="
set "PYTHON_EXE="

where lizard.exe >nul 2>nul
if not errorlevel 1 (
    for /f "delims=" %%I in ('where lizard.exe') do (
        if not defined LIZARD_EXE set "LIZARD_EXE=%%I"
    )
)

if not defined LIZARD_EXE (
    if exist "%LOCALAPPDATA%\Python\pythoncore-3.14-64\Scripts\lizard.exe" (
        set "LIZARD_EXE=%LOCALAPPDATA%\Python\pythoncore-3.14-64\Scripts\lizard.exe"
    )
)

if not defined LIZARD_EXE (
    for /d %%D in ("%LOCALAPPDATA%\Programs\Python\Python*\Scripts") do (
        if exist "%%D\lizard.exe" (
            set "LIZARD_EXE=%%D\lizard.exe"
        )
    )
)

if not defined LIZARD_EXE (
    if exist "%APPDATA%\Python\Scripts\lizard.exe" (
        set "LIZARD_EXE=%APPDATA%\Python\Scripts\lizard.exe"
    )
)

if not defined LIZARD_EXE (
    where py.exe >nul 2>nul
    if not errorlevel 1 (
        for /f "delims=" %%I in ('where py.exe') do (
            if not defined PYTHON_EXE set "PYTHON_EXE=%%I"
        )
    )
)

if not defined LIZARD_EXE (
    if not defined PYTHON_EXE (
        where python.exe >nul 2>nul
        if not errorlevel 1 (
            for /f "delims=" %%I in ('where python.exe') do (
                if not defined PYTHON_EXE set "PYTHON_EXE=%%I"
            )
        )
    )
)

if not defined LIZARD_EXE (
    if not defined PYTHON_EXE (
        echo [ERROR] Lizard executable not found.
        echo Please install via: pip install lizard
        popd
        exit /b 1
    )
)

:: -----------------------------------------------------------------------------
:: Parse Arguments & Modes
:: -----------------------------------------------------------------------------
set "OUTPUT_ARGS="
set "EXTRA_ARGS="

if "%1"=="--html" (
    echo [INFO] Generating HTML complexity report: complexity_report.html
    set "OUTPUT_ARGS=--html -o complexity_report.html"
    shift
)

:: -----------------------------------------------------------------------------
:: Run Lizard Analysis
:: -----------------------------------------------------------------------------
echo ============================================================
echo [LIZARD] Analyzing Code Complexity ^& Maintainability
echo Thresholds: CCN ^<= 10 ^| NLOC ^<= 75 ^| Params ^<= 5
echo ============================================================

if defined LIZARD_EXE (
    "%LIZARD_EXE%" ^
        -C 10 ^
        -L 75 ^
        -a 5 ^
        -W whitelizard.txt ^
        -x "*/Unity/*" ^
        -x "*/build/*" ^
        -x "*/CMakeFiles/*" ^
        -x "*/coverage/*" ^
        %OUTPUT_ARGS% ^
        %* ^
        modules ^
        ..\sertos\src ^
        ..\sertos\port ^
        ..\profiler\profiler.c ^
        ..\profiler\profiler.h
) else (
    "%PYTHON_EXE%" -m lizard ^
        -C 10 ^
        -L 75 ^
        -a 5 ^
        -W whitelizard.txt ^
        -x "*/Unity/*" ^
        -x "*/build/*" ^
        -x "*/CMakeFiles/*" ^
        -x "*/coverage/*" ^
        %OUTPUT_ARGS% ^
        %* ^
        modules ^
        ..\sertos\src ^
        ..\sertos\port ^
        ..\profiler\profiler.c ^
        ..\profiler\profiler.h
)

set "EXIT_CODE=%errorlevel%"

echo.
if %EXIT_CODE% equ 0 (
    echo ============================================================
    echo [SUCCESS] Lizard quality gate passed: 0 threshold violations.
    echo ============================================================
) else (
    echo ============================================================
    echo [ERROR] Lizard quality gate failed with violations.
    echo ============================================================
)

popd
exit /b %EXIT_CODE%
