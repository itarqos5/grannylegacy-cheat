@echo off
setlocal enabledelayedexpansion

set CONFIG=Release
if not "%~1"=="" set CONFIG=%~1

set "VCVARS="

for %%P in (
    "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) do (
    if exist %%~P (
        set "VCVARS=%%~P"
        goto :vcvars_found
    )
)

if "%VCVARS%"=="" (
    echo ERROR: Could not find x64 Native Tools Command Prompt for VS 2022 vcvars64.bat.
    exit /b 1
)

:vcvars_found

call "%VCVARS%"
if errorlevel 1 exit /b 1

cd /d "%~dp0"
if not exist build mkdir build

cmake -S . -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b 1

cmake --build build --config %CONFIG% --target package_dist
if errorlevel 1 exit /b 1

echo Build complete.
echo Output: dist\hid.dll
echo Output: dist\inject.exe
