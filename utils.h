#pragma once

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
    void Clear()
    {
        ResetEvent(m_Handle);
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

class CMutex : public CWaitableObject
{
public:
    CMutex() : CWaitableObject(CreateMutex(NULL, false, NULL))
    {

    }
    void Signal() override
    {
        ReleaseMutex(m_Handle);
    }
};

#if !defined(ERR)

extern CMutex _logMutex;

#define ERR(fmt, ...) fprintf(stderr, "DumpWork:" ## fmt ## "\n", __VA_ARGS__)
//#define LOG(fmt, ...) fprintf(stdout, fmt ## "\n", __VA_ARGS__)
#define LOG(fmt, ...)                                                                                                   \
    {                                                                                                                   \
        _logMutex.Wait();                                                                                               \
        CStringA _s_;                                                                                                   \
        _s_.Format(fmt, __VA_ARGS__);                                                                                   \
        puts(_s_);                                                                                                      \
        _logMutex.Signal();                                                                                             \
    }
#endif

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
    bool ShouldContinueRunning() const {
        return ThreadState() == tsRunning;
    }
private:
    LONG   m_State = tsNotRunning;
protected:
    tThreadState ThreadState() const { return (tThreadState)m_State; }
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

class CPipe
{
public:
    CPipe(bool ForRead = true)
    {
        SECURITY_ATTRIBUTES a = {};
        a.nLength = sizeof(a);
        a.bInheritHandle = true;
        CreatePipe(&m_hRead, &m_hWrite, &a, 0);
        SetHandleInformation(ForRead ? m_hRead : m_hWrite, HANDLE_FLAG_INHERIT, 0);
    }
    ~CPipe()
    {
        if (m_hRead)
            CloseHandle(m_hRead);
        if (m_hWrite)
            CloseHandle(m_hWrite);
    }
    HANDLE ReadHandle()
    {
        return m_hRead;
    }
    HANDLE WriteHandle()
    {
        return m_hWrite;
    }
    void CloseWrite()
    {
        if (m_hWrite)
        {
            CloseHandle(m_hWrite);
            m_hWrite = NULL;
        }
    }
protected:
    HANDLE m_hRead = NULL;
    HANDLE m_hWrite = NULL;
};

class CProcessRunner
{
public:
    // WaitTime = 0 starts the process without waiting for termination
    // WaitTime < INFINITE gives a possibility to do some action when the
    //     process is running and decide when to kill it
    CProcessRunner(bool Redirect = false, ULONG WaitTime = INFINITE) :
        m_Redirect(Redirect),
        m_WaitTime(WaitTime)
    {
        Clean();
        if (!m_WaitTime)
        {
            m_Redirect = false;
        }
    }
    ~CProcessRunner()
    {
        // for m_WaitTime = 0
        Clean();
    }
    void SetHidden() { m_Hidden = true; }
    void SetIntermediateWait(ULONG Millies)
    {
        m_IntermediateWait = Millies;
    }
    void Terminate()
    {
        if (pi.hProcess)
        {
            TerminateProcess(pi.hProcess, 0);
        }
    }
    void RunProcess(CString& CommandLine)
    {
        Clean();
        si.cb = sizeof(si);
        if (m_Redirect)
        {
            si.hStdOutput = m_StdOut.WriteHandle();
            si.hStdError = m_StdErr.WriteHandle();
            si.hStdInput = m_StdIn.ReadHandle();
            si.dwFlags |= STARTF_USESTDHANDLES;
        }
        if (m_Hidden)
        {
            si.wShowWindow = SW_HIDE;
            si.dwFlags |= STARTF_USESHOWWINDOW;
        }
        LOG(" Running %s ...", CommandLine.GetString());
        if (CreateProcess(NULL, CommandLine.GetBuffer(), NULL, NULL, m_Redirect, CREATE_SUSPENDED, NULL, _T("."), &si, &pi))
        {
            if (m_Redirect)
            {
                m_StdOut.CloseWrite();
                m_StdErr.CloseWrite();
            }
            ResumeThread(pi.hThread);
            LOG(" Running %s succeded, pid %d(%X)", CommandLine.GetString(), pi.dwProcessId, pi.dwProcessId);
            if (!m_IntermediateWait || m_IntermediateWait > m_WaitTime) {
                m_IntermediateWait = m_WaitTime;
            }
            while (m_WaitTime && WaitForSingleObject(pi.hProcess, m_IntermediateWait) == WAIT_TIMEOUT)
            {
                m_CumulativeWait += m_IntermediateWait;
                if (ShouldTerminate(m_CumulativeWait, m_WaitTime)) {
                    Terminate();
                } else if (m_Redirect) {
                    LOG(" pid %d(%X) is running %d ms, collected Out %d, Err %d",
                        pi.dwProcessId, pi.dwProcessId, m_CumulativeWait,
                        StdOutResult().GetLength(), StdErrResult().GetLength());
                    Flush();
                }
            }
            Flush();
            ULONG exitCode;
            if (!GetExitCodeProcess(pi.hProcess, &exitCode))
            {
                exitCode = GetLastError();
            }
            PostProcess(exitCode);
        }
        else
        {
            LOG(" Running %s failed, error %d", CommandLine.GetString(), GetLastError());
        }
        if (m_WaitTime)
        {
            Clean();
        }
    }
    const CString& StdOutResult() const { return m_StdOutResult; }
    const CString& StdErrResult() const { return m_StdErrResult; }
protected:
    STARTUPINFO si = {};
    PROCESS_INFORMATION pi = {};
    CPipe m_StdOut;
    CPipe m_StdErr;
    CPipe m_StdIn;
    CString m_StdOutResult;
    CString m_StdErrResult;
    void Clean()
    {
        if (pi.hProcess) CloseHandle(pi.hProcess);
        if (pi.hThread) CloseHandle(pi.hThread);
        memset(&si, 0, sizeof(si));
        memset(&pi, 0, sizeof(pi));
    }
    ULONG m_WaitTime;
    ULONG m_IntermediateWait = 0;
    ULONG m_CumulativeWait = 0;
    bool  m_Redirect;
    bool  m_Hidden = false;
    virtual bool ShouldTerminate(ULONG CumulativeWait, ULONG WaitLimit)
    {
        return CumulativeWait >= WaitLimit;
    }
    virtual void PostProcess(ULONG ExitCode)
    {
        UNREFERENCED_PARAMETER(ExitCode);
    }
    CString Flush(HANDLE h)
    {
        CString s;
        ULONG err;
        BYTE buffer[128];
        do
        {
            ULONG avail = 0;
            ULONG got = 0;
            bool b = PeekNamedPipe(h, buffer, sizeof(buffer), &got, &avail, NULL);
            if (!b) {
                err = GetLastError();
                Sleep(10);
            }
            if (!b || !avail)
                break;
            got = 0;
            avail = min(avail, sizeof(buffer));
            if (!ReadFile(h, buffer, avail, &got, NULL))
            {
                err = GetLastError();
                Sleep(10);
            }
            if (!got)
                break;
            for (ULONG i = 0; i < got; ++i)
            {
                s += buffer[i];
            }
        } while (true);
        return s;
    }
    void Flush()
    {
        if (!m_Redirect)
            return;
        m_StdOutResult += Flush(m_StdOut.ReadHandle());
        m_StdErrResult += Flush(m_StdErr.ReadHandle());
    }
};

static CString DirectoryOf(LPCSTR FileName)
{
    CString s = FileName;
    int n = s.ReverseFind('\\');
    if (n >= 0) {
        s.SetAt(n, 0);
    }
    else {
        s = ".";
    }
    return s;
}

static CStringW StringAToW(const CStringA& s)
{
    CStringW ws;
    for (int i = 0; i < s.GetLength(); ++i)
        ws += s[i];
    return ws;
}

class CPickFolderDialog : public CFileDialog
{
public:
    CPickFolderDialog() : CFileDialog(true)
    {
        m_bPickFoldersMode = true;
    }
};

template<typename TFunctor> void Tokenize(CString Text, LPCSTR Delimiters, CStringArray& Result, TFunctor Accept)
{
    int n = 0;
    CString next;
    do {
        next = Text.Tokenize(Delimiters, n);
        LOG("Text %s (pos => %d)", next.GetString(), n);
        next.Trim();
        if (next.IsEmpty())
            break;
        if (!Accept(next)) {
            continue;
        }
        Result.Add(next);
    } while (true);
}

int FORCEINLINE CountLines(const CString& String)
{
    int n = 0, nFiles = 0;
    while (n >= 0) {
        CString next = String.Tokenize("\r\n", n);
        next.Trim();
        if (next.IsEmpty()) {
            continue;
        }
        nFiles++;
    }
    return nFiles;
}

class SystemInfo
{
public:
    SystemInfo() : m_Info(&m_information)
    {
        if (!m_information.dwPageSize)
        {
            GetSystemInfo(&m_information);
        }
    }
    const SYSTEM_INFO *m_Info;
private:
    static SYSTEM_INFO m_information;
};

class CMemoryMappedFile
{
public:
    CMemoryMappedFile(ULONG MegaBytes, bool MapAll = true, LPCSTR Name = NULL)
    {
        m_Size = MegaBytes * MB;
        m_Handle = CreateFileMapping(
            INVALID_HANDLE_VALUE,        // Pagefile-backed
            nullptr,                     // Default security
            PAGE_READWRITE,
            (DWORD)((m_Size >> 32) & 0xFFFFFFFF),
            (DWORD)(m_Size & 0xFFFFFFFF),
            Name
        );
        if (m_Handle) {
            if (MapAll) {
                m_Buffer = (PCHAR)MapAndForget(false);
            }
        }
        else {
            LOG("can't create file mapping of %d MB (%lld bytes), error %d",
                MegaBytes, m_Size, GetLastError());
        }
    }
    bool MapOneByOne(bool DoTouch)
    {
        ULONG val = 1;
        if (!m_Handle) {
            return false;
        }
        for (size_t offset = 0; offset < m_Size; offset += AllocAlignment()) {
            PCHAR p = (PCHAR)MapViewOfFile(m_Handle, FILE_MAP_WRITE, (ULONG)(offset >> 32), (ULONG)offset, AllocAlignment());
            if (!p) {
                LOG("can't map view of file at 0x%llX, error %d", offset, GetLastError());
                return false;
            }
            if (DoTouch) {
                p[0] = (char)val;
            }
            UnmapViewOfFile(p);
        }
        return true;
    }
    PVOID MapAndForget(bool DoTouch)
    {
        PVOID p = MapViewOfFile(m_Handle, FILE_MAP_WRITE, 0, 0, m_Size);
        if (!p) {
            LOG("can't map view of file, error %d", GetLastError());
        }
        if (DoTouch) {
            Touch((PCHAR)p);
        }
        return p;
    }
    bool IsValid()
    {
        return m_Handle && m_Buffer;
    }
    ~CMemoryMappedFile()
    {
        if (m_Buffer) {
            UnmapViewOfFile(m_Buffer);
        }
        if (m_Handle) {
            CloseHandle(m_Handle);
        }
    }
    int Poke()
    {
        return Touch(m_Buffer);
    }
private:
    SystemInfo m_SysInfo;
    HANDLE m_Handle = NULL;
    PCHAR  m_Buffer = NULL;
    ULONGLONG  m_Size = 0;
    const ULONGLONG MB = (1024L * 1024);
    int Touch(PCHAR Buffer)
    {
        ULONG val;
        int n = 0;
        RtlGenRandom(&val, sizeof(val));
        ULONG location = val % PageSize();
        for (size_t offset = 0; offset < m_Size && Buffer; offset += PageSize()) {
            Buffer[offset + location] = (char)(val >> 24);
            ++n;
        }
        return n;
    }
    ULONG PageSize()
    {
        return m_SysInfo.m_Info->dwPageSize;
    }
    ULONG AllocAlignment()
    {
        return m_SysInfo.m_Info->dwAllocationGranularity;
    }
};

class CProcessToken
{
public:
    CProcessToken()
    {
        OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &m_Token);
    }
    ~CProcessToken()
    {
        if (m_Token) {
            CloseHandle(m_Token);
        }
    }
    bool Valid() const { return m_Token; }
    operator HANDLE() const { return m_Token; }
    bool GetPrivileges(CStringArray& Names, CStringArray& States)
    {
        if (!Valid()) {
            return false;
        }
        struct
        {
            TOKEN_PRIVILEGES header;
            LUID_AND_ATTRIBUTES attr[256];
        } Info;
        ULONG len;
        if (!GetTokenInformation(m_Token, TokenPrivileges, &Info, sizeof(Info), &len)) {
            return false;
        }
        for (UINT i = 0; i < Info.header.PrivilegeCount; ++i) {
            LUID_AND_ATTRIBUTES& data = Info.header.Privileges[i];
            char name[MAX_PATH];
            ULONG len = ARRAYSIZE(name);
            if (!LookupPrivilegeName(NULL, &data.Luid, name, &len)) {

            } else {
                Names.Add(name);
                States.Add(StringFromAttribute(data.Attributes));
            }
        }
        return true;
    }
    bool AddPrivilege(LPCSTR Name)
    {
        if (!Valid()) {
            return false;
        }
        TOKEN_PRIVILEGES priv;
        if (!LookupPrivilegeValue(NULL, Name, &priv.Privileges->Luid)) {
            return false;
        }
        priv.PrivilegeCount = 1;
        priv.Privileges->Attributes = SE_PRIVILEGE_ENABLED;
        return AdjustTokenPrivileges(m_Token, false, &priv, sizeof(priv), NULL, 0);
    }
private:
    static CString StringFromAttribute(ULONG Attribute)
    {
        return (Attribute & SE_PRIVILEGE_ENABLED) ? "Enabled" : "Disabled";
    }
    HANDLE m_Token = NULL;
};

class CPrivilege
{
public:
    bool Get(CStringArray& Names, CStringArray& States)
    {
        return m_Token.GetPrivileges(Names, States);
    }
    bool Add(LPCSTR PrivilegeName)
    {
        return m_Token.AddPrivilege(PrivilegeName);
    }
protected:
    CProcessToken m_Token;
};

ULONG FORCEINLINE GetNumber(LPCSTR String)
{
    char* end = NULL;
    ULONG val = strtoul(String, &end, 10);
    if (val) {
        LONG scale = 1;
        if (end && *end) switch (*end)
        {
            case 'm': case 'M': scale = 1024 * 1024; break;
            case 'k': case 'K': scale = 1024; break;
            default: break;
        }
        val *= scale;
    }
    return val;
}
