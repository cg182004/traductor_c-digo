@echo off
setlocal
cd /d "%~dp0"
where gcc >nul 2>nul
if errorlevel 1 (
    echo Instala MinGW-w64 y agrega la carpeta bin al PATH.
    pause
    exit /b 1
)
gcc -std=c11 -O2 -Wall -Wextra -municode -mwindows app.c traductor.c -o TraductorC_JS.exe -luser32 -lgdi32 -lm
if errorlevel 1 (
    echo La compilacion fallo. Este archivo requiere GCC de MinGW-w64 para Windows.
    pause
    exit /b 1
)
echo Listo: TraductorC_JS.exe. Abrelo con doble clic.
pause
