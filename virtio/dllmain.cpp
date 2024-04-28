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
private:
    PDEBUG_OUTPUT_CALLBACKS m_Previous = NULL;
    PDEBUG_CLIENT m_Client;
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

class CDebugExtension
{
public:
    CDebugExtension(PDEBUG_CLIENT Client)
    {
        LOG("%s", __FUNCTION__);
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
    ~CDebugExtension()
    {
        LOG("%s", __FUNCTION__);
    }
    void help()
    {
        verbose(m_Client, "--help");
        m_Control->Output(DEBUG_OUTPUT_NORMAL, "mp\n");
    }
    void mp()
    {
        CPtrArray adapters;
        CString version = "Unknown";
        {
            CGetVirtioAdapters cmd(m_Client, m_Control);
            cmd.Run();
            cmd.Process(adapters);
        }
        m_Control->Output(DEBUG_OUTPUT_NORMAL, "%d virtio network adapters\n", (int)adapters.GetCount());
        if (!adapters.GetCount())
            return;
        for (int i = 0; i < adapters.GetCount(); ++i) {
            ULONGLONG context = 0;
            m_Control->Output(DEBUG_OUTPUT_NORMAL, "#%d: adapter %I64X", i, (ULONGLONG)adapters[i]);
            {
                CGetAdapterContext cmd(m_Client, m_Control, (ULONGLONG)adapters[i]);
                cmd.Run();
                cmd.Process(context);
                cmd.Process(version);
            }
            adapters.ElementAt(i) = (PVOID)context;
            if (context) {
                m_Control->Output(DEBUG_OUTPUT_NORMAL, ", context %I64X, version %s\n", (ULONGLONG)adapters[i], version.GetString());
            } else {
                m_Control->Output(DEBUG_OUTPUT_NORMAL, ", context unknown\n");
            }
        }
        CheckSymbols("netkvm");
    }
    void CheckSymbols(LPCSTR name)
    {
        ULONG index;
        DEBUG_MODULE_PARAMETERS params;
        m_Result = m_Symbols->GetModuleByModuleName(name, 0, &index, NULL);
        if (m_Result != S_OK) {
            m_Control->Output(DEBUG_OUTPUT_NORMAL, "module %s is not loaded\n", name);
            return;
        }

        m_Result = m_Symbols->GetModuleParameters(1, NULL, index, &params);
        if (m_Result != S_OK) {
            LOG("GetModuleParameters failed for %s, error %X\n", name, m_Result);
            return;
        }
        CString timestamp = TimeStampToString(params.TimeDateStamp);
        m_Control->Output(DEBUG_OUTPUT_NORMAL, "%s: %s\n", name, timestamp.GetString());
        m_Control->Output(DEBUG_OUTPUT_NORMAL, "Symbols type: %s", Name<eDEBUG_SYMTYPE>(params.SymbolType));
        if (params.Flags) {
            m_Control->Output(DEBUG_OUTPUT_NORMAL, ", flags:(%d=%s)", params.Flags, Flags<eDEBUG_MODULE>(params.Flags).GetString());
        }
        m_Control->Output(DEBUG_OUTPUT_NORMAL, "\n");
        switch (params.SymbolType)
        {
            case DEBUG_SYMTYPE_DEFERRED:
                // nothing loaded and not tried
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
        if (params.SymbolType == DEBUG_SYMTYPE_PDB && m_Symbols3) {
            LOG("Getting symbol file name of %d\n", params.SymbolFileNameSize);
            CString symbolFileName;
            LPSTR buffer = symbolFileName.GetBufferSetLength(params.SymbolFileNameSize + 2);
            m_Result = m_Symbols3->GetModuleNameString(DEBUG_MODNAME_SYMBOL_FILE, index, 0, buffer, params.SymbolFileNameSize, NULL);
            if (m_Result == S_OK) {
                m_Control->Output(DEBUG_OUTPUT_NORMAL, "Symbol file: %s\n", symbolFileName.GetString());
            } else {
                LOG("GetModuleNameString failed for %s, error %X\n", name, m_Result);
            }
        }
    }
    void TestCommand(LPCSTR Command)
    {
        CTestCommand cmd(m_Client, m_Control, Command);
        cmd.Run();
    }
protected:
    CComPtr<IDebugControl> m_Control;
    CComPtr<IDebugClient>  m_Client;
    CComPtr<IDebugSymbols> m_Symbols;
    CComPtr<IDebugSymbols3> m_Symbols3;
    HRESULT m_Result;
private:
    CString TimeStampToString(ULONG TimeDateStamp)
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

extern "C" __declspec(dllexport) HRESULT help(IN PDEBUG_CLIENT Client, IN PCSTR Args)
{
    VERBOSE("%s: =>", __FUNCTION__);
    CDebugExtension e(Client);
    e.help();
    VERBOSE("%s: <=", __FUNCTION__);
    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT mp(IN PDEBUG_CLIENT Client, IN PCSTR Args)
{
    VERBOSE("%s: =>", __FUNCTION__);
    CDebugExtension e(Client);
    e.mp();
    VERBOSE("%s: <=", __FUNCTION__);
    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT test(IN PDEBUG_CLIENT Client, IN PCSTR Args)
{
    VERBOSE("%s: =>", __FUNCTION__);
    CDebugExtension e(Client);
    e.TestCommand(Args);
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
