#pragma once

template <typename t> LPCSTR Name(ULONG val)
{
    LPCSTR GetName(const t&);
    t a;
    a = (t)val;
    return GetName(a);
}

template <typename t> CString Flags(ULONG _val)
{
    CString s;
    for (ULONG i = 0; i < 32; ++i) {
        ULONG val = 1 << i;
        if (val & _val) {
            t a;
            a = (t)val;
            auto name = Name<t>(a);
            s += name;
            s += ',';
        }
    }
    if (!s.IsEmpty()) s += "\b";
    return s;
}

typedef enum { eDummy1 = DEBUG_SYMTYPE_NONE } eDEBUG_SYMTYPE;
typedef enum { eDummy2 = DEBUG_SYMTYPE_NONE } eDEBUG_MODULE;
typedef enum { eDummy3 = DEBUG_SYMTYPE_NONE } eSYMBOL_ERROR;
typedef enum { eDummy4 = DEBUG_SYMTYPE_NONE } eDEBUG_VALUE_TYPE;
