@echo off
title Compilar Lux

echo ========================================================
echo               LUX - AUTO COMPILADOR
echo ========================================================
echo.

cd /d "%~dp0"

:: Si el compilador ya esta en el PATH (ej. GitHub Actions o Developer Command Prompt), compilar directo
where cl.exe >nul 2>&1
if %errorlevel% equ 0 (
    echo Entorno MSVC detectado en PATH.
    goto :compile
)

set "VCVARS="
if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"

if not defined VCVARS goto :no_tools

echo [1/2] Configurando entorno MSVC x64...
call "%VCVARS%" >nul 2>&1

:compile
if not exist "x64\Release\obj" mkdir "x64\Release\obj"

echo [2/2] Compilando recursos y codigo fuente de Lux...
rc.exe /fo "x64\Release\obj\Lux.res" "Lux\Lux.rc"
if errorlevel 1 goto :build_error

cl.exe /nologo /O2 /std:c++17 /EHsc /MT /utf-8 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /I "Lux" /Fo"x64\Release\obj\\" "Lux\*.cpp" /link "x64\Release\obj\Lux.res" /OUT:"x64\Release\Lux.exe" /SUBSYSTEM:WINDOWS /INCREMENTAL:NO /OPT:REF /OPT:ICF user32.lib gdi32.lib shell32.lib advapi32.lib dwmapi.lib dxva2.lib ole32.lib oleaut32.lib wbemuuid.lib comsuppw.lib msimg32.lib uxtheme.lib gdiplus.lib comctl32.lib wininet.lib urlmon.lib
if errorlevel 1 goto :build_error

echo.
echo ========================================================
echo         COMPILACION COMPLETADA CON EXITO!
echo ========================================================
echo Ejecutable generado: x64\Release\Lux.exe
echo.

if /i "%~1"=="--no-run" exit /b 0

set "RUN_APP="
set /p RUN_APP="Deseas iniciar Lux ahora? (S/N) [Por defecto S]: "
if "%RUN_APP%"=="" set RUN_APP=S
if /i "%RUN_APP%"=="S" (
    echo Iniciando Lux...
    start "" "x64\Release\Lux.exe"
)
exit /b 0

:no_tools
echo [ERROR] No se encontro instalacion de Visual Studio 2022 (vcvars64.bat).
pause
exit /b 1

:build_error
echo.
echo [ERROR] La compilacion ha fallado. Revisa los errores anteriores.
echo.
pause
exit /b 1
