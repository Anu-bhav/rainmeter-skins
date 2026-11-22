@echo off
REM ---------------------------------------------------------------------------
REM Builds SimplicCpuTemp.dll with MinGW-w64 (no CRT dependency).
REM
REM Requirements:
REM   - MinGW-w64 (this was built with the toolchain bundled with Strawberry
REM     Perl: C:\Strawberry\c\bin\g++.exe)
REM
REM The DLL imports only KERNEL32 and USER32 (wsprintfW is used by the
REM temporary debug log) and exports the five Rainmeter plugin entry
REM points. No libstdc++/libgcc is linked.
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

"%GXX%" -O2 -shared -nostdlib -o "%OUT%" "%DIR%SimplicCpuTemp.cpp" "%DIR%SimplicCpuTemp.res" -lkernel32 -luser32 -Wl,--enable-stdcall-fixup

echo EXITCODE=%ERRORLEVEL%
dir "%OUT%" 2>nul