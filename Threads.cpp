#include "stdafx.h"

thread_local LONGLONG gThreadPokes = 0;

class CLoadingThread : public CThreadOwner
{
public:
    CLoadingThread(CWaitableObject& WaitOn) :
        m_Obj(WaitOn)
    {

    }
protected:
    void ThreadProc() override
    {
        m_Obj.Wait();
    }
private:
    CWaitableObject& m_Obj;
    void ThreadTerminated(tThreadState previous) override
    {
        //LOG("Thread finishes (%lld)", gThreadPokes);
        m_Obj.Signal();
        __super::ThreadTerminated(previous);
    }
};

class CLoadingThreadMemory : public CLoadingThread
{
public:
    CLoadingThreadMemory(CWaitableObject& WaitOn, LPCSTR Name) :
        CLoadingThread(WaitOn),
        m_Name(Name)
    {

    }
private:
    void ThreadProc() override
    {
        ULONG sizeMB = Config().Count;
        if (!sizeMB) sizeMB = 1;

        __super::ThreadProc();

        while (ShouldContinueRunning()) {
            if (1) {
                CMemoryMappedFile f(sizeMB, false, m_Name);
                auto n = f.Poke();
                //LOG("+%d", n);
                gThreadPokes += n;
            }
            Sleep(0);
        }
    }
    CString m_Name;
};

class CLoadingThreadCpu : public CLoadingThread
{
public:
    CLoadingThreadCpu(CWaitableObject& WaitOn, ULONG& Loops) :
        CLoadingThread(WaitOn),
        m_Loops(Loops)
    {

    }
private:
    ULONG& m_Loops;
    void ThreadProc() override
    {
        __super::ThreadProc();
        BYTE hash[128] = {};
        BYTE data[sizeof(hash) * 8] = {};
        RtlGenRandom(data, sizeof(data));
        while (ShouldContinueRunning()) {
            for (ULONG i = 0; i < m_Loops; ++i) {
                HashData(data, sizeof(data), hash, sizeof(hash));
            }
            Sleep(1);
        }
    }
};

class CFileWriteThread : public CLoadingThread
{
public:
    CFileWriteThread(CWaitableObject& WaitOn) :
        CLoadingThread(WaitOn)
    {

    }
private:
    void ThreadProc() override
    {
        __super::ThreadProc();
        ULONG data[1];
        ULONG units = 0x10000;
        RtlGenRandom(data, sizeof(data));
        CFile f;
        if (CreateWorkingFile(f, units, data[0])) {
            UINT nDone = 0;
            while (ShouldContinueRunning()) {
                USHORT pos;
                RtlGenRandom(&pos, sizeof(pos));
                ULONG offset = sizeof(data[0]) * pos;
                f.Seek(offset, CFile::begin);
                ULONG correctVal = data[0] + pos;
                f.Write(&correctVal, sizeof(correctVal));
                f.Flush();
                nDone++;
                if (nDone > units) {
                    nDone = 0;
                    Verify(f, units, data[0]);
                }
            }
        }
    }
    static bool CreateWorkingFile(CFile& File, ULONG NumberOfUnits, ULONG startValue)
    {
        char name[MAX_PATH];
        if (!GetTempFileName(".", "tmp", 0, name)) {
            return false;
        }
        if (!File.Open(name, CFile::modeReadWrite | CFile::shareDenyNone | CFile::modeCreate)) {
            return false;
        }
        //LOG("File %s, starts from %08X", name, startValue);
        for (ULONG i = 0; i < NumberOfUnits; ++i) {
            ULONG val = startValue + i;
            File.Write(&val, sizeof(val));
        }
        return true;
    }
    static void Verify(CFile& File, ULONG NumberOfUnits, ULONG startValue)
    {
        File.SeekToBegin();
        for (ULONG i = 0; i < NumberOfUnits; ++i) {
            ULONG correctVal = startValue + i;
            ULONG val;
            File.Read(&val, sizeof(val));
            if (val != correctVal) {
                LOG("error detected at %d", i);
            }
        }
    }
};

class CLoadingThreadRunProcess : public CLoadingThread
{
public:
    CLoadingThreadRunProcess(CWaitableObject& WaitOn, const char *CommandLine) :
        CLoadingThread(WaitOn), m_Command(CommandLine)
    {

    }
private:
    CString m_Command;
    void ThreadProc() override
    {
        __super::ThreadProc();
        CProcessRunner runner(false, 0);
        runner.RunProcess(m_Command);
        while (ShouldContinueRunning()) Sleep(1000);
        runner.Terminate();
    }
};

class CFileReadThread : public CLoadingThread
{
public:
    CFileReadThread(CWaitableObject& WaitOn, ULONG& WaitMillies) :
        CLoadingThread(WaitOn), m_Wait(WaitMillies)
    {
    }
    ~CFileReadThread()
    {
        if (m_Buffer) {
            _aligned_free(m_Buffer);
        }
    }
private:
    void ThreadProc() override
    {
        __super::ThreadProc();
        ULONG data[1];
        ULONG units = 0x10000;
        RtlGenRandom(data, sizeof(data));

        m_Buffer = (PULONG)_aligned_malloc(SectorSize, SectorSize);

        if (!m_Buffer) {
            return;
        }

        char name[MAX_PATH];
        GetTempFileName(".", "tmp", 0, name);

        if (!FillWorkingFile(name, units, data[0])) {
            return;
        }

        while (ShouldContinueRunning() && Verify(name, units, data[0])) {
            ;;
        }
    }
    static bool FillWorkingFile(LPCTSTR Name, ULONG NumberOfUnits, ULONG startValue)
    {
        CFile f;
        if (!f.Open(Name, CFile::modeReadWrite | CFile::shareDenyNone | CFile::modeCreate)) {
            LOG("can't open %s", Name);
            return false;
        }
        //LOG("File %s, starts from %08X", Name, startValue);
        for (ULONG i = 0; i < NumberOfUnits; ++i) {
            ULONG val = startValue + i;
            f.Write(&val, sizeof(val));
        }
        return true;
    }
    bool Verify(LPCTSTR Name, ULONG NumberOfUnits, ULONG startValue)
    {
        CFile f;
        if (!f.Open(Name, CFile::modeRead | CFile::shareDenyNone | CFile::osNoBuffer)) {
            LOG("can't open %s", Name);
            return false;
        }
        f.SeekToBegin();
        UINT nRounds = (NumberOfUnits * sizeof(ULONG)) / SectorSize;
        for (ULONG i = 0; i < nRounds; ++i) {
            SingleSleep();
            f.Read(m_Buffer, SectorSize);
            for (ULONG j = 0; j < SectorSize / sizeof(ULONG); ++j) {
                ULONG correctVal = (i * SectorSize) / sizeof(ULONG) + j + startValue;
                if (m_Buffer[j] != correctVal) {
                    LOG("error detected at %d", i);
                    return false;
                }
            }
        }
        return true;
    }
    void SingleSleep()
    {
        ULONG ms = m_Wait & ~0x80000000;
        if (ms) {
            Sleep(ms);
        }
    }
    PULONG m_Buffer = NULL;
    const UINT SectorSize = 4096;
    ULONG& m_Wait;
};

class CThreadCollection
{
    const ULONG MAX_THREADS = 4096;
public:
    CThreadCollection(const CStringArray& Parameters) :
        m_Semaphore(0, MAX_THREADS), m_Parameters(Parameters)
    {
        m_Activity = Parameters[0];
        if (m_Activity.Find("ni_") == 0) {
            m_Activity.Delete(0, 3);
            m_Interactive = false;
        }
        ULONG numThreads = atoi(Parameters[1]);
        for (ULONG i = 0; i < numThreads; ++i) {
            AddThread();
        }
    }
    ~CThreadCollection()
    {
        m_Loops = 1;

        for (INT i = 0; i < m_Threads.GetCount(); ++i) {
            CLoadingThread* p = m_Threads[i];
            p->StopThread();
        }
        for (INT i = 0; i < m_Threads.GetCount(); ++i) {
            m_Semaphore.Wait();
        }
        //LOG("All threads finished");
        for (INT i = 0; i < m_Threads.GetCount(); ++i) {
            CLoadingThread* p = m_Threads[i];
            delete p;
        }
    }
    void ProcessInput()
    {
        UINT timeCounter = 0;
        while (!m_Interactive) {
            Sleep(1000);
            if (Config().Time) {
                timeCounter++;
                if (timeCounter >= Config().Time) {
                    return;
                }
            }
        }

        puts("Interactive mode: q to exit");
        puts("i(nc),d(ec),t(read add),b(usy loop toggle)");

        char line[256] = "";
        char* s = line;
        do
        {
            s = fgets(line, sizeof(line), stdin);
            if (!s) break;
            int n = 0;
            switch (s[0])
            {
                case 'i':
                case 'd':
                    while (s[n++] == s[0]) ChangeLoad(s[0] == 'i');
                    LOG("current loop limit 0x%X", m_Loops);
                    break;
                case 't':
                    while (s[n++] == s[0]) AddThread();
                    break;
                case 'b':
                    ToggleBusyLoop();
                    LOG("current loop limit 0x%X", m_Loops);
                    break;
                case 'q':
                    n = -1;
                    break;
                default:
                    break;
            }
            if (n < 0)
                break;
        } while (s);
    }
private:
    void ToggleBusyLoop()
    {
        m_Loops ^= 0x80000000;
    }
    void ChangeLoad(bool Increment)
    {
        if (Increment) {
            m_Loops++;
        } else if (m_Loops) {
            m_Loops--;
        }
    }
    void AddThread()
    {
        const char* cmdLine = m_Parameters.GetCount() > 2 ? m_Parameters[2] : NULL;
        CLoadingThread* p = NULL;
        if (!m_Activity.CompareNoCase("mem")) {
            p = new CLoadingThreadMemory(m_Semaphore, cmdLine);
        } else if (!m_Activity.CompareNoCase("run")) {
            p = new CLoadingThreadRunProcess(m_Semaphore, cmdLine);
        } else if (!m_Activity.CompareNoCase("cpu")) {
            p = new CLoadingThreadCpu(m_Semaphore, m_Loops);
        } else if (!m_Activity.CompareNoCase("wfile")) {
            p = new CFileWriteThread(m_Semaphore);
        } else if (!m_Activity.CompareNoCase("rfile")) {
            // m_Loops here is wait time in millies, without changing resolution
            p = new CFileReadThread(m_Semaphore, m_Loops);
        } else {
            LOG("Unknown type of activity %s", m_Activity.GetString());
        }
        if (p) {
            m_Threads.Add(p);
            p->StartThread();
            m_Semaphore.Signal();
        }
    }
    CArray<CLoadingThread*>m_Threads;
    CSemaphore m_Semaphore;
    ULONG m_Loops = 1;
    const CStringArray& m_Parameters;
    CString m_Activity;
    bool m_Interactive = true;
};

static int CreateThreads(const CStringArray& Parameters)
{
    UINT maxLoops = Config().Loops;
    if (!maxLoops) maxLoops = 1;
    for (UINT loopCount = 0; loopCount < maxLoops; ++loopCount) {
        // scope
        {
            CThreadCollection t(Parameters);
            t.ProcessInput();
        }
        Sleep(5000);
    }
    return 0;
}

class CThreadsHandler : public CCommandHandler
{
public:
    CThreadsHandler() : CCommandHandler("threads", "Run multiple threads for various activities", 2) {}
private:
    int Run(const CStringArray& Parameters) override
    {
        return CreateThreads(Parameters);
    }
    void Help(CStringArray& a) override
    {
        a.Add("cpu   <number of threads>");
        a.Add("mem   <number of threads> <section-name> [-Count:sizeInMB(default=1)]");
        a.Add("wfile <number of threads>");
        a.Add("rfile <number of threads>");
        a.Add("run   <number of threads> <cmd line>");
        a.Add("ni_* - force non-interactive mode");
    }
};

static CThreadsHandler th;
