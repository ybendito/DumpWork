#include "stdafx.h"
#include <NTSecAPI.h>

class CWaitableObject
{
public:
    CWaitableObject(HANDLE h) : m_Handle(h)
    {
    }
    ~CWaitableObject()
    {
        CloseHandle(m_Handle);
    }
    ULONG Wait(ULONG Millies = INFINITE)
    {
        return WaitForSingleObject(m_Handle, Millies);
    }
    virtual void Signal() = 0;
protected:
    HANDLE m_Handle;
};

class CEvent : public CWaitableObject
{
public:
    CEvent(bool Manual) : CWaitableObject(CreateEvent(NULL, Manual, false, NULL))
    {
    }
    void Set()
    {
        SetEvent(m_Handle);
    }
    void Signal() override
    {
        Set();
    }
};

class CSemaphore : public CWaitableObject
{
public:
    CSemaphore(LONG Initial, LONG Max) : CWaitableObject(CreateSemaphore(NULL, Initial, Max, NULL))
    {

    }
    void Signal() override
    {
        ReleaseSemaphore(m_Handle, 1, NULL);
    }
};

class CThreadOwner
{
public:
    CThreadOwner() { }
    ~CThreadOwner()
    {
        StopThread(true);
        while (IsThreadRunning()) {
            Sleep(10);
        }
    }
    bool IsThreadRunning() const { return m_ThreadHandle != NULL; }
    enum tThreadState { tsNotRunning, tsRunning, tsAborted, tsDeleting };
    bool StartThread()
    {
        if (IsThreadRunning()) {
            return false;
        }
        InterlockedCompareExchange(&m_State, tsRunning, tsNotRunning);
        m_ThreadHandle = (HANDLE)_beginthread(_ThreadProc, 0, this);
        if (IsThreadRunning()) {
            return true;
        }
        else {
            InterlockedCompareExchange(&m_State, tsNotRunning, tsRunning);
            return false;
        }
    }
    virtual tThreadState StopThread(bool bDeleting = false)
    {
        LONG val;
        if (bDeleting) {
            val = InterlockedCompareExchange(&m_State, tsDeleting, tsRunning);
        }
        else {
            val = InterlockedCompareExchange(&m_State, tsAborted, tsRunning);
        }
        return (tThreadState)val;
    }
private:
    LONG   m_State = tsNotRunning;
protected:
    tThreadState ThreadState() { return (tThreadState)m_State; }
    HANDLE m_ThreadHandle = NULL;
    virtual void ThreadProc() = 0;
    virtual void ThreadTerminated(tThreadState previous)
    {
        UNREFERENCED_PARAMETER(previous);
        m_ThreadHandle = NULL;
    }
    static void __cdecl _ThreadProc(PVOID param)
    {
        CThreadOwner* pOwner = (CThreadOwner*)param;
        pOwner->ThreadProc();
        LONG val = InterlockedExchange(&pOwner->m_State, tsNotRunning);
        pOwner->ThreadTerminated((tThreadState)val);
    }
};

class CLoadingThread : public CThreadOwner
{
public:
    CLoadingThread(CWaitableObject& WaitOn) : m_Obj(WaitOn)
    {

    }
    void SetLoad(ULONG Loops)
    {
        m_Loops = Loops;
    }
private:
    CWaitableObject& m_Obj;
    ULONG m_Loops = 0;
    void ThreadProc() override
    {
        BYTE hash[128] = {};
        BYTE data[sizeof(hash) * 8] = {};
        RtlGenRandom(data, sizeof(data));
        m_Obj.Wait();
        while (ThreadState() == tsRunning) {
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
    CThreadCollection(ULONG ThreadsCount, ULONG CpuUsage) :
        m_Semaphore(MAX_THREADS, MAX_THREADS)
    {
        for (UINT i = 0; i < ThreadsCount; ++i) {
            AddThread();
        }
    }
    ~CThreadCollection()
    {
        ChangeLoad(false, true);
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
    void ChangeLoad(bool Increment, bool BusyLoop = false)
    {
        if (BusyLoop && Increment) {
            m_Loops |= 0x80000000;
        } else if (BusyLoop && !Increment) {
            m_Loops &= ~0x80000000;
        } else if (Increment) {
            m_Loops++;
        } else if (m_Loops) {
            m_Loops--;
        }

        for (INT i = 0; i < m_Threads.GetCount(); ++i) {
            CLoadingThread* p = m_Threads[i];
            p->SetLoad(m_Loops);
        }
    }
    void AddThread()
    {
        CLoadingThread* p = new CLoadingThread(m_Semaphore);
        if (p) {
            m_Threads.Add(p);
            p->StartThread();
            p->SetLoad(m_Loops);
        }
        m_Semaphore.Signal();
    }
private:
    void ThreadProc() override
    {
        while (ThreadState() == tsRunning) {
            Sleep(500);
        }
    }
    CArray<CLoadingThread*>m_Threads;
    CSemaphore m_Semaphore;
    ULONG m_Loops = 1;
};

int CreateThreads(int argc, char** argv)
{
    if (argc < 2)
        return 1;
    ULONG numThreads = atoi(argv[0]);
    ULONG cpuUsage = atoi(argv[1]);

    CThreadCollection t(numThreads, cpuUsage);

    if (cpuUsage > 99) cpuUsage = 99;
    char line[256] = "";
    char* s = line;
    puts("Interactive mode:");
    puts("i(nc),d(ec),t(read add),B(usy loop on),b(usy loop off)");
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
                break;
            case 't':
                while (s[n++] == s[0]) t.AddThread();
                break;
            case 'b':
                t.ChangeLoad(false, true);
                break;
            case 'B':
                t.ChangeLoad(true, true);
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
