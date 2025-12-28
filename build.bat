@echo off
setlocal

REM Define paths
set "SCRIPT_DIR=%~dp0"
set "SOURCE_DIR=%SCRIPT_DIR%LS_Reshade"
set "BUILD_DIR=%SOURCE_DIR%\build"

echo Source Dir: %SOURCE_DIR%
echo Build Dir:  %BUILD_DIR%

REM Check if source exists
if not exist "%SOURCE_DIR%\CMakeLists.txt" (
    echo Error: CMakeLists.txt not found in %SOURCE_DIR%
    pause
    exit /b 1
)

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM Navigate to build directory
pushd "%BUILD_DIR%"

REM Configure CMake
echo Configuring CMake...
cmake .. -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed.
    popd
    pause
    exit /b 1
)

REM Build
echo Building...
cmake --build . --config Release
if %ERRORLEVEL% NEQ 0 (
    echo Build failed.
    popd
    pause
    exit /b 1
)

popd
echo Done.
pause
