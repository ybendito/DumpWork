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

static int WriteTestFile(LPCSTR Name)
{
    ULONGLONG countBytes = 1024L * 1024L * (ULONGLONG)Config().Count;
    ULONGLONG countUnits = countBytes / sizeof(ULONGLONG);

    FILE* f = fopen(Name, "w+b");
    int err = 0;
    if (!f) {
        ERR("Error creating %s, error 0x%X", Name, errno);
        return 1;
    }
    for (ULONGLONG i = 0; i < countUnits; ++i) {
        if (fwrite(&i, 1, sizeof(i), f) != sizeof(i)) {
            err = errno;
            ERR("Error writing %s, error 0x%X", Name, err);
            break;
        }
    }
    if (!err) {
        LOG("Written file %s of %d MB", Name, (ULONG)(countBytes / (1024 * 1024)));
    }
    fclose(f);
    return err;
}

static int VerifyTestFile(LPCSTR Name)
{
    ULONGLONG countBytes = 1024L * 1024L * (ULONGLONG)Config().Count;
    ULONGLONG countUnits = countBytes / sizeof(ULONGLONG);
    FILE* f = fopen(Name, "r+b");
    int err = 0;
    ULONG percentage = 0;
    if (!f) {
        ERR("Error opening %s, error 0x%X", Name, errno);
        return 1;
    }
    for (ULONGLONG i = 0; !err && i < countUnits; ++i) {
        ULONGLONG val;
        if (fread(&val, 1, sizeof(&val), f) != sizeof(val)) {
            err = errno;
            ERR("Error reading %s, offset 0x%I64X, error 0x%X", Name, i, err);
            break;
        }
        ULONG curPercentage = ULONG((i * 100L) / countUnits);
        if (curPercentage != percentage) {
            percentage = curPercentage;
            printf(".");
        }
    }
    LOG("");
    fclose(f);
    return err;
}

class CFilesHandler : public CCommandHandler
{
public:
    CFilesHandler() : CCommandHandler("files", "some file operations", 2) {}
private:
    int Run(const CStringArray& Parameters) override
    {
        if (!Parameters[0].CompareNoCase("hold")) {
            return HoldFile(Parameters[1], Parameters[2]);
        } else if (!Parameters[0].CompareNoCase("write")) {
            return WriteTestFile(Parameters[1]);
        } else if (!Parameters[0].CompareNoCase("verify")) {
            return VerifyTestFile(Parameters[1]);
        } else {
            return Usage();
        }
    }
    void Help(CStringArray& a) override
    {
        a.Add("hold \t<filename> <r|w|rw>\tkeeps file open with r|w denial");
        a.Add("write \t<filename> -count:size>\twrite file for further verify, size in MBs");
        a.Add("verify\t<filename> -count:size>\tverify file, size in MBs");
    }
};

static CFilesHandler fh;
