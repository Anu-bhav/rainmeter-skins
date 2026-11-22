@echo off
REM ---------------------------------------------------------------------------
REM Builds SimplicGpu.dll with MinGW-w64 (no CRT dependency).
REM
REM Copyright (C) 2026 Anu-Bhav
REM https://github.com/Anu-bhav/rainmeter-skins
REM SPDX-License-Identifier: GPL-2.0
REM
REM Requirements:
REM   - MinGW-w64 (this was built with the toolchain bundled with Strawberry
REM     Perl: C:\Strawberry\c\bin\g++.exe)
REM   - nvml.dll, which ships with every NVIDIA driver at
REM     C:\Windows\System32\nvml.dll
REM
REM The DLL imports only KERNEL32 and nvml, and exports the five Rainmeter
REM plugin entry points. No libstdc++/libgcc is linked, so the file stays tiny.
REM
REM Output: SimplicGpu.dll (copy it to %APPDATA%\Rainmeter\Plugins\)
REM ---------------------------------------------------------------------------
setlocal
set DIR=%~dp0
set GXX=C:\Strawberry\c\bin\g++.exe
set WINDRES=C:\Strawberry\c\bin\windres.exe
set NVML=C:\Windows\System32\nvml.dll
set OUT=%DIR%SimplicGpu.dll

cd /d "%DIR%"
del /q "%OUT%" 2>nul

REM Compile the version resource (optional but gives the DLL a version tab)
if exist "%WINDRES%" (
    "%WINDRES%" -O coff "%DIR%SimplicGpu.rc" "%DIR%SimplicGpu.res"
)

"%GXX%" -O2 -shared -nostdlib -o "%OUT%" "%DIR%SimplicGpu.cpp" "%DIR%SimplicGpu.res" "%NVML%" -lkernel32 -Wl,--enable-stdcall-fixup

echo EXITCODE=%ERRORLEVEL%
dir "%OUT%" 2>nul