/*
 * SimplicCpuTemp - CPU/thermal temperature measure for Rainmeter
 *
 * Reads the ACPI thermal zone through the Windows Performance Counter
 * API ("\Thermal Zone Information(*)\Temperature"). No external app,
 * no elevation, no kernel driver, no child process.
 *
 * Options
 *   Index   Which thermal zone instance to read (default 0)
 *
 * Layout note (this is the part that cost the most time)
 * -----------------------------------------------------
 * PdhGetFormattedCounterArrayW with PDH_FMT_DOUBLE returns, per item:
 *
 *   offset  0 : LPWSTR szName
 *   offset  8 : DWORD  CStatus          (0 == PDH_CSTATUS_VALID_DATA)
 *   offset 12 : DWORD  Type
 *   offset 16 : LONG   value            <-- temperature in TENTHS of a
 *                                            degree C (301 == 30.1 C)
 *   ...       : the item name copied inline as UTF-16
 *
 * The total buffer for one item on this machine is 44 bytes, i.e. the
 * stride is NOT the 24 bytes the header implies. Deriving the stride
 * from bufSize / itemCount avoids hard-coding it, and the temperature
 * is read as a 32-bit LONG at offset 16 - reading an 8-byte double
 * there yields a denormal and made the value look like 0.
 *
 * Rainmeter SDK notes
 * -------------------
 *  - Exports are extern "C" so GetProcAddress finds them unmangled:
 *        Initialize(void** data, void* rm)
 *        Reload    (void*  data, void* rm, double* maxValue)
 *        Update    (void*  data)            -> double
 *        GetString (void*  data)            -> LPCWSTR
 *        Finalize  (void*  data)            <-- void*, NOT void**
 *  - Options are read in Reload, not Initialize; the option table does
 *    not exist yet when Initialize runs.
 *  - GetString must stay trivial: it is called many times per cycle.
 *    This measure is numeric, so it returns NULL.
 *  - No CRT is linked. kernel32/user32 only, HeapAlloc/HeapFree, and
 *    pdh.dll is resolved at runtime so no import library is needed.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* ------------------------------------------------------------------ */
/* Rainmeter plugin API (resolved at load time)                        */
/* ------------------------------------------------------------------ */

typedef LPCWSTR(__stdcall *RmReadString_t)(void *rm, LPCWSTR option, LPCWSTR defValue, BOOL replaceMeasures);
typedef double (__stdcall *RmReadFormula_t)(void *rm, LPCWSTR option, double defValue);

static RmReadString_t  pRmReadString  = NULL;
static RmReadFormula_t pRmReadFormula = NULL;

static LPCWSTR RmReadString(void *rm, LPCWSTR option, LPCWSTR defValue)
{
    if (pRmReadString)
        return pRmReadString(rm, option, defValue, FALSE);
    return defValue;
}

static int RmReadInt(void *rm, LPCWSTR option, int defValue)
{
    if (pRmReadFormula)
        return (int)pRmReadFormula(rm, option, (double)defValue);
    return defValue;
}

/* ------------------------------------------------------------------ */
/* PDH (pdh.dll resolved at runtime)                                   */
/* ------------------------------------------------------------------ */

typedef LONG   PDH_STATUS;
typedef HANDLE PDH_HQUERY;
typedef HANDLE PDH_HCOUNTER;

typedef PDH_STATUS (__stdcall *PdhOpenQuery_t)(LPCWSTR, DWORD_PTR, PDH_HQUERY *);
typedef PDH_STATUS (__stdcall *PdhAddCounter_t)(PDH_HQUERY, LPCWSTR, DWORD_PTR, PDH_HCOUNTER *);
typedef PDH_STATUS (__stdcall *PdhCollectQueryData_t)(PDH_HQUERY);
typedef PDH_STATUS (__stdcall *PdhCloseQuery_t)(PDH_HQUERY);
typedef PDH_STATUS (__stdcall *PdhGetFormattedCounterArray_t)(PDH_HCOUNTER, DWORD, DWORD *, DWORD *, void *);

static PdhOpenQuery_t                pPdhOpenQuery    = NULL;
static PdhAddCounter_t               pPdhAddCounter   = NULL;
static PdhCollectQueryData_t         pPdhCollect      = NULL;
static PdhCloseQuery_t               pPdhCloseQuery   = NULL;
static PdhGetFormattedCounterArray_t pPdhGetFormatted = NULL;

static HMODULE hPdh      = NULL;
static int     gPdhState = 0; /* 0 untried, 1 ready, -1 failed */

static SRWLOCK gPdhLock = SRWLOCK_INIT;

#define PDH_FMT_DOUBLE 0x00000100
#define PDH_MORE_DATA  0x800007D2L

/* Item layout offsets, see the layout note above. */
#define OFF_CSTATUS 8
#define OFF_TYPE    12
#define OFF_LONGVAL 16

static int EnsurePdh(void)
{
    int result = 0;

    if (gPdhState == 1)  return 1;
    if (gPdhState == -1) return 0;

    AcquireSRWLockExclusive(&gPdhLock);

    if (gPdhState == 1)  { result = 1; goto out; }
    if (gPdhState == -1) { result = 0; goto out; }

    hPdh = LoadLibraryW(L"pdh.dll");
    if (!hPdh) { gPdhState = -1; result = 0; goto out; }

#define RESOLVE(field, name) \
    field = (decltype(field))GetProcAddress(hPdh, name); \
    if (!field) { gPdhState = -1; result = 0; goto out; }

    RESOLVE(pPdhOpenQuery,    "PdhOpenQueryW");
    RESOLVE(pPdhAddCounter,   "PdhAddCounterW");
    RESOLVE(pPdhCollect,      "PdhCollectQueryData");
    RESOLVE(pPdhCloseQuery,   "PdhCloseQuery");
    RESOLVE(pPdhGetFormatted, "PdhGetFormattedCounterArrayW");

#undef RESOLVE

    gPdhState = 1;
    result = 1;

out:
    ReleaseSRWLockExclusive(&gPdhLock);
    return result;
}

/* ------------------------------------------------------------------ */
/* Measure state                                                       */
/* ------------------------------------------------------------------ */

#define MEASURE_MAGIC 0x53504355u /* "SPCU" */

typedef struct
{
    unsigned int magic;

    int          index;
    double       lastTemp;   /* degrees C, cached */

    PDH_HQUERY   query;
    PDH_HCOUNTER counter;
    int          haveQuery;
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
/* Thermal zone read                                                   */
/* ------------------------------------------------------------------ */

/*
 * Returns temperature in tenths of a degree C (>= 0), or -1 on failure.
 *
 * The instance named by Index is selected from the array. Index 0 maps
 * to the first item Pdh returns, which on ACPI systems is \_TZ.TZ00.
 */
static int ReadThermalZoneTenths(Measure *m)
{
    PDH_STATUS st;
    DWORD bufSize = 0;
    DWORD itemCount = 0;
    void *buf = NULL;
    int tenths = -1;

    if (!m->haveQuery)
    {
        PDH_HQUERY   q = NULL;
        PDH_HCOUNTER c = NULL;

        if (!EnsurePdh())
            return -1;

        st = pPdhOpenQuery(NULL, 0, &q);
        if (st != 0) return -1;

        st = pPdhAddCounter(q, L"\\Thermal Zone Information(*)\\Temperature", 0, &c);
        if (st != 0) { pPdhCloseQuery(q); return -1; }

        m->query     = q;
        m->counter   = c;
        m->haveQuery = 1;
    }

    st = pPdhCollect(m->query);
    if (st != 0) return -1;

    /* Size query: Pdh returns PDH_MORE_DATA and fills both values. */
    st = pPdhGetFormatted(m->counter, PDH_FMT_DOUBLE, &bufSize, &itemCount, NULL);
    if ((st != 0 && st != PDH_MORE_DATA) || bufSize == 0 || itemCount == 0)
        return -1;

    {
        /* Real stride, not the 24 the header implies. */
        DWORD stride = bufSize / itemCount;
        DWORD want   = (DWORD)m->index;

        if (stride < OFF_LONGVAL + 4)
            return -1;

        buf = HeapAlloc(GetProcessHeap(), 0, bufSize);
        if (!buf) return -1;

        st = pPdhGetFormatted(m->counter, PDH_FMT_DOUBLE, &bufSize, &itemCount, buf);
        if (st != 0 || itemCount == 0)
        {
            HeapFree(GetProcessHeap(), 0, buf);
            return -1;
        }

        if (want >= itemCount)
            want = 0;   /* fall back to the first zone */

        {
            unsigned char *p = (unsigned char *)buf + (size_t)want * stride;
            unsigned long  cstatus = *(unsigned long *)(p + OFF_CSTATUS);
            LONG           lval    = *(LONG *)(p + OFF_LONGVAL);

            if (cstatus == 0 && lval > 0 && lval < 20000)
                tenths = (int)lval;
        }

        HeapFree(GetProcessHeap(), 0, buf);
    }

    return tenths;
}

static void CloseThermalZone(Measure *m)
{
    if (m->haveQuery && m->query)
    {
        pPdhCloseQuery(m->query);
        m->query     = NULL;
        m->counter   = NULL;
        m->haveQuery = 0;
    }
}

/* ------------------------------------------------------------------ */
/* Rainmeter exports                                                   */
/* ------------------------------------------------------------------ */

#define PLUGIN_EXPORT extern "C" __declspec(dllexport)

PLUGIN_EXPORT void Initialize(void **data, void *rm)
{
    Measure *m;

    (void)rm;
    if (!data) return;

    *data = NULL;

    m = (Measure *)MeasureAlloc(sizeof(Measure));
    if (!m) return;

    m->magic = MEASURE_MAGIC;
    *data = m;
}

PLUGIN_EXPORT void Reload(void *data, void *rm, double *maxValue)
{
    Measure *m = (Measure *)data;

    if (!m) return;

    m->index = RmReadInt(rm, L"Index", 0);
    if (m->index < 0) m->index = 0;

    /* Options changed: drop the query so it is rebuilt on next Update. */
    CloseThermalZone(m);

    if (maxValue) *maxValue = 100.0;   /* graph scale: 0-100 C */
}

PLUGIN_EXPORT double Update(void *data)
{
    Measure *m = (Measure *)data;
    int tenths;

    if (!m) return 0.0;

    tenths = ReadThermalZoneTenths(m);
    if (tenths >= 0)
        m->lastTemp = (double)tenths / 10.0;

    return m->lastTemp;   /* 0.0 until the first successful read */
}

PLUGIN_EXPORT LPCWSTR GetString(void *data)
{
    (void)data;
    return NULL;   /* numeric metric: Rainmeter uses Update's value */
}

PLUGIN_EXPORT void Finalize(void *data)
{
    Measure *m = (Measure *)data;

    if (!m) return;

    if (m->magic == MEASURE_MAGIC)
    {
        CloseThermalZone(m);
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

    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        HMODULE hRm = GetModuleHandleW(L"Rainmeter.dll");
        if (hRm)
        {
            pRmReadString  = (RmReadString_t)GetProcAddress(hRm, "RmReadString");
            pRmReadFormula = (RmReadFormula_t)GetProcAddress(hRm, "RmReadFormula");
        }

        gPdhState = 0;
        hPdh      = NULL;
    }
    else if (fdwReason == DLL_PROCESS_DETACH)
    {
        /* Do NOT FreeLibrary(pdh): teardown can run under the loader lock
         * while another measure is still inside a PDH call. */
        gPdhState = 0;
        hPdh      = NULL;
    }

    return TRUE;
}