@echo off
setlocal EnableExtensions EnableDelayedExpansion
set "VSCMD_START_DIR=%CD%"
set "VCVARS=%~1"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if not defined VCVARS (
    if not exist "!VSWHERE!" (
        echo Visual Studio vswhere.exe was not found
        exit /b 1
    )
    for /f "usebackq tokens=*" %%I in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%I"
    if not defined VSINSTALL (
        echo Visual Studio C++ build tools were not found
        exit /b 1
    )
    set "VCVARS=!VSINSTALL!\VC\Auxiliary\Build\vcvars64.bat"
)

if not exist "!VCVARS!" (
    echo Visual Studio environment script was not found: !VCVARS!
    exit /b 1
)

call "!VCVARS!"

if %errorlevel% neq 0 (
    echo vcvars64.bat failed
    exit /b %errorlevel%
)

rc.exe f1copy.rc

cl.exe /utf-8 /O2 /EHsc /W3 /D "UNICODE" /D "_UNICODE" main.cpp SplashWnd.cpp TrayIcon.cpp KeyHook.cpp TaskReg.cpp ScancodeMap.cpp f1copy.res user32.lib shell32.lib gdi32.lib advapi32.lib ole32.lib oleaut32.lib dwmapi.lib /Fe:f1copy.exe

if %errorlevel% neq 0 (
    echo Build failed!
    exit /b %errorlevel%
)
echo Build succeeded!
