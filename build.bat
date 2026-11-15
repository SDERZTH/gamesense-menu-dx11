@echo off
setlocal
cd /d "%~dp0"
if /i "%VSCMD_ARG_TGT_ARCH%"=="x64" goto compile
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto no_compiler
for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%I"
if not defined VSINSTALL goto no_compiler
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
:compile
if not exist build mkdir build
pushd build
cl /nologo /O2 /MT /EHsc /std:c++17 /utf-8 /W4 /D_CRT_SECURE_NO_WARNINGS /I..\third_party\imgui ^
  ..\main.cpp ..\third_party\imgui\imgui.cpp ..\third_party\imgui\imgui_draw.cpp ^
  ..\third_party\imgui\imgui_tables.cpp ..\third_party\imgui\imgui_widgets.cpp ^
  ..\third_party\imgui\backends\imgui_impl_win32.cpp ..\third_party\imgui\backends\imgui_impl_dx11.cpp ^
  /Fe:gsmenu-dx11.exe user32.lib gdi32.lib ole32.lib windowscodecs.lib d3d11.lib dxgi.lib d3dcompiler.lib dwmapi.lib ^
  /link /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup /INCREMENTAL:NO
set "BUILD_RESULT=%ERRORLEVEL%"
popd
if not "%BUILD_RESULT%"=="0" exit /b %BUILD_RESULT%
if /i "%~1"=="--no-copy" exit /b 0
copy /y "build\gsmenu-dx11.exe" "gsmenu.exe" >nul
if errorlevel 1 exit /b 1
echo Built %CD%\gsmenu.exe
exit /b 0
:no_compiler
echo Visual Studio with the Desktop development with C++ workload is required.
exit /b 1
