/*
 * SimplicCpuTemp - real CPU temperature for Rainmeter
 *
 * Copyright (C) 2026 Anu-Bhav
 * https://github.com/Anu-bhav/rainmeter-skins
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 * Reads Intel's on-die thermal sensor through PawnIO, a signed ring-0 IO
 * driver, using the official IntelMSR Pawn module. No Core Temp, no HWiNFO,
 * no external process and no polling script.
 *
 * How it works
 * ------------
 * PawnIO exposes a small API (PawnIOLib.dll) that loads a signed "module"
 * blob into the kernel driver and then executes named functions from it.
 * The IntelMSR module permits reading a fixed allowlist of MSRs, including:
 *
 *   MSR_IA32_TEMPERATURE_TARGET     0x1A2  -> TjMax in bits 23:16
 *   MSR_IA32_THERM_STATUS           0x19C  -> current temp in bits 22:16
 *   MSR_IA32_PACKAGE_THERM_STATUS   0x1B1  -> package temp
 *
 * Temperature is then (Intel SDM Vol. 4):
 *
 *   TjMax   = (MSR_TEMPERATURE_TARGET >> 16) & 0xFF
 *   T       = TjMax - ((MSR_THERM_STATUS >> 16) & 0x7F)
 *
 * Verified on the development machine (i9-13900KF, TjMax 100 C): reads
 * 35-47 C at idle and moves under load, unlike the ACPI thermal zone,
 * which is a hard-coded firmware constant.
 *
 * Requires elevation
 * ------------------
 * PawnIO's INF sets the device security descriptor to:
 *
 *   HKR,,Security,,"D:P(A;;GA;;;SY)(A;;GA;;;BA)"
 *
 * which grants access to SYSTEM and Builtin Administrators only. An
 * unelevated open fails with ERROR_ACCESS_DENIED, so Rainmeter itself must
 * run elevated for this measure to report. When it cannot get access the
 * measure returns -1.0 and the skin is expected to render that as "no
 * data" rather than showing a fabricated 0 C.
 *
 * SDK correctness notes
 * ---------------------
 *  - Exports are extern "C" so Rainmeter's GetProcAddress can resolve
 *    them unmangled:
 *        Initialize(void** data, void* rm)
 *        Reload    (void*  data, void* rm, double* maxValue)
 *        Update    (void*  data)            -> double
 *        GetString (void*  data)            -> LPCWSTR
 *        Finalize  (void*  data)            <-- void*, NOT void**
 *  - Options are read in Reload, not Initialize; Rainmeter calls
 *    Initialize before the option table exists.
 *  - GetString is called many times per cycle, so it stays trivial and
 *    returns NULL because this measure is numeric.
 *  - No CRT is linked: kernel32 only, HeapAlloc/HeapFree, and both
 *    PawnIOLib.dll and the embedded module are resolved lazily.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "IntelMSR_blob.h"

/* ------------------------------------------------------------------ */
/* Rainmeter plugin API (resolved at load time)                        */
/* ------------------------------------------------------------------ */

typedef LPCWSTR(__stdcall *RmReadString_t)(void *rm, LPCWSTR option, LPCWSTR defValue, BOOL replaceMeasures);
typedef double (__stdcall *RmReadFormula_t)(void *rm, LPCWSTR option, double defValue);

static RmReadString_t  pRmReadString  = NULL;
static RmReadFormula_t pRmReadFormula = NULL;

static int RmReadInt(void *rm, LPCWSTR option, int defValue)
{
    if (pRmReadFormula)
        return (int)pRmReadFormula(rm, option, (double)defValue);
    return defValue;
}

/* ------------------------------------------------------------------ */
/* PawnIOLib API (resolved from the installed DLL at runtime)          */
/* ------------------------------------------------------------------ */

typedef HRESULT (STDAPICALLTYPE *pawnio_open_t)(PHANDLE);
typedef HRESULT (STDAPICALLTYPE *pawnio_load_t)(HANDLE, const UCHAR *, SIZE_T);
typedef HRESULT (STDAPICALLTYPE *pawnio_execute_t)(HANDLE, PCSTR, const ULONG64 *, SIZE_T, PULONG64, SIZE_T, PSIZE_T);
typedef HRESULT (STDAPICALLTYPE *pawnio_close_t)(HANDLE);

static pawnio_open_t    pPawnOpen  = NULL;
static pawnio_load_t    pPawnLoad  = NULL;
static pawnio_execute_t pPawnExec  = NULL;
static pawnio_close_t   pPawnClose = NULL;

static HMODULE hPawnIOLib = NULL;
static int     gPawnState = 0; /* 0 untried, 1 ready, -1 failed */

static SRWLOCK gPawnLock = SRWLOCK_INIT;

/* Intel MSRs used by the IntelMSR module. */
#define MSR_IA32_TEMPERATURE_TARGET   0x000001A2
#define MSR_IA32_THERM_STATUS         0x0000019C

/*
 * Locates PawnIOLib.dll.
 *
 * The PawnIO wiki documents reading InstallLocation from
 *   HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\PawnIO
 * with %ProgramFiles%\PawnIO as the fallback, since the installer does not
 * allow changing the path. The DLL is also probed next to Rainmeter and on
 * the default search path.
 */
static HMODULE LoadPawnIOLib(void)
{
    HMODULE lib = NULL;
    HKEY    key;
    wchar_t path[MAX_PATH * 2];
    DWORD   len;
    LONG    rc;
    static const wchar_t *fallbacks[] = {
        L"C:\\Program Files\\PawnIO\\PawnIOLib.dll",
        L"C:\\Program Files (x86)\\PawnIO\\PawnIOLib.dll",
        NULL
    };
    int i;

    /* 1. Registry InstallLocation (documented method). */
    rc = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                       L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PawnIO",
                       0, KEY_READ, &key);
    if (rc == ERROR_SUCCESS) {
        DWORD type = 0;
        len = sizeof(path) - sizeof(wchar_t);   /* leave room for the NUL */
        rc = RegQueryValueExW(key, L"InstallLocation", NULL, &type, (LPBYTE)path, &len);
        RegCloseKey(key);

        if (rc == ERROR_SUCCESS && type == REG_SZ && len >= sizeof(wchar_t)) {
            DWORD chars = len / sizeof(wchar_t);
            if (chars > MAX_PATH * 2 - 1)
                chars = MAX_PATH * 2 - 1;
            path[chars] = 0;                    /* always terminate */

            /* Append "\PawnIOLib.dll" without CRT string helpers. */
            {
                static const wchar_t suffix[] = L"\\PawnIOLib.dll";
                DWORD used = chars;
                DWORD s;

                while (used > 0 && path[used - 1] == 0)
                    used--;
                path[used] = 0;

                for (s = 0; suffix[s] != 0 && (used + s) < (MAX_PATH * 2 - 1); s++)
                    path[used + s] = suffix[s];
                path[used + s] = 0;

                lib = LoadLibraryW(path);
                if (lib)
                    return lib;
            }
        }
    }

    /* 2. Known install locations. */
    for (i = 0; fallbacks[i]; i++) {
        lib = LoadLibraryW(fallbacks[i]);
        if (lib)
            return lib;
    }

    /* 3. Default search path. */
    return LoadLibraryW(L"PawnIOLib.dll");
}

static int EnsurePawn(void)
{
    int result = 0;

    if (gPawnState == 1)  return 1;
    if (gPawnState == -1) return 0;

    AcquireSRWLockExclusive(&gPawnLock);

    if (gPawnState == 1)  { result = 1; goto out; }
    if (gPawnState == -1) { result = 0; goto out; }

    hPawnIOLib = LoadPawnIOLib();
    if (!hPawnIOLib) {
        /* Not installed, or blocked. Try again next Reload. */
        gPawnState = -1;
        result = 0;
        goto out;
    }

#define RESOLVE(field, name) \
    field = (decltype(field))GetProcAddress(hPawnIOLib, name); \
    if (!field) { gPawnState = -1; result = 0; goto out; }

    RESOLVE(pPawnOpen,  "pawnio_open");
    RESOLVE(pPawnLoad,  "pawnio_load");
    RESOLVE(pPawnExec,  "pawnio_execute");
    RESOLVE(pPawnClose, "pawnio_close");

#undef RESOLVE

    gPawnState = 1;
    result = 1;

out:
    ReleaseSRWLockExclusive(&gPawnLock);
    return result;
}

/* ------------------------------------------------------------------ */
/* Measure state                                                       */
/* ------------------------------------------------------------------ */

#define MEASURE_MAGIC 0x53504355u /* "SPCU" */
#define TEMP_UNAVAILABLE (-1.0)

typedef struct
{
    unsigned int magic;

    int    index;      /* reserved for a future multi-zone option */
    double lastTemp;   /* degrees C, or TEMP_UNAVAILABLE */

    HANDLE pawn;       /* PawnIO executor handle */
    int    havePawn;
} Measure;

static void *MeasureAlloc(size_t bytes)
{
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes);
}

static void MeasureFree(void *block)
{
    if (block)
        HeapFree(GetProcessHeap(), 0, block);
}

/* ------------------------------------------------------------------ */
/* Reading                                                             */
/* ------------------------------------------------------------------ */

/* Execute ioctl_read_msr from the IntelMSR module. */
static int ReadMsr(HANDLE pawn, unsigned long msr, unsigned long long *out)
{
    ULONG64 in[1];
    ULONG64 res[1];
    SIZE_T  rsize = 0;
    HRESULT hr;

    in[0] = (ULONG64)msr;

    hr = pPawnExec(pawn, "ioctl_read_msr", in, 1, res, 1, &rsize);
    if (FAILED(hr) || rsize < 1)
        return 0;

    *out = (unsigned long long)res[0];
    return 1;
}

/*
 * Opens the PawnIO executor and loads the embedded IntelMSR module.
 * Returns 1 when ready to read, 0 otherwise.
 */
static int EnsurePawnOpen(Measure *m)
{
    HRESULT hr;

    if (m->havePawn)
        return 1;

    if (!EnsurePawn())
        return 0;

    hr = pPawnOpen(&m->pawn);
    if (FAILED(hr) || !m->pawn) {
        /* Most likely ERROR_ACCESS_DENIED: Rainmeter is not elevated. */
        m->pawn = NULL;
        return 0;
    }

    hr = pPawnLoad(m->pawn, gIntelMsrBlob, (SIZE_T)gIntelMsrBlobSize);
    if (FAILED(hr)) {
        pPawnClose(m->pawn);
        m->pawn = NULL;
        return 0;
    }

    m->havePawn = 1;
    return 1;
}

static void ClosePawn(Measure *m)
{
    if (m->havePawn && m->pawn && pPawnClose) {
        pPawnClose(m->pawn);
        m->pawn = NULL;
        m->havePawn = 0;
    }
}

/*
 * Returns CPU temperature in degrees C, or TEMP_UNAVAILABLE when PawnIO
 * cannot be reached or the MSR read fails.
 */
static double ReadCpuTemp(Measure *m)
{
    unsigned long long ttarget = 0;
    unsigned long long therm   = 0;
    int tjmax, deg;

    if (!EnsurePawnOpen(m))
        return TEMP_UNAVAILABLE;

    if (!ReadMsr(m->pawn, MSR_IA32_TEMPERATURE_TARGET, &ttarget))
        return TEMP_UNAVAILABLE;

    if (!ReadMsr(m->pawn, MSR_IA32_THERM_STATUS, &therm))
        return TEMP_UNAVAILABLE;

    tjmax = (int)((ttarget >> 16) & 0xFF);
    if (tjmax <= 0)
        return TEMP_UNAVAILABLE;

    deg = tjmax - (int)((therm >> 16) & 0x7F);
    if (deg < 0)
        deg = 0;

    return (double)deg;
}

/* ------------------------------------------------------------------ */
/* Rainmeter exports                                                   */
/* ------------------------------------------------------------------ */

#define PLUGIN_EXPORT extern "C" __declspec(dllexport)

PLUGIN_EXPORT void Initialize(void **data, void *rm)
{
    Measure *m;

    (void)rm;
    if (!data)
        return;

    *data = NULL;

    m = (Measure *)MeasureAlloc(sizeof(Measure));
    if (!m)
        return;

    m->magic    = MEASURE_MAGIC;
    m->lastTemp = TEMP_UNAVAILABLE;

    *data = m;
}

PLUGIN_EXPORT void Reload(void *data, void *rm, double *maxValue)
{
    Measure *m = (Measure *)data;

    if (!m)
        return;

    m->index = RmReadInt(rm, L"Index", 0);
    if (m->index < 0)
        m->index = 0;

    /* Options changed: drop the PawnIO handle so it is rebuilt lazily. */
    ClosePawn(m);

    if (maxValue)
        *maxValue = 100.0;   /* graph scale: 0-100 C */
}

PLUGIN_EXPORT double Update(void *data)
{
    Measure *m = (Measure *)data;
    double t;

    if (!m)
        return TEMP_UNAVAILABLE;

    t = ReadCpuTemp(m);
    if (t < 0.0) {
        /* Keep reporting unavailable rather than a fabricated 0 C. */
        m->lastTemp = TEMP_UNAVAILABLE;
        return TEMP_UNAVAILABLE;
    }

    m->lastTemp = t;
    return t;
}

PLUGIN_EXPORT LPCWSTR GetString(void *data)
{
    (void)data;
    return NULL;   /* numeric metric */
}

PLUGIN_EXPORT void Finalize(void *data)
{
    Measure *m = (Measure *)data;

    if (!m)
        return;

    if (m->magic == MEASURE_MAGIC) {
        ClosePawn(m);
        m->magic = 0;
        MeasureFree(m);
    }
}

/* ------------------------------------------------------------------ */
/* DLL entry point                                                     */
/* ------------------------------------------------------------------ */

extern "C" BOOL WINAPI DllMainCRTStartup(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    (void)hinstDLL;
    (void)lpvReserved;

    if (fdwReason == DLL_PROCESS_ATTACH) {
        HMODULE hRm = GetModuleHandleW(L"Rainmeter.dll");
        if (hRm) {
            pRmReadString  = (RmReadString_t)GetProcAddress(hRm, "RmReadString");
            pRmReadFormula = (RmReadFormula_t)GetProcAddress(hRm, "RmReadFormula");
        }

        gPawnState = 0;
        hPawnIOLib = NULL;
    }
    else if (fdwReason == DLL_PROCESS_DETACH) {
        /* Do NOT FreeLibrary(PawnIOLib) or close PawnIO handles here:
         * teardown can run under the loader lock while a measure is still
         * inside a PawnIO call. The OS reclaims everything at exit. */
        gPawnState = 0;
        hPawnIOLib = NULL;
    }

    return TRUE;
}