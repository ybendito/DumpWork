// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

#pragma comment(lib, "dbgeng.lib")

WINDBG_EXTENSION_APIS ExtensionApis;
bool bVerbose;

extern "C" __declspec(dllexport) HRESULT verbose(IN PDEBUG_CLIENT Client, IN PCSTR Args)
{
    CComPtr<IDebugClient> client = Client;
    CComPtr<IDebugControl> control;
    auto usage = [&]() { control->Output(DEBUG_OUTPUT_NORMAL, "verbose <on|off>\n"); };
    if (S_OK != client.QueryInterface<IDebugControl>(&control)) {
        return S_FALSE;
    }
    if (!CString("on").Compare(Args)) {
        bVerbose = true;
    }
    else if (!CString("off").Compare(Args)) {
        bVerbose = false;
    }
    else {
        control->Output(DEBUG_OUTPUT_NORMAL, "verbose <on|off>\n");
    }
    if (CString("--help").Compare(Args)) {
        control->Output(DEBUG_OUTPUT_NORMAL, "verbose is %s\n", bVerbose ? "on" : "off");
    }
    return S_OK;
}

class CUnknown : public IUnknown
{
public:
    virtual ~CUnknown() {}
protected:
    STDMETHOD_(ULONG, AddRef)(
        THIS
        ) override
    {
        VERBOSE("%s: => %d", __FUNCTION__, m_RefCount);
        return ++m_RefCount;
    }
    STDMETHOD_(ULONG, Release)(
        THIS
        ) override
    {
        VERBOSE("%s: => %d", __FUNCTION__, m_RefCount);
        return --m_RefCount;
    }
    STDMETHOD(QueryInterface)(
        THIS_
        __in REFIID InterfaceId,
        __out PVOID* Interface
        ) override
    {
        m_QueryData.InterfaceId = InterfaceId;
        m_QueryData.Interface = Interface;
        ProcessQuery(__uuidof(IUnknown), (IUnknown*)this);
        return m_QueryData.Result;
    }
    bool ProcessQuery(__in REFIID Supported, PVOID Object)
    {
        *m_QueryData.Interface = NULL;
        if (IsEqualIID(m_QueryData.InterfaceId, Supported)) {
            *m_QueryData.Interface = Object;
            AddRef();
        }
        m_QueryData.Result = (*m_QueryData.Interface) ? S_OK : E_NOINTERFACE;
        return m_QueryData.Result == S_OK;
    }
protected:
    ULONG m_RefCount = 0;
private:
    struct
    {
        HRESULT Result;
        GUID    InterfaceId;
        PVOID*  Interface;
    } m_QueryData;
};

class CDebugOutputCallback :
    public virtual CUnknown,
    public IDebugOutputCallbacks
{
public:
    CDebugOutputCallback(PDEBUG_CLIENT Client) : m_Client(Client)
    {
        Client->GetOutputCallbacks(&m_Previous);
        Client->SetOutputCallbacks(this);
    }
    ~CDebugOutputCallback()
    {
        m_Client->SetOutputCallbacks(m_Previous);
    }
protected:
    STDMETHOD(QueryInterface)(
        THIS_
        __in REFIID InterfaceId,
        __out PVOID* Interface
        ) override
    {
        if (CUnknown::QueryInterface(InterfaceId, Interface) == S_OK ||
            ProcessQuery(__uuidof(IDebugOutputCallbacks), (IDebugOutputCallbacks*)this)) {
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHOD(Output)(
        THIS_
        _In_ ULONG Mask,
        _In_ PCSTR Text
        ) override
    {
        CString s = Text;
        s.Trim();
        if (!s.IsEmpty()) {
            VERBOSE("got#%02d %s", (int)m_Output.GetCount(), Text);
            m_Output.Add(s);
        }
        return S_OK;
    }
protected:
    CStringArray m_Output;
    PDEBUG_CLIENT m_Client;
    void CombineOutput(CString& Combined)
    {
        for (int i = 0; i < m_Output.GetCount(); ++i) {
            Combined += m_Output[i];
        }
    }
private:
    PDEBUG_OUTPUT_CALLBACKS m_Previous = NULL;
    STDMETHOD_(ULONG, AddRef)(THIS) override {
        return CUnknown::AddRef();
    }
    STDMETHOD_(ULONG, Release)(THIS) override {
        return CUnknown::Release();
    }
};

class CExternalCommandParser : public CDebugOutputCallback
{
public:
    CExternalCommandParser(PDEBUG_CLIENT Client, PDEBUG_CONTROL Control, LPCSTR Command) :
        m_Command(Command),
        m_Control(Control),
        CDebugOutputCallback(Client)
    {
        // at this point the output callback is set, previous one is saved
    }
    virtual HRESULT Run()
    {
        HRESULT res = m_Control->Execute(DEBUG_OUTPUT_NORMAL, m_Command, DEBUG_EXECUTE_NO_REPEAT);
        return res;
    }
protected:
    CString m_Command;
    PDEBUG_CONTROL m_Control;
};

class CTestCommand : CExternalCommandParser
{
public:
    CTestCommand(PDEBUG_CLIENT Client, PDEBUG_CONTROL Control, LPCSTR Command) :
        CExternalCommandParser(Client, Control, Command)
    {}
    HRESULT Run() override
    {
        bool saveVerbose = bVerbose;
        bVerbose = true;
        HRESULT res = __super::Run();
        bVerbose = saveVerbose;
        return res;
    }
};

class CGetVirtioAdapters : public CExternalCommandParser
{
public:
    CGetVirtioAdapters(PDEBUG_CLIENT Client, PDEBUG_CONTROL Control) :
        CExternalCommandParser(Client, Control, "!ndiskd.miniports") {}
    void Process(CPtrArray& Adapters)
    {
        //example:
        //@3: whatever
        //@4: ffffb904326d81a0
        //@5: Red Hat VirtIO Ethernet Adapter
        for (int n = 6; n <= m_Output.GetCount(); n += 3) {
            if (m_Output[n - 1].Find("Red Hat VirtIO") >= 0) {
                char* endptr;
                void *p = (void *)strtoull(m_Output[n - 2], &endptr, 16);
                if (p) {
                    Adapters.Add(p);
                }
            }
        }
    }
};

class CGetAdapterContext : public CExternalCommandParser
{
public:
    CGetAdapterContext(PDEBUG_CLIENT Client, PDEBUG_CONTROL Control, ULONGLONG Adapter) :
        CExternalCommandParser(Client, Control, MakeCommand(Adapter)) {}
    void Process(ULONGLONG& Context)
    {
// example
// Adapter context
// ffffb904303f3000
        bool bFound = false;
        for (int i = 0; i < m_Output.GetCount(); ++i) {
            if (0 == m_Output[i].Compare("Adapter context")) {
                bFound = true;
            } else if (bFound) {
                char* endptr;
                Context = strtoull(m_Output[i], &endptr, 16);
                break;
            }
        }
    }
    void Process(CString& Version)
    {
        // example
        // Driver
        // ...
        // v101.14400
        int Found = 0;
        for (int i = 0; i < m_Output.GetCount(); ++i) {
            switch (Found) {
                case 0:
                    Found = m_Output[i].Compare("Driver") == 0;
                    break;
                case 1:
                    Found = 2;
                    break;
                case 2:
                    Found = -1;
                    Version = m_Output[i];
                    break;
                default:
                    return;
            }
        }
    }
    static CString MakeCommand(ULONGLONG Adapter)
    {
        CString s;
        s.Format("!ndiskd.miniport %p", (PVOID)Adapter);
        return s;
    }
};

class CCheckPdb : public CExternalCommandParser
{
public:
    CCheckPdb(PDEBUG_CLIENT Client, PDEBUG_CONTROL Control, LPCSTR ModuleName, LPCSTR PdbName) :
        CExternalCommandParser(Client, Control, MakeCommand(ModuleName, PdbName))
    {}
    bool Parse()
    {
        bool match = false;
        CString s;
        CombineOutput(s);
        int matchPos = s.Find("MATCH:");
        int mismatchPos = s.Find("MISMATCH:");
        return matchPos >= 0 && mismatchPos < 0;
    }
private:
    CString MakeCommand(LPCSTR ModuleName, LPCSTR PdbName)
    {
        CString s;
        s.Format("!chksym %s.sys %s", ModuleName, PdbName);
        return s;
    }
};

class CGetLmiData : CExternalCommandParser
{
public:
    CGetLmiData(PDEBUG_CLIENT Client, PDEBUG_CONTROL Control, LPCSTR Name) :
        CExternalCommandParser(Client, Control, MakeCommand(Name))
    {
        VERBOSE("%s: %s", __FUNCTION__, m_Command.GetString());
    }
    HRESULT Run() override
    {
        bool saveVerbose = bVerbose;
        bVerbose = true;
        HRESULT res = __super::Run();
        bVerbose = saveVerbose;
        return res;
    }
    bool Process(GUID& Guid)
    {
        // ...
        // GUID: {C6F32534-15A4-415F-ACE1-5F7BFC1BD569}
        // ...
        int found = 0;
        for (int i = 0; !found && i < m_Output.GetCount(); ++i) {
            switch (found) {
            case 0:
                found = m_Output[i].Find("GUID: ");
                if (found >= 0) {
                    CString s = m_Output[i];
                    //LOG("1: string of %d", s.GetLength());
                    s.Delete(0, found + 6);
                    //LOG("2: string of %d", s.GetLength());
                    found = s.Find('}');
                    if (found >= 0) {
                        s.Delete(found + 1, s.GetLength() - found - 1);
                        //LOG("Breaking from processing, found=%d, last string of %d", found, s.GetLength());
                        //return false;
                        CStringW ws = StringAToW(s);
                        return IIDFromString(ws, &Guid) == S_OK;
                    }
                    return false;
                }
                found = 0;
                break;
            default:
                return false;
            }
        }
        return false;
    }
private:
    static CString MakeCommand(LPCSTR Name)
    {
        CString s;
        s.Format("!lmi %s.sys", Name);
        return s;
    }
};

class CDebugExtension
{
public:
    virtual ~CDebugExtension()
    {
        LOG("%s", __FUNCTION__);
    }
    void TestCommand(LPCSTR Command)
    {
        CTestCommand cmd(m_Client, m_Control, Command);
        cmd.Run();
    }

    void findpdb(LPCSTR TopDir)
    {
        CStringArray dirs;
        dirs.Add(TopDir);
        CStringArray found;

        FindPdbInDirectories(m_Module, found, dirs);

        if (found.GetCount() == 0) {
            Output("No compatible PDB found\n");
            return;
        }
        for (int i = 0; i < found.GetCount(); ++i) {
            Output("  %s\n", found[i].GetString());
        }
        AppendSymbolPath(found);
    }

protected:
    CDebugExtension(PDEBUG_CLIENT Client, LPCSTR Module)
    {
        LOG("%s %s", __FUNCTION__, Module);
        m_Module = Module;
        m_Client = Client;
        m_Result = m_Client.QueryInterface<IDebugControl>(&m_Control);
        if (m_Result != S_OK) {
            ERR("%s: Can't obtain IDebugControl", __FUNCTION__);
            throw;
        }
        m_Result = m_Client.QueryInterface<IDebugSymbols>(&m_Symbols);
        if (m_Result != S_OK) {
            ERR("%s: Can't obtain IDebugSymbols", __FUNCTION__);
            throw;
        }
        // optional, do not fail if not present
        m_Result = m_Client.QueryInterface<IDebugSymbols3>(&m_Symbols3);
    }
    void help()
    {
        Output("findpdb <directory> - recursive find and append to symbol path\n");
        verbose(m_Client, "--help");
    }

    void TriggerSymbolLoading(LPCSTR ModuleName)
    {
        CString command;
        command.Format("x %s!_any_symbol_just_to_trigger_pdb_loading", ModuleName);
        CExternalCommandParser cmd(m_Client, m_Control, command);
        cmd.Run();
    }

    void CheckSymbols()
    {
        CheckSymbols(m_Module);
    }

    void CheckSymbols(LPCSTR Name)
    {
        ULONG index;
        m_Result = m_Symbols->GetModuleByModuleName(Name, 0, &index, NULL);
        if (m_Result != S_OK) {
            Output("module %s is not loaded\n", Name);
            return;
        }
        // first round
        SYM_TYPE type = NumSymTypes;
        GetSymbolsState(index, Name, type, true);

        if (type == SymDeferred) {
            Output("Trying to load symbols ... \n");
            TriggerSymbolLoading(Name);
            GetSymbolsState(index, Name, type);
        }

        if (type != SymPdb) {
            Output("Trying to locate symbols ... \n");

            CStringArray additionalDirs;
            FindPdb(Name, additionalDirs);

            AppendSymbolPath(additionalDirs);

            if (additionalDirs.GetCount()) {
                GetSymbolsState(index, Name, type);
                if (type == SymDeferred) {
                    Output("Trying to load symbols ... \n");
                    TriggerSymbolLoading(Name);
                    GetSymbolsState(index, Name, type);
                }
            }
        }
    }

    void QueryModuleVersion(ULONG Index)
    {
        if (!m_Symbols3)
            return;
        VS_FIXEDFILEINFO info;
        m_Result = m_Symbols3->GetModuleVersionInformation(Index, 0, "\\",
            &info, sizeof(info), NULL);
        if (m_Result == S_OK) {
            Output("Driver version: %d.%d.%d.%d\n",
                HIWORD(info.dwFileVersionMS), LOWORD(info.dwFileVersionMS),
                HIWORD(info.dwFileVersionLS), LOWORD(info.dwFileVersionLS));
        }
        else {
            LOG("GetModuleVersionInformation, result %X", m_Result);
        }
    }

    void GetSymbolsState(IN ULONG Index, IN LPCSTR ModuleName, OUT SYM_TYPE& SymbolsType, bool MoreOutput = false)
    {
        DEBUG_MODULE_PARAMETERS params;
        LPCSTR name = ModuleName;
        m_Result = m_Symbols->GetModuleParameters(1, NULL, Index, &params);
        if (m_Result != S_OK) {
            ERR("GetModuleParameters failed for %s, error %X", name, m_Result);
            return;
        }

        SymbolsType = (SYM_TYPE)params.SymbolType;
        if (MoreOutput) {
            //
            CString timestamp = TimeStampToString(params.TimeDateStamp);
            Output("%s: %s\n", name, timestamp.GetString());
            QueryModuleVersion(Index);
        }

        Output("Symbols type: %s", Name<eDEBUG_SYMTYPE>(params.SymbolType));
        if (params.Flags) {
            Output(", flags:(%d=%s)", params.Flags, Flags<eDEBUG_MODULE>(params.Flags).GetString());
        }
        Output("\n");
        switch (params.SymbolType)
        {
        case DEBUG_SYMTYPE_DEFERRED:
            // nothing loaded and not tried yet
            break;
        case DEBUG_SYMTYPE_NONE:
            // tried and not found
            break;
        case DEBUG_SYMTYPE_PDB:
            // loaded
            break;
        default:
            break;
        }

        if (params.SymbolType != DEBUG_SYMTYPE_PDB && MoreOutput)
        {
            CGetLmiData cmd(m_Client, m_Control, name);
            cmd.Run();
            GUID guid = {};
            if (!cmd.Process(guid)) {
                ERR("Can't retrieve GUID from output!");
            }
        }

        if (params.SymbolType == DEBUG_SYMTYPE_PDB && m_Symbols3) {
            LOG("Getting symbol file name of %d\n", params.SymbolFileNameSize);
            CString symbolFileName;
            LPSTR buffer = symbolFileName.GetBufferSetLength(params.SymbolFileNameSize + 2);
            m_Result = m_Symbols3->GetModuleNameString(DEBUG_MODNAME_SYMBOL_FILE, Index, 0, buffer, params.SymbolFileNameSize, NULL);
            if (m_Result == S_OK) {
                Output("Symbol file: %s\n", symbolFileName.GetString());
            } else {
                LOG("GetModuleNameString failed for %s, error %X\n", name, m_Result);
            }
        }
    }

    bool CheckPdb(LPCSTR Module, LPCSTR PdbFile)
    {
        CCheckPdb cmd(m_Client, m_Control, Module, PdbFile);
        cmd.Run();
        return cmd.Parse();
    }

    void AppendSymbolPath(const CStringArray& Dirs)
    {
        for (UINT i = 0; i < Dirs.GetCount(); ++i) {
            Output("Appending %s\n", Dirs[i].GetString());
            m_Symbols->AppendSymbolPath(Dirs[i]);
        }

        if (Dirs.GetCount()) {
            CExternalCommandParser cmd(m_Client, m_Control, ".reload");
            cmd.Run();
        }
    }
    void FindPdbInDirectories(IN LPCSTR Name, OUT CStringArray& Dirs, IN const CStringArray& Where)
    {
        const CStringArray& dirs = Where;
        for (UINT i = 0; i < dirs.GetCount(); ++i) {
            LOG("Checking %s", dirs.GetAt(i).GetString());
            CProcessRunner r(true);
            CString cmdLine;
            cmdLine.Format("cmd.exe /c dir /s /b %s\\%s.pdb", dirs.GetAt(i).GetString(), Name);
            r.SetIntermediateWait(3000);
            r.RunProcess(cmdLine);
            int n = 0;
            do {
                CString next = r.StdOutResult().Tokenize("\r\n", n);
                next.Trim();
                if (next.IsEmpty())
                    break;
                bool bCompat = CheckPdb(Name, next);
                LOG("Found %s %s", next.GetString(), bCompat ? "GOOD" : "BAD");
                if (bCompat) {
                    Dirs.Add(DirectoryOf(next));
                    break;
                }
            } while (true);
        }
    }

    void FindPdb(LPCSTR Name, CStringArray& Dirs)
    {
        CStringArray dirs;
        GetExternalSymbolDirectories(dirs);
        FindPdbInDirectories(Name, Dirs, dirs);
    }
    void GetExternalSymbolDirectories(CStringArray& Dirs)
    {
        CString symPath;
        int n = 2048;
        ULONG got = 0;
        m_Symbols->GetSymbolPath(symPath.GetBufferSetLength(n), n, &got);
        if (!got)
            return;
        symPath.SetAt(got, 0);
        LOG("SymPath=%s(len %d)\n", symPath.GetString(), got);
        n = 0;
        CString next;
        do {
            next = symPath.Tokenize(";", n);
            LOG("SubPath %s (pos => %d)", next.GetString(), n);
            if (next.Find('*') >= 0)
                continue;
            if (next.IsEmpty())
                break;
            Dirs.Add(next);
        } while (true);
        Dirs.Add(".");
    }
    void Output(LPCSTR Format, ...)
    {
        va_list list;
        va_start(list, Format);
        m_Control->OutputVaList(DEBUG_OUTPUT_NORMAL, Format, list);
        va_end(list);
    }
protected:
    CComPtr<IDebugControl> m_Control;
    CComPtr<IDebugClient>  m_Client;
    CComPtr<IDebugSymbols> m_Symbols;
    CComPtr<IDebugSymbols3> m_Symbols3;
    HRESULT m_Result;
    CString m_Module;
    void WaitForDebugger()
    {
        Output("Connect debugger NOW!\n");
        while (!IsDebuggerPresent()) {
            Sleep(1000);
        }
        Sleep(10000);
        DebugBreak();
    }
private:
    static CString TimeStampToString(ULONG TimeDateStamp)
    {
        CString s;
        char buffer[256];
        __time32_t timestamp = TimeDateStamp;
        tm* timer = _gmtime32(&timestamp);
        strftime(buffer, sizeof(buffer), "%d %b %Y %H:%M:%S UTC, ", timer);
        s = buffer;

        timer = _localtime32(&timestamp);
        strftime(buffer, sizeof(buffer), "%d %b %Y %H:%M:%S Local", timer);
        s += buffer;

        return s;
    }
    static bool FileTimeMatch(const FILETIME& FileTime, __time32_t TimeStamp)
    {
        SYSTEMTIME st;
        FileTimeToSystemTime(&FileTime, &st);
        tm* timer = _gmtime32(&TimeStamp);
        bool match = (timer->tm_hour == st.wHour && timer->tm_min == st.wMinute &&
            timer->tm_sec == st.wSecond && timer->tm_mday == st.wDay &&
            (timer->tm_mon + 1) == st.wMonth && (timer->tm_year + 1900) == st.wYear);

        LOG("ft:%d-%d-%d-%d-%d-%d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        LOG("ts:%d-%d-%d-%d-%d-%d", timer->tm_year, timer->tm_mon, timer->tm_mday, timer->tm_hour, timer->tm_min, timer->tm_sec);
        LOG("match - %d", match);

        return match;
    }
};

extern "C" __declspec(dllexport) HRESULT CALLBACK DebugExtensionInitialize(
    OUT PULONG Version,
    OUT PULONG Flags)
{
    *Version = DEBUG_EXTENSION_VERSION(0, 1);
    *Flags = 0;
#if USE_EXT_API
    CComPtr<IDebugControl> control;
    HRESULT hr = DebugCreate(__uuidof(IDebugControl), (PVOID *)&control);
    if (hr != S_OK) {
        LOG("%s: DebugCreate error %X", __FUNCTION__, hr);
    } else {
        ExtensionApis.nSize = sizeof(ExtensionApis);
        hr = control->GetWindbgExtensionApis64(&ExtensionApis);
        if (hr != S_OK) {
            LOG("%s: GetWindbgExtensionApis64 error %X", __FUNCTION__, hr);
        } else {
            LOG("%s: GetWindbgExtensionApis64 got OK", __FUNCTION__);
            // this does not produce any affect
            dprintf("WinDbg extension API active\n");
        }
    }
#endif
    return S_OK;
}

class CDebugExtensionNet : public CDebugExtension
{
public:
    CDebugExtensionNet(PDEBUG_CLIENT Client) : CDebugExtension(Client, "netkvm") {}
    void mp()
    {
        CPtrArray adapters;
        CString version = "Unknown";
        {
            CGetVirtioAdapters cmd(m_Client, m_Control);
            cmd.Run();
            cmd.Process(adapters);
        }
        Output("%d virtio network adapters\n", (int)adapters.GetCount());
        if (!adapters.GetCount())
            return;
        for (int i = 0; i < adapters.GetCount(); ++i) {
            ULONGLONG context = 0;
            Output("#%d: adapter %I64X", i, (ULONGLONG)adapters[i]);
            {
                CGetAdapterContext cmd(m_Client, m_Control, (ULONGLONG)adapters[i]);
                cmd.Run();
                cmd.Process(context);
                cmd.Process(version);
            }
            adapters.ElementAt(i) = (PVOID)context;
            if (context) {
                Output(", context %I64X, version %s\n", (ULONGLONG)adapters[i], version.GetString());
            }
            else {
                Output(", context unknown\n");
            }
        }
        CheckSymbols();
    }
    void help()
    {
        Output("mp - find miniport and symbols in known places\n");
        __super::help();
    }
};

extern "C" __declspec(dllexport) HRESULT help(IN PDEBUG_CLIENT Client, IN PCSTR Args)
{
    VERBOSE("%s: =>", __FUNCTION__);
    CDebugExtensionNet e(Client);
    e.help();
    VERBOSE("%s: <=", __FUNCTION__);
    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT mp(IN PDEBUG_CLIENT Client, IN PCSTR Args)
{
    VERBOSE("%s: =>", __FUNCTION__);
    CDebugExtensionNet e(Client);
    e.mp();
    VERBOSE("%s: <=", __FUNCTION__);
    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT test(IN PDEBUG_CLIENT Client, IN PCSTR Args)
{
    VERBOSE("%s: =>", __FUNCTION__);
    CDebugExtensionNet e(Client);
    e.TestCommand(Args);
    VERBOSE("%s: <=", __FUNCTION__);
    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT findpdb(IN PDEBUG_CLIENT Client, IN PCSTR Args)
{
    VERBOSE("%s: =>", __FUNCTION__);
    CDebugExtensionNet e(Client);
    e.findpdb(Args);
    VERBOSE("%s: <=", __FUNCTION__);
    return S_OK;
}

extern "C" __declspec(dllexport) void CALLBACK DebugExtensionUninitialize() {}

#if 0
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
#endif
