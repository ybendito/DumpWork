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
