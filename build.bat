@echo off
REM build.bat - Build voxel sandbox game
REM
REM Prerequisites:
REM   1. raylib 5.5 is bundled in the raylib\ folder next to this script
REM      (no download, no environment variable needed)
REM   2. Make sure g++ (MinGW-w64) is in PATH

setlocal
cd /d "%~dp0"

REM ---- Find raylib: bundled copy in project first ----
set RAYLIB=
if exist "%~dp0raylib\include\raylib.h" (
    set RAYLIB=%~dp0raylib
    goto :compile
)

REM ---- Fallback: RAYLIB_DIR env var ----
if not "%RAYLIB_DIR%"=="" (
    if exist "%RAYLIB_DIR%\include\raylib.h" (
        set RAYLIB=%RAYLIB_DIR%
        goto :compile
    )
)

REM ---- Fallback: raylib.h on PATH ----
for /f "delims=" %%i in ('where raylib.h 2^>nul') do (
    set RAYLIB=%%~dpi
    goto :found
)
:found
if not "%RAYLIB%"=="" (
    REM RAYLIB looks like D:\...raylib-5.5_win64_mingw-w64\include\
    REM Remove trailing include\ to get base dir
    set RAYLIB=%RAYLIB:~0,-8%
)

:compile
if "%RAYLIB%"=="" (
    echo [ERROR] Cannot find raylib.h!
    echo Expected it in the raylib\ folder next to build.bat,
    echo or set the RAYLIB_DIR environment variable.
    echo.
    pause
    exit /b 1
)

echo [INFO] Using raylib: %RAYLIB%

echo [1/2] Compiling src\*.cpp ...
g++ -std=c++17 -O2 -Wall -Wno-unused-parameter -I"%RAYLIB%\include" src\*.cpp -o game.exe -L"%RAYLIB%\lib" -lraylib -lopengl32 -lgdi32 -lwinmm

if errorlevel 1 (
    echo.
    echo Build FAILED.
    pause
    exit /b 1
)

echo.
echo [2/2] Build OK. game.exe ready.
endlocal
pause
