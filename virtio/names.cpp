#include "pch.h"

typedef struct _tNamedEntry
{
    ULONG value;
    LPCSTR name;
}tNamedEntry;

#define MAKE_ENTRY(e) { e, #e},
#define GET_NAME(table, val) GetName(table, ELEMENTS_IN(table), val)

static LPCSTR GetName(const tNamedEntry* table, UINT size, ULONG val)
{
    for (UINT i = 0; i < size; ++i)
    {
        if (table[i].value == val) return table[i].name;
    }
    return "Unknown";
}

LPCSTR GetName(const eDEBUG_SYMTYPE& val)
{
# undef MAKE_ENTRY
#define MAKE_ENTRY(e) { DEBUG_SYMTYPE_##e, #e},

    static tNamedEntry names[] = {
        MAKE_ENTRY(NONE)
        MAKE_ENTRY(COFF)
        MAKE_ENTRY(CODEVIEW)
        MAKE_ENTRY(PDB)
        MAKE_ENTRY(EXPORT)
        MAKE_ENTRY(DEFERRED)
        MAKE_ENTRY(SYM)
        MAKE_ENTRY(DIA)
    };
    return GET_NAME(names, val);
}

LPCSTR GetName(const eDEBUG_MODULE& val)
{
# undef MAKE_ENTRY
#define MAKE_ENTRY(e) { DEBUG_MODULE_##e, #e},

    static tNamedEntry names[] = {
        MAKE_ENTRY(LOADED)
        MAKE_ENTRY(UNLOADED)
        MAKE_ENTRY(USER_MODE)
        MAKE_ENTRY(EXE_MODULE)
        MAKE_ENTRY(EXPLICIT)
        MAKE_ENTRY(SECONDARY)
        MAKE_ENTRY(SYNTHETIC)
        MAKE_ENTRY(SYM_BAD_CHECKSUM)
    };
    return GET_NAME(names, val);
}

LPCSTR GetName(const eSYMBOL_ERROR& val)
{
# undef MAKE_ENTRY
#define MAKE_ENTRY(e) { ##e, #e},
    static tNamedEntry names[] = {
        MAKE_ENTRY(MEMORY_READ_ERROR)
        MAKE_ENTRY(SYMBOL_TYPE_INDEX_NOT_FOUND)
        MAKE_ENTRY(SYMBOL_TYPE_INFO_NOT_FOUND)
        MAKE_ENTRY(NULL_SYM_DUMP_PARAM)
        MAKE_ENTRY(NULL_FIELD_NAME)
        MAKE_ENTRY(INCORRECT_VERSION_INFO)
        MAKE_ENTRY(INSUFFICIENT_SPACE_TO_COPY)
        MAKE_ENTRY(ADDRESS_TYPE_INDEX_NOT_FOUND)
        MAKE_ENTRY(EXIT_ON_CONTROLC)
    };
    return GET_NAME(names, val);
}

LPCSTR GetName(const eDEBUG_VALUE_TYPE& val)
{
# undef MAKE_ENTRY
#define MAKE_ENTRY(e) { DEBUG_VALUE_##e, #e},
    static tNamedEntry names[] = {
        MAKE_ENTRY(INVALID)
        MAKE_ENTRY(INT8)
        MAKE_ENTRY(INT16)
        MAKE_ENTRY(INT32)
        MAKE_ENTRY(INT64)
        MAKE_ENTRY(FLOAT32)
        MAKE_ENTRY(FLOAT64)
        MAKE_ENTRY(FLOAT80)
        MAKE_ENTRY(FLOAT82)
        MAKE_ENTRY(FLOAT128)
        MAKE_ENTRY(VECTOR64)
        MAKE_ENTRY(VECTOR128)
    };
    return GET_NAME(names, val);
}

LPCSTR GetName(const enum SymTagEnum& val)
{
# undef MAKE_ENTRY
#define MAKE_ENTRY(e) { SymTag##e, #e},
    static tNamedEntry names[] = {
        MAKE_ENTRY(Null)
        MAKE_ENTRY(Exe)
        MAKE_ENTRY(Compiland)
        MAKE_ENTRY(CompilandDetails)
        MAKE_ENTRY(CompilandEnv)
        MAKE_ENTRY(Function)
        MAKE_ENTRY(Block)
        MAKE_ENTRY(Data)
        MAKE_ENTRY(Annotation)
        MAKE_ENTRY(Label)
        MAKE_ENTRY(PublicSymbol)
        MAKE_ENTRY(UDT)
        MAKE_ENTRY(Enum)
        MAKE_ENTRY(FunctionType)
        MAKE_ENTRY(PointerType)
        MAKE_ENTRY(ArrayType)
        MAKE_ENTRY(BaseType)
        MAKE_ENTRY(Typedef)
        MAKE_ENTRY(Custom)
        MAKE_ENTRY(CustomType)
    };
    return GET_NAME(names, val);
}

LPCSTR GetName(const eHV_FLAG_TYPE& val)
{
    // meaning of flags aligned per windbg 26100.4202
    LPCSTR bits[32] = {
        "ApicEnlightened", // 0
        "CpuManager", // 1
        "DynamicCpuDisabled", // 2
        "Phase0InitDone", // 3
        "DeprecateAutoEoi", // 4
        "SynicAvailable", // 5
        "VsmAvailable", // 6
        "ExtendedProcessorMasks", // 7
        NULL, // 8 MaxBankNumber.0
        NULL, // 9 MaxBankNumber.1
        NULL, // 10 MaxBankNumber.2
        NULL, // 11 MaxBankNumber.3
        "HypervisorPresent", // 12
        "NoExtendedRangeFlush", // 13
        "UseQpcBias", // 14
        "RootScheduler", // 15
        "PowerSchedulerQos", // 16
        "HardwareMbecAvailable", // 17
        "MemoryZeroingControl", // 18
        "VpAssistPage", // 19
        "Epf", // 20
        "AsyncMemoryHint", // 21
        "NoNonArchCoreSharing", // 22
        "CoreSchedulerRequested", // 23
        "ApicVirtualizationAvailable", // 24
        NULL, // 25
        NULL, // 26
        NULL, // 27
        NULL, // 28
        NULL, // 29
        NULL, // 30
        NULL, // 31
    };
    if (val >= ARRAYSIZE(bits)) {
        return NULL;
    }
    return bits[val];
}

#if 0
LPCSTR GetName(const eHV_ENLIGHTMENT_TYPE& val)
{
    LPCSTR bits[32] = {
        "UseHypercallForAddressSpaceSwitch", // 0
        "UseHypercallForLocalFlush", // 1
        "UseHypercallForRemoteFlush", // 2
        "UseApicMsrs", // 3
        "UseHvRegisterForReset", // 4
        "UseRelaxedTiming", // 5
        "?", // 6 (Old UseDmaRemapping)
        "UseInterruptRemapping", // 7
        "UseX2ApicMsrs", // 8
        "DeprecateAutoEoi", // 9
        "UseSyntheticClusterIpi", // 10
        "UseExProcessorMasks", // 11
        "Nested", // 12
        "UseVirtualTlbFlush", // 13
        "UseVmcsEnlightenments", // 14
        "UseSyncedTimeline", // 15
        "Unknown", // 16
        "Unknown", // 17
        "Unknown", // 18
        "Unknown", // 19
        "Unknown", // 20
        "Unknown", // 21
        "Unknown", // 22
        "Unknown", // 23
        "Unknown", // 24
        "Unknown", // 25
        "Unknown", // 26
        "Unknown", // 27
        "Unknown", // 28
        "Unknown", // 29
        "Unknown", // 30
        "Unknown", // 31
    };
    if (val >= ARRAYSIZE(bits)) {
        return NULL;
    }
    return bits[val];
}
#else
LPCSTR GetName(const eHV_ENLIGHTMENT_TYPE& val)
{
    // reflects QEMU settings
    // no effect: vpindex, frequencies, runtime, reenlightenment
    // no effect: stimer, stimer_direct
    LPCSTR bits[32] = {
        NULL, // 0
        NULL, // 1
        "tlbflush", // 2
        NULL, // 3
        "vapic", // 4
        "relaxed", // 5
        "spinlock", // 6
        NULL, // 7
        "time", // 8
        NULL, // 9
        NULL, // 10
        NULL, // 11
        NULL, // 12
        NULL, // 13
        "ipi", // 14
        NULL, // 15
        NULL, // 16
        NULL, // 17
        NULL, // 18
        NULL, // 19
        NULL, // 20
        NULL, // 21
        NULL, // 22
        NULL, // 23
        NULL, // 24
        NULL, // 25
        NULL, // 26
        NULL, // 27
        NULL, // 28
        NULL, // 29
        NULL, // 30
        NULL, // 31
    };
    if (val >= ARRAYSIZE(bits)) {
        return NULL;
    }
    return bits[val];
}
#endif