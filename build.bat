@echo off
REM build.bat - Build voxel sandbox game
REM
REM Prerequisites:
REM   1. Set RAYLIB_DIR env var, e.g.:
REM        set RAYLIB_DIR=D:\raylib\raylib-5.5_win64_mingw-w64
REM   2. Or add raylib include dir to PATH so "where raylib.h" works
REM   3. Make sure g++ (MinGW-w64) is in PATH

setlocal
cd /d "%~dp0"

REM ---- Find raylib ----
set RAYLIB=

if not "%RAYLIB_DIR%"=="" (
    if exist "%RAYLIB_DIR%\include\raylib.h" (
        set RAYLIB=%RAYLIB_DIR%
        goto :compile
    )
)

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
    REM Fallback to common install location
    if exist "D:\raylib\raylib-5.5_win64_mingw-w64\include\raylib.h" (
        set RAYLIB=D:\raylib\raylib-5.5_win64_mingw-w64
    ) else (
        echo [ERROR] Cannot find raylib.h!
        echo Set RAYLIB_DIR, for example:
        echo     set RAYLIB_DIR=D:\raylib\raylib-5.5_win64_mingw-w64
        echo.
        pause
        exit /b 1
    )
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
