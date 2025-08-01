#include "stdafx.h"

static int MemInfo(const CStringArray& Parameters)
{
    ULONGLONG ramSizeInKb;
    BOOL b = GetPhysicallyInstalledSystemMemory(&ramSizeInKb);
    if (!b) {
        LOG("GetPhysicallyInstalledSystemMemory failed, error %d", GetLastError());
    } else {
        ULONG inGB = (ULONG)(ramSizeInKb / (1024L * 1024L));
        LOG("memory size %d GB", inGB);
    }
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    LOG("page size %d, alloc granularity %d", si.dwPageSize, si.dwAllocationGranularity);
    return 0;
}

static void PteIteration(ULONG size /*MB*/, ULONG SleepTimeMs)
{
    ULONG pageSize = 0x1000;

    CMemoryMappedFile f(size);
    f.Poke();
    if (SleepTimeMs) {
        Sleep(SleepTimeMs);
    }
}

static int PteStress(const CStringArray& Parameters)
{
    UINT sizeInMB = atoi(Parameters[0]);
    UINT count = atoi(Parameters[1]);
    UINT sleep = atoi(Parameters[2]);
    if (count == 0) count -= 1;
    while (count) {
        --count;
        PteIteration(sizeInMB, sleep);
    }
    return 0;
}

static int PteToFail(const CStringArray& Parameters)
{
    UINT sizeInMB = atoi(Parameters[0]);
    CMemoryMappedFile f(sizeInMB);
    if (!f.IsValid()) {
        return 0;
    }
    while (f.MapAndForget(true)) {}
    return 0;
}

static int PteSmallToFail(const CStringArray& Parameters)
{
    UINT sizeInMB = atoi(Parameters[0]);
    UINT loops = atoi(Parameters[1]);
    CMemoryMappedFile f(sizeInMB, false);
    int n = 0;
    while (f.MapOneByOne(true) && loops--) { ++n; }
    LOG("done %d loops", n);
    return 0;
}

class CMemInfo : public CCommandHandler
{
public:
    CMemInfo() : CCommandHandler("memory", "memory info", 1)
    {
        DeclareCommand("info", MemInfo, 0, 0);
        DeclareCommand("pte", PteStress, 3, 0);
        DeclareCommand("pte2", PteToFail, 1, 0);
        DeclareCommand("pte3", PteSmallToFail, 2, 0);
    }
private:
    void Help(CStringArray& a) override
    {
        a.Add("info                      \tGet physical info size");
        a.Add("pte  <MB> <iter> <sleep ms>\tTest PTE");
        a.Add("pte2 <MB>                  \tTest PTE leak until fail");
        a.Add("pte3 <MB> <loops>          \tTest small PTE");
    }
};

static CMemInfo vf;
