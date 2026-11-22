@echo off
REM ---------------------------------------------------------------------------
REM Builds SimplicCpuTemp.dll with MinGW-w64 (no CRT dependency).
REM
REM Copyright (C) 2026 Anu-Bhav
REM https://github.com/Anu-bhav/rainmeter-skins
REM SPDX-License-Identifier: GPL-2.0
REM
REM Requirements:
REM   - MinGW-w64 (this was built with the toolchain bundled with Strawberry
REM     Perl: C:\Strawberry\c\bin\g++.exe)
REM
REM The IntelMSR Pawn module blob is embedded by IntelMSR_blob.h, which is
REM generated from the official PawnIO.Modules release:
REM   https://github.com/namazso/PawnIO.Modules/releases/tag/0.2.12
REM To regenerate it after a module update, run gen_blob.ps1 against the
REM new IntelMSR.bin.
REM
REM The DLL imports KERNEL32 and USER32 only. PawnIOLib.dll is resolved at
REM runtime from the registry InstallLocation, so there is no import library
REM and nothing extra to ship.
REM
REM Output: SimplicCpuTemp.dll (copy it to %%APPDATA%%\Rainmeter\Plugins\)
REM ---------------------------------------------------------------------------
setlocal
set DIR=%~dp0
set GXX=C:\Strawberry\c\bin\g++.exe
set WINDRES=C:\Strawberry\c\bin\windres.exe
set OUT=%DIR%SimplicCpuTemp.dll

cd /d "%DIR%"
del /q "%OUT%" 2>nul

REM Compile the version resource (optional but gives the DLL a version tab)
if exist "%WINDRES%" (
    "%WINDRES%" -O coff "%DIR%SimplicCpuTemp.rc" "%DIR%SimplicCpuTemp.res"
)

"%GXX%" -O2 -shared -nostdlib -o "%OUT%" "%DIR%SimplicCpuTemp.cpp" "%DIR%SimplicCpuTemp.res" -lkernel32 -ladvapi32 -Wl,--enable-stdcall-fixup

echo EXITCODE=%ERRORLEVEL%
dir "%OUT%" 2>nul