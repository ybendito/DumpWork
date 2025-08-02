#include "stdafx.h"

#define MEM_SIZE_MB     1024

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
    CLoadingThreadMemory(CWaitableObject& WaitOn) : CLoadingThread(WaitOn)
    {

    }
private:
    void ThreadProc() override
    {
        __super::ThreadProc();
        while (ShouldContinueRunning()) {
            CMemoryMappedFile f(MEM_SIZE_MB);
            auto n = f.Poke();
            //LOG("+%d", n);
            gThreadPokes += n;
        }
    }
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

class CThreadCollection : public CThreadOwner
{
    const ULONG MAX_THREADS = 4096;
public:
    CThreadCollection() : m_Semaphore(0, MAX_THREADS)
    {

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
    ULONG Load() const { return m_Loops; }
    void AddThread(bool MemoryStress)
    {
        CLoadingThread* p;
        if (MemoryStress) {
            p = new CLoadingThreadMemory(m_Semaphore);
        } else {
            p = new CLoadingThreadCpu(m_Semaphore, m_Loops);
        }
        if (p) {
            m_Threads.Add(p);
            p->StartThread();
            m_Semaphore.Signal();
        }
    }
private:
    void ThreadProc() override
    {
        while (ShouldContinueRunning()) {
            Sleep(500);
        }
    }
    CArray<CLoadingThread*>m_Threads;
    CSemaphore m_Semaphore;
    ULONG m_Loops = 1;
};

static int CreateThreads(const char *Activity, const char *Param)
{
    ULONG numThreads = atoi(Param);
    bool bCpu = !_stricmp(Activity, "cpu");
    bool bMem = !_stricmp(Activity, "mem");
    if (!bCpu && !bMem) {
        LOG("Unknown type of activity");
        return 1;
    } else if (bMem) {
        LOG("Using %d MB per thread", MEM_SIZE_MB);
    }

    CThreadCollection t;

    for (ULONG i = 0; i < numThreads; ++i) {
        t.AddThread(bMem);
    }

    char line[256] = "";
    char* s = line;
    if (!bMem) {
        puts("Interactive mode: q to exit");
        puts("i(nc),d(ec),t(read add),b(usy loop toggle)");
    } else {
        puts("Interactive mode: 'q' to exit, 't' to add thread");
    }
    do
    {
        s = fgets(line, sizeof(line), stdin);
        if (!s) break;
        int n = 0;
        switch (s[0])
        {
            case 'i':
            case 'd':
                while (s[n++] == s[0]) t.ChangeLoad(s[0] == 'i');
                LOG("current loop limit 0x%X", t.Load());
                break;
            case 't':
                while (s[n++] == s[0]) t.AddThread(bMem);
                break;
            case 'b':
                t.ToggleBusyLoop();
                LOG("current loop limit 0x%X", t.Load());
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
    
    return 0;
}

class CThreadsHandler : public CCommandHandler
{
public:
    CThreadsHandler() : CCommandHandler("load", "Load CPU by threads", 2) {}
private:
    int Run(const CStringArray& Parameters) override
    {
        return CreateThreads(Parameters[0], Parameters[1]);
    }
    void Help(CStringArray& a) override
    {
        a.Add("cpu <number of threads>");
        a.Add("mem <number of threads>");
    }
};

static CThreadsHandler th;
