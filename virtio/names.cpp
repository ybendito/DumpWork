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
