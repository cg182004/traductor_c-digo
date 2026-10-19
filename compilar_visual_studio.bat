@echo off
setlocal
cd /d "%~dp0"
where cl >nul 2>nul
if errorlevel 1 (
    echo Abre x64 Native Tools Command Prompt for VS 2022 y ejecuta este archivo.
    echo En Visual Studio instala Desarrollo para el escritorio con C++.
    pause
    exit /b 1
)
cl /nologo /std:c11 /utf-8 /W4 /O2 /MT /D_CRT_SECURE_NO_WARNINGS app.c traductor.c /Fe:TraductorC_JS.exe /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib
if errorlevel 1 (
    echo La compilacion fallo. Revisa los mensajes anteriores.
    pause
    exit /b 1
)
echo Listo: TraductorC_JS.exe. Abrelo con doble clic.
pause
