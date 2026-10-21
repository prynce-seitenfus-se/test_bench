@echo off
set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
pushd "%SCRIPT_DIR%"

set "CLEAN_BUILD=0"
for %%A in ("%~1" "%~2") do (
    if /i "%%~A"=="-h" goto :show_help
    if /i "%%~A"=="--help" goto :show_help
    if /i "%%~A"=="/?" goto :show_help
    if /i "%%~A"=="-c" set "CLEAN_BUILD=1"
    if /i "%%~A"=="--clean" set "CLEAN_BUILD=1"
)

if "%CLEAN_BUILD%"=="1" (
    echo [CLEAN] Removing previous CMake build outputs...
    if exist "build" (
        rmdir /s /q "build"
        if exist "build" (
            echo [ERROR] Could not remove the build directory.
            popd
            exit /b 1
        )
    )
)

:: Ensure CMake and MinGW are available in PATH
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

:: Ensure git submodules are checked out
if not exist "modules\Unity\src\unity.c" (
    echo [INFO] Submodules not initialized. Initializing git submodules...
    git submodule update --init --recursive
    if errorlevel 1 (
        echo [ERROR] Failed to initialize submodules.
        popd
        exit /b %errorlevel%
    )
)

echo ============================================================
echo [BUILD] Configuring CMake (MinGW Makefiles)...
echo ============================================================
cmake -B build -S . -G "MinGW Makefiles"
if errorlevel 1 (
    echo [ERROR] CMake configuration failed.
    popd
    exit /b %errorlevel%
)

echo.
echo ============================================================
echo [BUILD] Building test suite...
echo ============================================================
cmake --build build
if errorlevel 1 (
    echo [ERROR] Compilation failed.
    popd
    exit /b %errorlevel%
)

echo.
echo [SUCCESS] Build completed successfully.
popd
endlocal
exit /b 0

:show_help
echo ============================================================
echo [BUILD] Centralized Embedded Test Bench Builder
echo Generator: CMake + MinGW Makefiles (GCC 13.2.0)
echo ============================================================
echo.
echo Usage: build.bat [options]
echo.
echo Options:
echo   -c, --clean       Remove previous CMake build directory before rebuilding
echo   -h, --help, /?    Show this help message and exit
echo.
echo Typical workflows:
echo   build.bat           Incremental configure, build, and automated complexity check
echo   build.bat --clean   Clean rebuild from scratch
echo.
popd
exit /b 0
