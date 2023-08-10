#include "stdafx.h"

static int HoldFile(LPCSTR Name, bool FromRead, bool FromWrite)
{
    CFile f;
    int res = 0;
    CFileException ex;
    ULONG flags = 0;
    if (FromRead) flags |= CFile::shareDenyRead;
    if (FromWrite) flags |= CFile::shareDenyWrite;
    if (f.Open(Name, CFile::modeRead | flags , &ex)) {
        LOG("Hit Enter to stop holding %s from (%s%s)", Name, FromRead ? "R" : "", FromWrite ? "W" : "");
        getchar();
    } else {
        ERR("Can't open, error %d", ex.m_lOsError);
        return res;
    }
    return res;
}

static int HoldFile(LPCSTR Name, LPCSTR Param)
{
    bool fromRead = strchr(Param, 'r') || strchr(Param, 'R');
    bool fromWrite = strchr(Param, 'w') || strchr(Param, 'W');
    return HoldFile(Name, fromRead, fromWrite);
}

class CFilesHandler : public CCommandHandler
{
public:
    CFilesHandler() : CCommandHandler("files", "some file operations", 3) {}
private:
    int Run(const CStringArray& Parameters) override
    {
        if (!Parameters[0].CompareNoCase("hold")) {
            return HoldFile(Parameters[1], Parameters[2]);
        } else if (false) {
            return 0;
        } else {
            return Usage();
        }
    }
    void Help(CStringArray& a) override
    {
        a.Add("hold \t<filename> <r|w|rw>\tkeeps file open with r|w denial");
    }
};

static CFilesHandler fh;