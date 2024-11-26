#include "stdafx.h"
#include <NTSecAPI.h>

class CLoadingThread : public CThreadOwner
{
public:
    CLoadingThread(CWaitableObject& WaitOn, ULONG& Loops) :
        m_Obj(WaitOn),
        m_Loops(Loops)
    {

    }
private:
    CWaitableObject& m_Obj;
    ULONG& m_Loops;
    void ThreadProc() override
    {
        BYTE hash[128] = {};
        BYTE data[sizeof(hash) * 8] = {};
        RtlGenRandom(data, sizeof(data));
        m_Obj.Wait();
        while (ShouldContinueRunning()) {
            for (ULONG i = 0; i < m_Loops; ++i) {
                HashData(data, sizeof(data), hash, sizeof(hash));
            }
            Sleep(1);
        }
    }
    void ThreadTerminated(tThreadState previous) override
    {
        m_Obj.Signal();
        __super::ThreadTerminated(previous);
    }
};

class CThreadCollection : public CThreadOwner
{
    const ULONG MAX_THREADS = 4096;
public:
    CThreadCollection(ULONG ThreadsCount) :
        m_Semaphore(MAX_THREADS, MAX_THREADS)
    {
        for (UINT i = 0; i < ThreadsCount; ++i) {
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
    void AddThread()
    {
        CLoadingThread* p = new CLoadingThread(m_Semaphore, m_Loops);
        if (p) {
            m_Threads.Add(p);
            p->StartThread();
        }
        m_Semaphore.Signal();
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

static int CreateThreads(const char *Param)
{
    ULONG numThreads = atoi(Param);

    CThreadCollection t(numThreads);

    char line[256] = "";
    char* s = line;
    puts("Interactive mode:");
    puts("i(nc),d(ec),t(read add),b(usy loop toggle)");
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
                while (s[n++] == s[0]) t.AddThread();
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
    CThreadsHandler() : CCommandHandler("load", "Load CPU by threads", 1) {}
private:
    int Run(const CStringArray& Parameters) override
    {
        return CreateThreads(Parameters[0]);
    }
    void Help(CStringArray& a) override
    {
        a.Add("<number of threads>");
    }
};

static CThreadsHandler th;
