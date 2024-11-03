@echo off
if not exist build (
    cmake -S . -B build
    cmake --build build --config Debug
)

set RAYLIB=build\_deps\raylib-build\raylib\Debug\raylib.lib
set INCLUD=build\_deps\raylib-src\src
set SOURCE=src\*.c
set OUTPUT=build\src\Debug\raylib_game.exe
set WINLIB=user32.lib winmm.lib gdi32.lib shell32.lib

cl /MDd %RAYLIB% %WINLIB% %SOURCE% /I %INCLUD% /Fe:%OUTPUT%
if %errorlevel% neq 0 exit /b %errorlevel%

%OUTPUT%