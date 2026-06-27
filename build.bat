@echo off
setlocal

REM ---- Configure your environment ----
set "VCPKG_ROOT=C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\vcpkg"
set "PATH=C:\Program Files\LLVM\bin;%PATH%"
set "PATH=C:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"

REM ---- Move to the project root ----
cd /d "%~dp0"

REM ---- Configure if build directory doesn't exist ----
if not exist "build\win-x64-clang-rwdi" (
    echo Configuring project...
    cmake --preset win-x64-clang-rwdi
    if errorlevel 1 goto :error
)

REM ---- Build ----
echo Building...
cmake --build build\win-x64-clang-rwdi --parallel
if errorlevel 1 goto :error

echo.
echo ===========================
echo Build completed successfully.
echo ===========================
exit /b 0

:error
echo.
echo ===========================
echo Build failed.
echo ===========================
pause
exit /b 1