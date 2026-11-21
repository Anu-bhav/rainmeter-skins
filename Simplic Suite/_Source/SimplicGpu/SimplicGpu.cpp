/*
 * SimplicGpu - NVML measure for Rainmeter
 *
 * Reports overall NVIDIA GPU load, VRAM and temperature straight from NVML
 * (nvml.dll ships with every NVIDIA driver). No external process, no temp
 * files, no polling script.
 *
 * Options
 *   Metric   Usage (default) | VramUsed | VramTotal | Temp | Name
 *   Index    Which GPU to read (default 0)
 *
 * The number value drives graphs and measures. Metric=Name also sets the
 * string value; every other metric returns nullptr from GetString so Rainmeter
 * uses the number value (per the plugin guidelines).
 *
 * ---------------------------------------------------------------------------
 * Correctness notes (see docs.rainmeter.net/developers/plugin/guidelines)
 *
 *  - Exported signatures follow the official Plugin SDK exactly:
 *        Initialize(void** data, void* rm)
 *        Reload    (void*  data, void* rm, double* maxValue)
 *        Update    (void*  data)            -> double
 *        GetString (void*  data)            -> LPCWSTR
 *        Finalize  (void*  data)            <-- void*, NOT void**
 *    Getting Finalize wrong (void**) makes Rainmeter pass the measure pointer
 *    where the plugin expects a pointer-to-pointer; the plugin then frees an
 *    arbitrary address and the process dies on skin unload.
 *
 *  - Options must be read in Reload, not Initialize: Rainmeter calls
 *    Initialize before the measure's option table is populated, so
 *    RmReadString returns an empty string there and every measure would
 *    silently fall back to METRIC_USAGE.
 *
 *  - GetString is called many times per cycle (meters, section variables,
 *    the About dialog) so it is kept trivial: it returns a cached buffer and
 *    does no NVML work.
 *
 *  - Rainmeter serialises measure updates, but a skin may be unloaded at any
 *    time, so Update/Finalize must be safe without relying on ordering.
 *
 *  - NVML is documented as thread-safe, and nvmlShutdown() is never called:
 *    it can block on driver threads and may run under the loader lock.
 *
 *  - No CRT is linked. Only kernel32 is imported, and memory comes from
 *    HeapAlloc/HeapFree with GetProcessHeap, always paired.
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

/* Mirrors RmReadString; falls back to defValue if the host API is missing. */
static LPCWSTR RmReadString(void *rm, LPCWSTR option, LPCWSTR defValue)
{
    if (pRmReadString)
        return pRmReadString(rm, option, defValue, FALSE);
    return defValue;
}

/* Mirrors RmReadInt. */
static int RmReadInt(void *rm, LPCWSTR option, int defValue)
{
    if (pRmReadFormula)
        return (int)pRmReadFormula(rm, option, (double)defValue);
    return defValue;
}

/* ------------------------------------------------------------------ */
/* NVML types and function pointers                                    */
/* ------------------------------------------------------------------ */

typedef void *nvmlDevice_t;

typedef struct
{
    unsigned int gpu;    /* percent */
    unsigned int memory; /* percent */
} nvmlUtilization_t;

typedef struct
{
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
} nvmlMemory_t;

typedef enum { NVML_SUCCESS = 0 } nvmlReturn_t;

typedef nvmlReturn_t (*fnInit)(void);
typedef nvmlReturn_t (*fnGetCount)(unsigned int *count);
typedef nvmlReturn_t (*fnGetHandle)(unsigned int index, nvmlDevice_t *device);
typedef nvmlReturn_t (*fnGetUtil)(nvmlDevice_t device, nvmlUtilization_t *util);
typedef nvmlReturn_t (*fnGetMemory)(nvmlDevice_t device, nvmlMemory_t *mem);
typedef nvmlReturn_t (*fnGetTemp)(nvmlDevice_t device, int sensorType, unsigned int *temp);
typedef nvmlReturn_t (*fnGetName)(nvmlDevice_t device, char *name, unsigned int length);

static fnInit      pNvmlInit      = NULL;
static fnGetCount  pNvmlGetCount  = NULL;
static fnGetHandle pNvmlGetHandle = NULL;
static fnGetUtil   pNvmlGetUtil   = NULL;
static fnGetMemory pNvmlGetMemory = NULL;
static fnGetTemp   pNvmlGetTemp   = NULL;
static fnGetName   pNvmlGetName   = NULL;

static HMODULE      hNvml         = NULL;
static int          gNvmlState    = 0; /* 0 = untried, 1 = ready, -1 = failed */
static unsigned int gDeviceCount  = 0;

/* Serialises lazy initialisation. Rainmeter mostly updates measures serially,
 * but a skin unload can race a refresh, so the state transition is guarded. */
static SRWLOCK gNvmlLock = SRWLOCK_INIT;

/* NVML_TEMPERATURE_GPU */
#define NVML_TEMPERATURE_GPU 0

/*
 * Loads nvml.dll once and resolves every function we need.
 *
 * nvmlShutdown() is deliberately never called: teardown can block on driver
 * threads, and this code can run during process exit. The OS reclaims the
 * library and its handles when the process ends.
 *
 * Returns 1 when NVML is ready, 0 otherwise. Safe to call concurrently.
 */
static int EnsureNvml(void)
{
    int i;
    int result = 0;

    /* Fast path: already resolved. gNvmlState is only written under the lock,
     * so a stale read here can only cost an extra lock acquisition. */
    if (gNvmlState == 1)
        return 1;
    if (gNvmlState == -1)
        return 0;

    AcquireSRWLockExclusive(&gNvmlLock);

    /* Re-check under the lock: another thread may have finished while we
     * were waiting. */
    if (gNvmlState == 1)
    {
        result = 1;
        goto out;
    }
    if (gNvmlState == -1)
    {
        result = 0;
        goto out;
    }

    hNvml = LoadLibraryW(L"nvml.dll");
    if (!hNvml)
    {
        gNvmlState = -1;
        result = 0;
        goto out;
    }

#define RESOLVE(field, name)                                     \
    field = (decltype(field))GetProcAddress(hNvml, name);        \
    if (!field) { gNvmlState = -1; result = 0; goto out; }

    RESOLVE(pNvmlInit,      "nvmlInit_v2");
    RESOLVE(pNvmlGetCount,  "nvmlDeviceGetCount_v2");
    RESOLVE(pNvmlGetHandle, "nvmlDeviceGetHandleByIndex_v2");
    RESOLVE(pNvmlGetUtil,   "nvmlDeviceGetUtilizationRates");
    RESOLVE(pNvmlGetMemory, "nvmlDeviceGetMemoryInfo");
    RESOLVE(pNvmlGetTemp,   "nvmlDeviceGetTemperature");
    RESOLVE(pNvmlGetName,   "nvmlDeviceGetName");

#undef RESOLVE

    if (pNvmlInit() != NVML_SUCCESS)
    {
        gNvmlState = -1;
        result = 0;
        goto out;
    }

    /* The device count can be momentarily unavailable right after init. */
    for (i = 0; i < 8; ++i)
    {
        if (pNvmlGetCount(&gDeviceCount) == NVML_SUCCESS)
            break;
        gDeviceCount = 0;
        Sleep(10);
    }

    if (gDeviceCount == 0)
    {
        gNvmlState = -1;
        result = 0;
        goto out;
    }

    gNvmlState = 1;
    result = 1;

out:
    ReleaseSRWLockExclusive(&gNvmlLock);
    return result;
}

/* ------------------------------------------------------------------ */
/* Measure state                                                       */
/* ------------------------------------------------------------------ */

enum Metric
{
    METRIC_USAGE = 0,
    METRIC_VRAM_USED,
    METRIC_VRAM_TOTAL,
    METRIC_TEMP,
    METRIC_NAME
};

/* Guards against freeing a block this DLL did not allocate. */
#define MEASURE_MAGIC 0x53504755u /* "SPGU" */

/* Name buffer. NVML requires at least NVML_DEVICE_NAME_BUFFER_SIZE (64) bytes;
 * 128 is ample for any real GPU name and keeps the struct small. */
#define NAME_SLOTS 128

typedef struct
{
    unsigned int magic;      /* MEASURE_MAGIC while this block is ours */

    int          metric;
    unsigned int index;

    nvmlDevice_t device;
    int          haveDevice;

    wchar_t      name[NAME_SLOTS];  /* Metric=Name only, inline: no allocation */
} Measure;

/* Heap helpers. Allocation and release must use the same allocator: mixing
 * HeapAlloc with free/new is a classic heap-corruption crash. */
static void *MeasureAlloc(size_t bytes)
{
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes);
}

static void MeasureFree(void *block)
{
    if (block)
        HeapFree(GetProcessHeap(), 0, block);
}

/* Case-insensitive compare for the fixed metric names. Avoids _wcsicmp so the
 * UCRT is not needed; CompareStringOrdinal is a kernel32 call.
 *
 * Returns 1 when the strings are equal and 0 when they are not, so callers
 * must test for a non-zero result. */
static int StrEqW(const wchar_t *a, const wchar_t *b)
{
    return CompareStringOrdinal(a, -1, b, -1, TRUE) == CSTR_EQUAL;
}

static int MetricFromWString(const wchar_t *s)
{
    if (!s)
        return METRIC_USAGE;

    if (StrEqW(s, L"Usage") != 0)     return METRIC_USAGE;
    if (StrEqW(s, L"VramUsed") != 0)  return METRIC_VRAM_USED;
    if (StrEqW(s, L"VramTotal") != 0) return METRIC_VRAM_TOTAL;
    if (StrEqW(s, L"Temp") != 0)      return METRIC_TEMP;
    if (StrEqW(s, L"Name") != 0)      return METRIC_NAME;

    return METRIC_USAGE;
}

/* Returns 1 when the measure has a usable device handle. */
static int ResolveDevice(Measure *m)
{
    nvmlDevice_t dev;

    if (m->haveDevice)
        return 1;

    if (!EnsureNvml())
        return 0;

    if (m->index >= gDeviceCount)
        return 0;

    dev = NULL;
    if (pNvmlGetHandle(m->index, &dev) != NVML_SUCCESS || !dev)
        return 0;

    m->device     = dev;
    m->haveDevice = 1;
    return 1;
}

/*
 * Reads the GPU name into the measure's fixed buffer once, then caches it.
 *
 * Two details that commonly go wrong:
 *
 *  1. MultiByteToWideChar with cbMultiByte == -1 includes the terminating NUL
 *     in its result. Using that count as an index writes one element past the
 *     end when the string exactly fills the buffer. We pass an explicit byte
 *     length instead and always terminate ourselves.
 *
 *  2. cchWideChar is a CHARACTER count and must leave room for the NUL.
 */
static void CacheDeviceName(Measure *m)
{
    char buf[128];
    int  len;
    int  n;

    if (m->name[0] != 0 || !pNvmlGetName || !m->device)
        return;

    ZeroMemory(buf, sizeof(buf));

    if (pNvmlGetName(m->device, buf, (unsigned int)(sizeof(buf) - 1)) != NVML_SUCCESS)
        return;

    /* Force termination even if the driver filled the buffer completely. */
    buf[sizeof(buf) - 1] = 0;

    len = 0;
    while (len < (int)sizeof(buf) && buf[len] != 0)
        ++len;

    if (len == 0)
        return;

    /* Convert at most NAME_SLOTS-1 characters, leaving room for the NUL. */
    n = MultiByteToWideChar(CP_UTF8, 0, buf, len, m->name, NAME_SLOTS - 1);
    if (n <= 0)
        return;

    if (n > NAME_SLOTS - 1)
        n = NAME_SLOTS - 1;

    m->name[n] = 0;
}

/* ------------------------------------------------------------------ */
/* Rainmeter exports (signatures match the official Plugin SDK)          */
/* ------------------------------------------------------------------ */

/*
 * Rainmeter resolves the five plugin entry points by name through
 * GetProcAddress, so they must keep their C names. extern "C" prevents C++
 * name mangling; without it the DLL exports _Z10InitializePPvS_ and Rainmeter
 * cannot find "Initialize".
 */
#define PLUGIN_EXPORT extern "C" __declspec(dllexport)

PLUGIN_EXPORT void Initialize(void **data, void *rm)
{
    Measure *m;

    if (!data)
        return;

    *data = NULL;

    m = (Measure *)MeasureAlloc(sizeof(Measure));
    if (!m)
        return;

    m->magic = MEASURE_MAGIC;

    /* Options are deliberately NOT read here. Rainmeter calls Initialize
     * before the measure's option table is populated, so RmReadString returns
     * an empty string at this point and every measure would silently fall back
     * to METRIC_USAGE. Reload is the documented place to read options and is
     * called immediately after Initialize. */
    (void)rm;

    *data = m;
}

PLUGIN_EXPORT void Reload(void *data, void *rm, double *maxValue)
{
    Measure *m = (Measure *)data;
    int idx;

    if (!m)
        return;

    /* RmReadString returns a pointer into Rainmeter's internal buffer, valid
     * only until the next API call, so each value is consumed immediately and
     * never stored. */
    m->metric = MetricFromWString(RmReadString(rm, L"Metric", L"Usage"));

    /* Clamp so a negative Index= in the skin cannot wrap to a huge value. */
    idx = RmReadInt(rm, L"Index", 0);
    m->index = (idx > 0) ? (unsigned int)idx : 0u;

    /* Options changed: drop any cached device handle and name. */
    m->haveDevice = 0;
    m->device     = NULL;
    m->name[0]    = 0;

    if (maxValue)
        *maxValue = 0.0;
}

PLUGIN_EXPORT double Update(void *data)
{
    Measure *m = (Measure *)data;
    nvmlUtilization_t util;
    nvmlMemory_t mem;

    if (!m)
        return 0.0;

    if (!ResolveDevice(m))
        return 0.0;

    switch (m->metric)
    {
    case METRIC_USAGE:
        ZeroMemory(&util, sizeof(util));
        if (pNvmlGetUtil(m->device, &util) == NVML_SUCCESS)
            return (double)util.gpu;
        break;

    case METRIC_VRAM_USED:
    case METRIC_VRAM_TOTAL:
        ZeroMemory(&mem, sizeof(mem));
        if (pNvmlGetMemory(m->device, &mem) == NVML_SUCCESS)
            return (m->metric == METRIC_VRAM_USED)
                       ? (double)mem.used
                       : (double)mem.total;
        break;

    case METRIC_TEMP:
    {
        unsigned int temp = 0;
        if (pNvmlGetTemp(m->device, NVML_TEMPERATURE_GPU, &temp) == NVML_SUCCESS)
            return (double)temp;
        break;
    }

    case METRIC_NAME:
        /* The name is a string metric; the value itself is served by
         * GetString, so Update only warms the cache. */
        CacheDeviceName(m);
        break;
    }

    return 0.0;
}

/*
 * Returns the measure's string value, or nullptr when it has none.
 *
 * Per the plugin guidelines this is called many times per cycle (meters,
 * section variables, the About dialog), so it must stay trivial: no NVML calls,
 * no allocation, just a cached pointer.
 *
 * Returning nullptr for numeric metrics is required: it tells Rainmeter to use
 * the number value from Update instead of a string.
 */
PLUGIN_EXPORT LPCWSTR GetString(void *data)
{
    Measure *m = (Measure *)data;

    if (!m)
        return NULL;

    if (m->metric == METRIC_NAME && m->name[0] != 0)
        return m->name;

    return NULL;
}

/*
 * Releases the measure.
 *
 * NOTE: the signature is void* (not void**) to match the official SDK. Rainmeter
 * passes the measure pointer directly; treating it as void** dereferences an
 * arbitrary address and then frees it, which crashes the process on unload.
 */
PLUGIN_EXPORT void Finalize(void *data)
{
    Measure *m = (Measure *)data;

    if (!m)
        return;

    /* Only free a block this DLL allocated, and only once. */
    if (m->magic == MEASURE_MAGIC)
    {
        m->magic = 0;
        MeasureFree(m);
    }
}

/* ------------------------------------------------------------------ */
/* DLL entry point                                                     */
/* ------------------------------------------------------------------ */

/*
 * MinGW needs DllMainCRTStartup as the entry point. The CRT normally provides
 * it, but we deliberately do not link the CRT, so it is defined here.
 *
 * extern "C" is required: without it the symbol is C++-mangled, the linker
 * cannot find the entry point, and the DLL is emitted with
 * AddressOfEntryPoint == 0, which fails to load.
 */
extern "C" BOOL WINAPI DllMainCRTStartup(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    (void)hinstDLL;
    (void)lpvReserved;

    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        /* Rainmeter.dll is already loaded in-process and exports the API. */
        HMODULE hRm = GetModuleHandleW(L"Rainmeter.dll");
        if (hRm)
        {
            pRmReadString  = (RmReadString_t)GetProcAddress(hRm, "RmReadString");
            pRmReadFormula = (RmReadFormula_t)GetProcAddress(hRm, "RmReadFormula");
        }

        /* Everything else is initialised lazily on first use. */
        gNvmlState   = 0;
        gDeviceCount = 0;
        hNvml        = NULL;
    }
    else if (fdwReason == DLL_PROCESS_DETACH)
    {
        /* Do NOT call nvmlShutdown() here: DLL_PROCESS_DETACH can run under the
         * loader lock, and NVML teardown can block on driver threads. That is
         * the bug that makes some plugins hang Rainmeter on unload.
         *
         * Do NOT FreeLibrary(nvml) either: another measure may still be inside
         * an NVML call, and unmapping the library would fault. The OS unloads
         * everything when the process exits. */
        gNvmlState   = 0;
        gDeviceCount = 0;
        hNvml        = NULL;
    }

    return TRUE;
}