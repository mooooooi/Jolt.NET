@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set ROOT_DIR=%SCRIPT_DIR%..
set JOLTC_DIR=%ROOT_DIR%\lib\joltc
set BIN_DIR=%ROOT_DIR%\bin

echo ==========================================
echo  Jolt.NET - Windows Build (x64 + ARM64)
echo ==========================================
echo Root:  %ROOT_DIR%
echo Joltc: %JOLTC_DIR%
echo.

cd /d "%JOLTC_DIR%"

REM --- win-x64 Distribution ---
echo [1/4] Configure win-x64 (Distribution)...
cmake -S "." -B "build_win_64" -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE:String=Distribution ^
    -DCMAKE_INSTALL_PREFIX:String="SDK" ^
    -DCROSS_PLATFORM_DETERMINISTIC=ON
if errorlevel 1 goto :error

echo [1/4] Build win-x64 (Distribution)...
cmake --build build_win_64 --config Distribution
if errorlevel 1 goto :error

REM --- win-arm64 Distribution ---
echo [2/4] Configure win-arm64 (Distribution)...
cmake -S "." -B "build_win_arm64" -G "Visual Studio 17 2022" -A ARM64 ^
    -DCMAKE_BUILD_TYPE:String=Distribution ^
    -DCMAKE_INSTALL_PREFIX:String="SDK" ^
    -DCROSS_PLATFORM_DETERMINISTIC=ON
if errorlevel 1 goto :error

echo [2/4] Build win-arm64 (Distribution)...
cmake --build build_win_arm64 --config Distribution
if errorlevel 1 goto :error

REM --- win-x64 Debug ---
echo [3/4] Configure win-x64 (Debug)...
cmake -S "." -B "build_win_64" -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE:String=Debug ^
    -DCMAKE_INSTALL_PREFIX:String="SDK" ^
    -DCROSS_PLATFORM_DETERMINISTIC=ON
if errorlevel 1 goto :error

echo [3/4] Build win-x64 (Debug)...
cmake --build build_win_64 --config Debug
if errorlevel 1 goto :error

REM --- win-arm64 Debug ---
echo [4/4] Configure win-arm64 (Debug)...
cmake -S "." -B "build_win_arm64" -G "Visual Studio 17 2022" -A ARM64 ^
    -DCMAKE_BUILD_TYPE:String=Debug ^
    -DCMAKE_INSTALL_PREFIX:String="SDK" ^
    -DCROSS_PLATFORM_DETERMINISTIC=ON
if errorlevel 1 goto :error

echo [4/4] Build win-arm64 (Debug)...
cmake --build build_win_arm64 --config Debug
if errorlevel 1 goto :error

REM --- Package ---
echo Packaging...
mkdir "%BIN_DIR%\win-x64" 2>nul
mkdir "%BIN_DIR%\win-arm64" 2>nul

copy /Y "build_win_64\bin\Distribution\joltc.dll"    "%BIN_DIR%\win-x64\joltc.dll"
copy /Y "build_win_64\bin\Debug\joltcd.dll"           "%BIN_DIR%\win-x64\joltcd.dll"
copy /Y "build_win_64\bin\Debug\joltcd.pdb"           "%BIN_DIR%\win-x64\joltcd.pdb"

copy /Y "build_win_arm64\bin\Distribution\joltc.dll"  "%BIN_DIR%\win-arm64\joltc.dll"
copy /Y "build_win_arm64\bin\Debug\joltcd.dll"        "%BIN_DIR%\win-arm64\joltcd.dll"
copy /Y "build_win_arm64\bin\Debug\joltcd.pdb"        "%BIN_DIR%\win-arm64\joltcd.pdb"

echo.
echo Done! Output in %BIN_DIR%
dir /s /b "%BIN_DIR%\win-*"
goto :eof

:error
echo.
echo BUILD FAILED!
exit /b 1
