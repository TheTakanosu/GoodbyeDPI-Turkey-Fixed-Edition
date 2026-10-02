@echo off
rem Kara liste testlerini derler ve calistirir. Gerekenler: Visual Studio 2026 (C++), Qt 6 msvc 64-bit.
setlocal
if "%QTDIR%"=="" set QTDIR=C:\Qt\6.11.0\msvc2022_64
set VSDEVCMD=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat
call "%VSDEVCMD%" >nul || exit /b 1
set OUT=%TEMP%\takanosu_test
if not exist "%OUT%" mkdir "%OUT%"
cl /nologo /EHsc /std:c++17 /utf-8 /Zc:__cplusplus /permissive- /MD /I"%QTDIR%\include" /I"%QTDIR%\include\QtCore" "%~dp0kara_liste_test.cpp" /Fo"%OUT%\\" /Fe"%OUT%\kara_liste_test.exe" /link "%QTDIR%\lib\Qt6Core.lib" >"%OUT%\derleme.log" || (type "%OUT%\derleme.log" & exit /b 1)
set PATH=%QTDIR%\bin;%PATH%
"%OUT%\kara_liste_test.exe"
