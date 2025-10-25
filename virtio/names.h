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

template <typename t> CString FlagsTable(ULONG _val)
{
    CString s;
    ULONG unknown = 0;
    for (ULONG i = 0; i < 32; ++i) {
        ULONG val = 1 << i;
        bool bOn = val & _val;
        t a;
        a = (t)i;
        auto name = Name<t>(a);
        if (!name) {
            if (bOn) {
                unknown |= val;
            }
            continue;
        }
        s += '[';
        s += bOn ? 'x' : ' ';
        s += ']';
        s.AppendFormat("(%d)", i);
        s += name;
        s += '\n';
    }
    if (unknown) {
        s.AppendFormat(" unknown bits %X\n", unknown);
    }
    return s;
}

typedef enum { eDummy1 = DEBUG_SYMTYPE_NONE } eDEBUG_SYMTYPE;
typedef enum { eDummy2 = DEBUG_SYMTYPE_NONE } eDEBUG_MODULE;
typedef enum { eDummy3 = DEBUG_SYMTYPE_NONE } eSYMBOL_ERROR;
typedef enum { eDummy4 = DEBUG_SYMTYPE_NONE } eDEBUG_VALUE_TYPE;
typedef enum { eDummy5 = DEBUG_SYMTYPE_NONE } eHV_FLAG_TYPE;
typedef enum { eDummy6 = DEBUG_SYMTYPE_NONE } eHV_ENLIGHTMENT_TYPE;
