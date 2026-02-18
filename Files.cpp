#include "stdafx.h"

class CFileReader
{
public:
    CFileReader() {}
    ~CFileReader()
    {
        if (m_FilePtr) {
            fclose(m_FilePtr);
        }
    }
    bool Open(LPCSTR Name, bool Uncached, bool UseWinApi)
    {
        if (Uncached && !UseWinApi) {
            ERR("%s: uncached access requires WinApi", Name);
            return false;
        }
        if (!UseWinApi) {
            m_FilePtr = fopen(Name, "r+b");
            if (!m_FilePtr) {
                ERR("Error creating %s, error 0x%X", Name, errno);
            }
            return m_FilePtr;
        }
        CFileException ex;
        UINT flags = CFile::modeRead | CFile::shareDenyRead;
        if (Uncached) {
            flags |= CFile::osNoBuffer;
        }
        if (!m_File.Open(Name, flags, &ex)) {
            ERR("Error opening %s, error 0x%X", Name, ex.m_lOsError);
            return false;
        }
        return true;
    }
    bool Read(PVOID Buffer, ULONG Size)
    {
        if (m_FilePtr) {
            return fread(Buffer, 1, Size, m_FilePtr) == Size;
        }
        return m_File.Read(Buffer, Size) == Size;
    }
private:
    FILE* m_FilePtr = NULL;
    CFile m_File;
};

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
        if (i != val) {
            ERR("Error value @%I64d != %I64d", i, val);
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

static void VerifyTestFile(LPCSTR Name, ULONGLONG* Buffer, ULONG NumberOfUnits, bool Uncached, bool WinApi)
{
    ULONG sizeOfBuffer = NumberOfUnits * sizeof(*Buffer);
    ULONGLONG countBytes = 1024L * 1024L * (ULONGLONG)Config().Count;
    ULONGLONG countUnits = countBytes / sizeof(ULONGLONG);
    CFileReader fr;
    if (!fr.Open(Name, Uncached, WinApi)) {
        return;
    }
    ULONGLONG fileOffset = 0;
    ULONG percentage = 0;
    for (ULONGLONG i = 0; i < countUnits; ++i) {
        ULONGLONG val;
        if (i >= fileOffset) {
            if (!fr.Read(Buffer, sizeOfBuffer)) {
                ERR("Error reading %s, offset 0x%I64X", Name, i);
                break;
            }
            fileOffset += NumberOfUnits;
            ULONG curPercentage = ULONG((i * 100L) / countUnits);
            if (curPercentage != percentage) {
                percentage = curPercentage;
                printf(".");
            }
        }
        val = Buffer[i % NumberOfUnits];
        if (i != val) {
            ERR("Error value @%I64d != %I64d", i, val);
            break;
        }
    }
    LOG("");
}

static void JustReadTestFile(LPCSTR Name, PVOID Buffer, ULONG BufferSize, bool Uncached, bool WinApi)
{
    ULONGLONG countBytes = 1024L * 1024L * (ULONGLONG)Config().Count;
    ULONG rounds = (ULONG)(countBytes / BufferSize);
    CFileReader fr;
    if (!fr.Open(Name, Uncached, WinApi)) {
        return;
    }
    ULONGLONG fileOffset = 0;
    ULONG percentage = 0;
    for (ULONGLONG i = 0; i < rounds; ++i) {
        if (!fr.Read(Buffer, BufferSize)) {
            ERR("Error reading %s, offset 0x%I64X", Name, i);
            break;
        }
    }
}


#if 0
static int VerifyTestFile2(LPCSTR Name)
{
    static ULONGLONG buffer[(1024L * 1024L) / sizeof(ULONGLONG)];
    ULONGLONG countBytes = 1024L * 1024L * (ULONGLONG)Config().Count;
    ULONGLONG countUnits = countBytes / sizeof(ULONGLONG);
    FILE* f = fopen(Name, "r+b");
    ULONGLONG fileOffset = 0;
    int err = 0;
    ULONG percentage = 0;
    if (!f) {
        ERR("Error opening %s, error 0x%X", Name, errno);
        return 1;
    }
    for (ULONGLONG i = 0; !err && i < countUnits; ++i) {
        ULONGLONG val;
        if (i >= fileOffset) {
            if (fread(buffer, 1, sizeof(buffer), f) != sizeof(buffer)) {
                err = errno;
                ERR("Error reading %s, offset 0x%I64X, error 0x%X", Name, i, err);
                break;
            }
            fileOffset += ARRAYSIZE(buffer);

            ULONG curPercentage = ULONG((i * 100L) / countUnits);
            if (curPercentage != percentage) {
                percentage = curPercentage;
                printf(".");
            }
        }
        val = buffer[i % ARRAYSIZE(buffer)];
        if (i != val) {
            ERR("Error value @%I64d != %I64d", i, val);
            break;
        }
    }
    LOG("");
    fclose(f);
    return err;
}
#endif

static int _HoldFile(const CStringArray& Parameters)
{
    return HoldFile(Parameters[0], Parameters[1]);
}

static int _WriteTestFile(const CStringArray& Parameters)
{
    if (!Config().Count) {
        LOG("-count:<value> is mandatory");
        return 1;
    }
    return WriteTestFile(Parameters[0]);
}

static int _VerifyTestFile(const CStringArray& Parameters)
{
    if (!Config().Count) {
        LOG("-count:<value> is mandatory");
        return 1;
    }
    return VerifyTestFile(Parameters[0]);
}

static int _Verify(const CStringArray& Parameters, bool Uncached, bool WinAPI)
{
    ULONG numOfUnits = GetNumber(Parameters[1]) / sizeof(ULONGLONG);
    if (!Config().Count) {
        LOG("-count:<value> is mandatory");
        return 1;
    }
    if (!numOfUnits) {
        LOG("using minimal block size");
        return VerifyTestFile(Parameters[0]);
    }

    ULONGLONG* buffer = new ULONGLONG[numOfUnits];
    if (!buffer) {
        LOG("can't allocate memory block");
        return 1;
    }
    ULONG loops = Config().Loops;
    for (ULONG i = 0; i <= loops; ++i) {
        VerifyTestFile(Parameters[0], buffer, numOfUnits, Uncached, WinAPI);
    }
    delete[] buffer;

    return 0;
}

static int _VerifyTestFileApi(const CStringArray& Parameters)
{
    return _Verify(Parameters, false, true);
}

static int _VerifyTestFileCrt(const CStringArray& Parameters)
{
    return _Verify(Parameters, false, false);
}

static int _VerifyTestFileApiUncached(const CStringArray& Parameters)
{
    return _Verify(Parameters, true, true);
}

static int _JustReadUncached(const CStringArray& Parameters)
{
    ULONG bufferSize = GetNumber(Parameters[1]);
    if (!Config().Count) {
        LOG("-count:<value> is mandatory");
        return 1;
    }
    if (!bufferSize) {
        LOG("buffer size parameter is mandatory");
        return 1;
    }

    PVOID buffer = new BYTE[bufferSize];
    if (!buffer) {
        LOG("can't allocate memory block");
        return 1;
    }
    ULONG loops = Config().Loops;
    for (ULONG i = 0; i <= loops; ++i) {
        JustReadTestFile(Parameters[0], buffer, bufferSize, true, true);
    }
    delete[] buffer;

    return 0;
}

static int GetFSCache(const CStringArray& Parameters)
{
    SIZE_T minlimit, maxlimit;
    ULONG flags;
    BOOL b  = GetSystemFileCacheSize(&minlimit, &maxlimit, &flags);
    if (!b) {
        LOG("GetSystemFileCacheSize failed, error %d", GetLastError());
    } else {
        LOG("Flags 0x%X, Min Limit %s %I64d (%d MB), Max Limit %s %I64d (%d MB)",
            flags,
            (flags & FILE_CACHE_MIN_HARD_ENABLE) ? "enabled" : "disabled",
            minlimit, (ULONG)(minlimit / (1024L * 1024L)),
            (flags & FILE_CACHE_MAX_HARD_ENABLE) ? "enabled" : "disabled",
            maxlimit, (ULONG)(maxlimit / (1024L * 1024L)));
    }
    return 0;
}

static int SetFSCache(const CStringArray& Parameters)
{
    CPrivilege priv;
    priv.Add(SE_INCREASE_QUOTA_NAME);
    SIZE_T minSize = atoi(Parameters[0]) * 1024L * 1024L;
    SIZE_T maxSize = (SIZE_T)(-1L);
    ULONG flags = 0;
    if (Parameters.GetCount() > 1) {
        maxSize = atoi(Parameters[1]) * 1024L * 1024L;
        flags = FILE_CACHE_MAX_HARD_ENABLE;
    } else {
        LOG("Disabling MAX limit");
        flags = FILE_CACHE_MAX_HARD_DISABLE;
    }
    flags |= minSize ? FILE_CACHE_MIN_HARD_ENABLE : FILE_CACHE_MIN_HARD_DISABLE;
    BOOL b = SetSystemFileCacheSize(minSize, maxSize, flags);
    if (!b) {
        LOG("SetSystemFileCacheSize failed, error %d", GetLastError());
        CStringArray names, states;
        priv.Get(names, states);
        for (UINT i = 0; i < names.GetCount(); ++i) {
            LOG(" %s %s", states[i].GetString(), names[i].GetString());
        }
    }
    return 0;
}

static int FlushFSCache(const CStringArray& Dammy)
{
    CPrivilege priv;
    priv.Add(SE_INCREASE_QUOTA_NAME);
    SIZE_T minSize = (SIZE_T)(-1L);
    SIZE_T maxSize = (SIZE_T)(-1L);
    ULONG flags = 0;
    BOOL b = SetSystemFileCacheSize(minSize, maxSize, flags);
    if (!b) {
        LOG("SetSystemFileCacheSize failed, error %d", GetLastError());
    }
    return 0;
}

static int InvalidateFSCache(const CStringArray& Parameters)
{
    CString s;
    s.Format("\\\\.\\%c:", (Parameters[0].GetString())[0]);
    CFile f;
    int res = 0;
    CFileException ex;
    if (!f.Open(s, CFile::modeReadWrite, &ex)) {
        ERR("Can't open, error %d", ex.m_lOsError);
    }
    return f.m_hFile != INVALID_HANDLE_VALUE;
}

class CFilesHandler : public CCommandHandler
{
public:
    CFilesHandler() : CCommandHandler("files", "some file operations", 1)
    {
        DeclareCommand("hold", _HoldFile, 2, 0);
        DeclareCommand("write", _WriteTestFile, 1, 0);
        DeclareCommand("verify", _VerifyTestFile, 1, 0);
        DeclareCommand("verify-api", _VerifyTestFileApi, 2, 0);
        DeclareCommand("verify-crt", _VerifyTestFileCrt, 2, 0);
        DeclareCommand("verify-uc", _VerifyTestFileApiUncached, 2, 0);
        DeclareCommand("read-uc", _JustReadUncached, 2, 0);
        DeclareCommand("cache-get", GetFSCache, 0, 0);
        DeclareCommand("cache-set", SetFSCache, 1, fMoreOK);
        DeclareCommand("cache-flush", FlushFSCache, 0, 0);
        DeclareCommand("cache-inv", InvalidateFSCache, 1, 0);
    }
private:
    void Help(CStringArray& a) override
    {
        a.Add("hold \t<filename> <r|w|rw>\tkeeps file open with r|w denial");
        a.Add("write \t<filename> -count:size>\twrite file for further verify, size in MBs");
        a.Add("verify\t<filename> -count:size>\tverify file, size in MBs (crt, 8 bytes)");
        a.Add("verify-api\t<filename> <blocksize> -count:size>\tverify file (WinApi), count=size in MBs");
        a.Add("verify-crt\t<filename> <blocksize> -count:size>\tverify file (crt), blocksize (bytes), count=size in MBs");
        a.Add("verify-uc\t<filename> <blocksize> -count:size>\tverify file (uncached), count=size in MBs");
        a.Add("read-uc\t<filename> <blocksize> -count:size>\tjust read the file (uncached), count=size in MBs");
        a.Add("cache-get");
        a.Add("cache-set <minlimit MB, 0 to disable> [maxlimit MB]");
        a.Add("cache-flush");
        a.Add("cache-inv <drive letter> - should invalidate the cache, but not sure");
    }
};

static CFilesHandler fh;
