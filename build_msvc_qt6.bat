@echo off
setlocal

:: ==========================================
:: Configuration
:: ==========================================
:: IMPORTANT: Change this to match your actual Qt 6 MSVC installation path!
set QT_PATH=C:\Qt\6.10.3\msvc2022_64
set SDL2_VERSION=2.32.10

echo ==========================================
echo Building AntiMicroX (WinServer2022 MSVC QT6 Debug)
echo ==========================================

:: 1. Check if we are in a developer command prompt
where cmake >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake is not in your PATH. 
    echo Please run this script from the "Developer Command Prompt for VS 2022".
    pause
    exit /b 1
)

:: 2. Download and Setup SDL2 (if not already done)
if not exist "build" mkdir build
if not exist "build\sdl2" (
    echo [INFO] SDL2 not found locally. Downloading SDL2 %SDL2_VERSION%...
    curl -L -o SDL2-devel-%SDL2_VERSION%-VC.zip https://github.com/libsdl-org/SDL/releases/download/release-%SDL2_VERSION%/SDL2-devel-%SDL2_VERSION%-VC.zip
    
    echo [INFO] Extracting SDL2...
    tar -xf SDL2-devel-%SDL2_VERSION%-VC.zip
    
    echo [INFO] Setting up SDL2 directories...
    move SDL2-%SDL2_VERSION% build\sdl2 >nul
    mklink /J "build\sdl2\SDL2" "build\sdl2\include"
    
    echo [INFO] Cleaning up zip file...
    del SDL2-devel-%SDL2_VERSION%-VC.zip
) else (
    echo [INFO] SDL2 folder already exists. Skipping download.
)

:: 3. Configure CMake
echo [INFO] Configuring CMake...
set CMAKE_PREFIX_PATH=%QT_PATH%\lib\cmake

cmake -DCMAKE_BUILD_TYPE=Debug -B "build" -G "Visual Studio 18 2026" "-DSDL2_PATH=%CD%\build\sdl2\\" "-DSDL2_LIBRARY=%CD%\build\sdl2\lib\x64\SDL2.lib" "-DSDL2_INCLUDE_DIR=%CD%\build\sdl2\\" "-DSDL2_DLL_LOCATION_DIR=%CD%\build\sdl2\lib\x64\SDL2.dll"

if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed! 
    echo Please make sure the QT_PATH variable at the top of this script is correct.
    pause
    exit /b %ERRORLEVEL%
)

:: 4. Build
echo [INFO] Building with parallel jobs...
cmake --build build --parallel 8

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed!
    pause
    exit /b %ERRORLEVEL%
)

echo [SUCCESS] Build completed successfully! 
echo If you want to run it from the folder, remember to use windeployqt to copy the Qt DLLs!
endlocal
pause
