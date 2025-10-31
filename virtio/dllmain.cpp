// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

#pragma comment(lib, "dbgeng.lib")
#pragma comment(lib, "dbghelp.lib")

//#define THIS_IS_WINDBG_EXTENSION
WINDBG_EXTENSION_APIS ExtensionApis;

bool bVerbose;

extern "C" __declspec(dllexport) HRESULT verbose(IN PDEBUG_CLIENT Client, IN PCSTR Args)
{
    CComPtr<IDebugClient> client = Client;
    CComPtr<IDebugControl> control;
    auto usage = [&]() { control->Output(DEBUG_OUTPUT_NORMAL, "verbose <on|off>    - enable/disable extended debug output to DbgView\n"); };
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
        //control->Output(DEBUG_OUTPUT_NORMAL, "verbose <on|off>\n");
        usage();
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
        m_Client->GetOutputCallbacks(&m_Previous);
        m_Client->SetOutputCallbacks(this);
    }
    ~CDebugOutputCallback()
    {
        m_Client->SetOutputCallbacks(m_Previous);
    }
    void GetOutput(CStringArray& Arr)
    {
        Arr.RemoveAll();
        for (int i = 0; i < m_Output.GetCount(); ++i) {
            Arr.Add(m_Output[i]);
        }
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
    CComPtr<IDebugClient> m_Client;
private:
    PDEBUG_OUTPUT_CALLBACKS m_Previous = NULL;
    STDMETHOD_(ULONG, AddRef)(THIS) override {
        return CUnknown::AddRef();
    }
    STDMETHOD_(ULONG, Release)(THIS) override {
        return CUnknown::Release();
    }
};

class CExternalCommandParser
{
public:
    CExternalCommandParser(PDEBUG_CLIENT Client, LPCSTR Command) :
        m_Command(Command), m_Client(Client)
    {
        m_Client.QueryInterface<IDebugControl>(&m_Control);
    }
    virtual HRESULT Run()
    {
        CDebugOutputCallback cb(m_Client);
        // at this point the output callback is set, previous one is saved
        bool saveVerbose = bVerbose;
        if (m_Verbose) {
            bVerbose = true;
        }
        HRESULT res = m_Control->Execute(DEBUG_OUTPUT_NORMAL, m_Command, DEBUG_EXECUTE_NO_REPEAT);
        if (m_Verbose) {
            bVerbose = saveVerbose;
        }
        cb.GetOutput(m_Output);
        return res;
    }
protected:
    CComPtr<IDebugClient> m_Client;
    CComPtr<IDebugControl> m_Control;
    CString m_Command;
    void BeVerbose() { m_Verbose = true; }
    void CombineOutput(CString& Combined)
    {
        for (int i = 0; i < m_Output.GetCount(); ++i) {
            Combined += m_Output[i];
        }
    }
    CStringArray m_Output;
private:
    bool m_Verbose = false;
};

class CTestCommand : public CExternalCommandParser
{
public:
    CTestCommand(PDEBUG_CLIENT Client, LPCSTR Command) :
        CExternalCommandParser(Client, Command)
    {
        BeVerbose();
    }
};

class CGetVirtioAdapters : public CExternalCommandParser
{
public:
    CGetVirtioAdapters(PDEBUG_CLIENT Client) :
        CExternalCommandParser(Client, "!ndiskd.miniports") {}
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
    CGetAdapterContext(PDEBUG_CLIENT Client, ULONGLONG Adapter) :
        CExternalCommandParser(Client, MakeCommand(Adapter)) {}
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
    CCheckPdb(PDEBUG_CLIENT Client, LPCSTR ModuleName, LPCSTR PdbName) :
        CExternalCommandParser(Client, MakeCommand(ModuleName, PdbName))
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

class CGetLmiData : public CExternalCommandParser
{
public:
    CGetLmiData(PDEBUG_CLIENT Client, LPCSTR Name) :
        CExternalCommandParser(Client, MakeCommand(Name))
    {
        VERBOSE("%s: %s", __FUNCTION__, m_Command.GetString());
        BeVerbose();
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

class CDumpNameGetter : public CExternalCommandParser
{
public:
    CDumpNameGetter(PDEBUG_CLIENT Client) :
        CExternalCommandParser(Client, "dx -r0 Debugger.Sessions[0]")
    {}
    CString Parse()
    {
        for (UINT i = 0; i < m_Output.GetCount(); ++i) {
            m_Output[i].MakeLower();
            if (m_Output[i].Find(".dmp") > 0) {
                CString s = m_Output[i];
                LOG("Found %s", s.GetString());
                s.Delete(0, 6 + s.Find("dump:"));
                s.Delete(s.ReverseFind('\\'), MAXINT16);
                LOG("Assuming %s", s.GetString());
                return s;
            }
        }
        return "";
    }
};

class CIommuDetect : public CExternalCommandParser
{
public:
    CIommuDetect(PDEBUG_CLIENT Client) :
        CExternalCommandParser(Client, "!acpicache")
    {}
    bool Present()
    {
        Run();
        for (UINT i = 0; i < m_Output.GetCount(); ++i) {
            if (m_Output[i].Find("DMAR") >= 0) {
                return true;
            }
        }
        return false;
    }
};

class CRegCommand : public CExternalCommandParser
{
public:
    CRegCommand(PDEBUG_CLIENT Client, LPCSTR Command) :
        CExternalCommandParser(Client, "!reg ")
    {
        m_Command += Command;
    }
    CString GetString()
    {
        Run();
        for (UINT i = 0; i < m_Output.GetCount(); ++i) {
            if (m_Output[i].Find("REG_SZ") >= 0) {
                return m_Output[i];
            }
        }
        return "";
    }
};

#define FIELD_FLAG_REAL     0x80000000

static void FieldMarkReal(FIELD_INFO& Field, bool Real)
{
    Field.fOptions &= ~FIELD_FLAG_REAL;
    Field.fOptions |= Real ? FIELD_FLAG_REAL : 0;
}

static bool FieldIsReal(const FIELD_INFO& Field)
{
    return (Field.fOptions & 0x80000000) != 0;
}

class CFieldInfo
{
public:
    CFieldInfo(const FIELD_INFO &Info) : m_Info(Info)
    {
        m_Info.printName = nullptr;
        Name((LPCSTR)Info.fName);
        Describe();
    }
    CFieldInfo& operator =(const CFieldInfo& Other)
    {
        m_Name = Other.m_Name;
        m_Type = Other.m_Type;
        m_Info = Other.m_Info;
        m_Info.fName = (PUCHAR)m_Name.GetBuffer();
        m_Description = Other.m_Description;
        return *this;
    }
    CFieldInfo() { }
    void Type(LPCSTR Type) { m_Type = Type; }
    void Name(LPCSTR Name) { m_Name = Name; m_Info.fName = (PUCHAR)m_Name.GetBuffer(); }
    LPCSTR Name() const { return m_Name; }
    LPCSTR Type() const { return m_Type; }
    LPCSTR Description() const { return m_Description; }
    operator FIELD_INFO& () { return m_Info; }
    operator FIELD_INFO* () { return &m_Info; }
    operator const FIELD_INFO& () const { return m_Info; }
    operator const FIELD_INFO* () const { return &m_Info; }
    ULONG64 Offset() const { return m_Info.address; }
    ULONG Size() const { return m_Info.size; }
    void AdjustOffset(LONG Change) { m_Info.address += Change; }
    bool IsReal() const { return FieldIsReal(m_Info); }
    bool IsPointer() const { return m_Info.fPointer; }
    bool IsList() const
    {
        bool b = !m_Type.Compare("_LIST_ENTRY") || m_Type.Compare("LIST_ENTRY");
        b = b && !m_Name.Compare("m_List");
        return b;
    }
private:
    FIELD_INFO m_Info = {};
    CString m_Name;
    CString m_Description;
    CString m_Type;
    void Describe()
    {
        auto append = [&](LPCSTR s) {
            if (!m_Description.IsEmpty()) {
                m_Description += ',';
            }
            m_Description += s;
        };
        if (m_Info.fStatic) append("static");
        if (m_Info.fConstant) append("const");
        if (m_Info.fArray) append("array");
        if (m_Info.fPointer) append("ptr");
        if (m_Info.fStruct) append("struct");
    }
};

typedef CArray<CFieldInfo> CBasicFieldsArray;
class CFieldsArray : public CBasicFieldsArray
{
public:
    void SetFilter(LPCSTR Filter) { m_Filter = Filter; }
    INT_PTR Add(const CFieldInfo& Info)
    {
        if (!m_Filter.IsEmpty() && !Match(Info.Name())) {
            return -1;
        }
        return __super::Add(Info);
    }
protected:
    CString m_Filter;
    bool Match(LPCSTR Name)
    {
        return PathMatchSpecA(Name, m_Filter);
    }
};

struct ADDRESS_INFO
{
    ADDRESS_INFO(ULONG64 _Address)
    {
        Address = _Address;
        IsReal = !!Address;
    }
    ULONG64 Address;
    bool IsReal;
};

class CPureParser
{
public:
    virtual void Process(const CFieldInfo& Field, LPCSTR OuterType) = 0;
};

template <typename TParam>
class CBasicParser : public CPureParser
{
protected:
    CBasicParser(TParam Parameter) : m_Parameter(Parameter) {}
    void Process(const CFieldInfo& Field, LPCSTR OuterType) override
    {
        Worker(Field, OuterType);
    }
    virtual void Worker(const CFieldInfo& Field, LPCSTR OuterType) = 0;
    TParam m_Parameter;
};

class CDummyParser : public CBasicParser<int>
{
public:
    CDummyParser() : CBasicParser(0) {}
protected:
    void Worker(const CFieldInfo&, LPCSTR) override {};
};

static CDummyParser DummyParser;

class CListParser : public CBasicParser<IDebugControl3 *>
{
public:
    CListParser(IDebugControl3 *p) : CBasicParser(p) {}
    void Worker(const CFieldInfo& Field, LPCSTR OuterType) override
    {
        if (!Field.IsReal() || !Field.IsList()) {
            return;
        }

        CString listType = OuterType;
        CString entryType;
        ULONG64 Root = Field.Offset();
        ULONG offset;

        LOG("%s: @%p %s", __FUNCTION__, (PVOID)Root, OuterType);

        int found = listType.Find('<');
        if (found > 0) {
            int end = listType.FindOneOf(">,");
            if (end > found) {
                LOG("%s: from %d to %d", __FUNCTION__, found, end);
                entryType = listType.Mid(found + 1, end - found - 1);
            }
        }
        if (entryType.IsEmpty()) {
            LOG("%s: entry type not recognized", __FUNCTION__);
            return;
        }
        if (GetFieldOffset(entryType, "m_ListEntry", &offset)) {
            LOG("%s: type %s is not list member", __FUNCTION__, entryType.GetString());
            return;
        }
        auto readEntry = [](ULONG64 Address) -> LIST_ENTRY
        {
            LIST_ENTRY e;
            ReadMemory(Address, &e, sizeof(e), NULL);
            return e;
        };

        LIST_ENTRY localRoot, localCurrent;
        ULONG64 targetCurrent, targetNext;
        localRoot = readEntry(Root);
        for (targetCurrent = (ULONG64)localRoot.Flink; targetCurrent != Root; targetCurrent = targetNext) {
            localCurrent = readEntry(targetCurrent);
            targetNext = (ULONG64)localCurrent.Flink;
            ULONG64 object = targetCurrent - offset;
            m_Parameter->ControlledOutput(
                DEBUG_OUTCTL_DML,
                DEBUG_OUTPUT_NORMAL,
                "<link cmd=\"?? (%s *)0x%p\">%s at %p</link>\n",
                entryType.GetString(), (PVOID)object,
                entryType.GetString(), (PVOID)object);
        }
    }
};

class CFieldParser : public CBasicParser<int>
{
public:
    CFieldParser() : CBasicParser(0) {}
    void Worker(const CFieldInfo& Field, LPCSTR OuterType) override
    {
        m_Info = Field;
    }
    CFieldInfo& Info() { return m_Info; }
protected:
    CFieldInfo m_Info;
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
        CTestCommand cmd(m_Client, Command);
        cmd.Run();
    }
    void cn()
    {
        CString s;
        CRegCommand cmd(m_Client, "q \\registry\\machine\\System\\ControlSet001\\Control\\ComputerName\\ActiveComputerName");
        s = cmd.GetString();
        if (!s.IsEmpty()) {
            Output("%s\n", s.GetString());
        }
    }
    void findpdb(LPCSTR TopDir)
    {
        CStringArray dirs;
        if (*TopDir) {
            dirs.Add(TopDir);
        } else {
            CPickFolderDialog d;
            if (d.DoModal() == IDOK) {
                dirs.Add(d.GetPathName());
            }
        }
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
    // this actually works like 'x' command
    // just returns the address as I64
    // and does not understand fields
    void Evaluate(LPCSTR Expression)
    {
        DEBUG_VALUE v = {};
        ULONG remainderIndex = 0;
        m_Result = m_Control->Evaluate(Expression, DEBUG_VALUE_INVALID, &v, &remainderIndex);
        if (FAILED(m_Result)) {
            Output("evaluation failed, error %X, index %d\n", m_Result, remainderIndex);
            return;
        }
        Output("received type %d(%s), value 0x%p\n", v.Type, Name<eDEBUG_VALUE_TYPE>(v.Type), (PVOID)v.I64);
    }
protected:
    CDebugExtension(PDEBUG_CLIENT Client, LPCSTR Module, LPCSTR MainContext)
    {
        LOG("%s %s", __FUNCTION__, Module);
        m_Module = Module;
        m_Client = Client;
        m_MainContext = MainContext;
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
        m_Result = m_Client.QueryInterface<IDebugDataSpaces>(&m_DataSpaces);
        if (m_Result != S_OK) {
            ERR("%s: Can't obtain IDebugDataSpaces", __FUNCTION__);
            throw;
        }

        // optional, do not fail if not present
        m_Result = m_Client.QueryInterface<IDebugSymbols3>(&m_Symbols3);
        if (m_Result != S_OK) {
            LOG("%s: Can't obtain IDebugSymbols3", __FUNCTION__);
        }
        // optional, do not fail if not present
        m_Result = m_Client.QueryInterface<IDebugControl3>(&m_Control3);
        if (m_Result != S_OK) {
            LOG("%s: Can't obtain IDebugControl3", __FUNCTION__);
        }

        ExtensionApis.nSize = sizeof(ExtensionApis);
        m_Result = m_Control->GetWindbgExtensionApis64(&ExtensionApis);
        if (m_Result != S_OK) {
            LOG("%s: Can't obtain WinDbg Extension API (%X)", __FUNCTION__, m_Result);
            ExtensionApis.nSize = 0;
        } else {
            #define chkptr(f) { if (!f) LOG(#f " is not initialized!"); }
            chkptr(dprintf);
            chkptr(GetExpression);
            chkptr(ReadMemory);
            chkptr(WriteMemory);
            chkptr(StackTrace);
            chkptr(Ioctl);
            chkptr(dprintf);

            #undef chkptr
        }
    }
    void help()
    {
        verbose(m_Client, "--help");
        Output("findpdb <directory> - recursive find and append to symbol path\n");
        Output("hv                  - dump Hyper-V fields\n");
        Output("cn                  - get computer name\n");
    }

    void TriggerSymbolLoading(LPCSTR ModuleName)
    {
        CString command;
        command.Format("x %s!_any_symbol_just_to_trigger_pdb_loading", ModuleName);
        CExternalCommandParser cmd(m_Client, command);
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
        m_Result = m_Symbols3->GetModuleVersionInformation(Index, 0, "\\StringFileInfo\\FileVersion",
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
            CGetLmiData cmd(m_Client, name);
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
        CCheckPdb cmd(m_Client, Module, PdbFile);
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
            CExternalCommandParser cmd(m_Client, ".reload");
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
            Output("%d files to check\n", CountLines(r.StdOutResult()));
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
    void GetSymbolPath(CString& Path)
    {
        int n = 2048;
        ULONG got = 0;
        m_Symbols->GetSymbolPath(Path.GetBufferSetLength(n), n, &got);
        if (!got)
            return;
        Path.SetAt(got, 0);
        LOG("SymPath=%s(len %d)\n", Path.GetString(), got);
    }
    void GetExternalSymbolDirectories(CStringArray& Dirs)
    {
        CString symPath;
        GetSymbolPath(symPath);

        Tokenize(symPath, ";", Dirs, [&](CString& next)
            {
                if (next.Find('*') >= 0)
                    return false;
                return true;
            });

        // add dump file directory, if any
        CDumpNameGetter getter(m_Client);
        getter.Run();
        symPath = getter.Parse();
        if (!symPath.IsEmpty()) {
            Dirs.Add(symPath);
        }
    }
    void Output(LPCSTR Format, ...)
    {
        va_list list;
        va_start(list, Format);
        m_Control->OutputVaList(DEBUG_OUTPUT_NORMAL, Format, list);
        va_end(list);
    }
    bool GetTypeName(ULONG64 ModuleBase, ULONG TypeId, CString& TypeName)
    {
        char buffer[1024];
        if (FAILED(m_Symbols->GetTypeName(ModuleBase, TypeId, buffer, sizeof(buffer), nullptr))) {
            return false;
        }
        TypeName = buffer;
        return true;
    }

    // sets only address, type & typeId, size
    // flags we need to guess
    // description is created based on guessed flags
    bool ResolveSymbol(LPCSTR Name, CFieldInfo& Out)
    {
        CString sFullName;
        ULONG64 moduleBase;
        FIELD_INFO info = {};

        info.fName = (PUCHAR)Name;

        if (strchr(Name, '!')) {
            sFullName = Name;
        }
        else {
            sFullName.Format("%s!%s", m_Module.GetString(), Name);
        }

        m_Result = m_Symbols->GetOffsetByName(sFullName, &info.address);
        if (FAILED(m_Result)) {
            Output("Can't find %s, error %X\n", sFullName.GetString(), m_Result);
            return false;
        }

        m_Result = m_Symbols->GetSymbolTypeId(sFullName, &info.TypeId, &moduleBase);
        if (FAILED(m_Result)) {
            Out = info;
            return true;
        }
        UpdateDescription(moduleBase, info);

        Out = info;

        UpdateInfo(moduleBase, Out);

        return true;
    }

#if 0
    void ATL_NOINLINE UpdateTag(ULONG64 ModuleBase, FIELD_INFO& info)
    {
        ULONG val[256];
        UINT used = 0;
        HANDLE magic = HANDLE((ULONG_PTR)0xf0f0f0f0);
#if 0
        if (!m_SymHandle) {
            CString symPath;
            GetSymbolPath(symPath);
            if (!SymInitialize(this, symPath, false)) {
                return;
            }
            m_SymHandle = this;
            ULONG64 res = SymLoadModule64(m_SymHandle, NULL, NULL, m_Module, 0, 0);
            LOG("SymLoadModule: %p, known module base %p", (PVOID)res, (PVOID)ModuleBase);
        }
#endif
        memset(val, 0xff, sizeof(val));
        if (!SymGetTypeInfo(magic, ModuleBase, info.TypeId, TI_GET_SYMTAG, val)) {
            LOG("Can't get tag of %s", info.fName);
            return;
        }
        enum SymTagEnum tag = (enum SymTagEnum)val[0];
        for (UINT i = 0; i < ARRAYSIZE(val); ++i) {
            if (val[i] != 0xffffffff) {
                used = i + 1;
            }
        }
        LOG("Tag of %s: %d(%s), used %d on stack", info.fName, val[0], Name<enum SymTagEnum>(val[0]), used);
    }
#endif

    void ATL_NOINLINE UpdateDescription(ULONG64 ModuleBase, FIELD_INFO& info, bool Force = false)
    {
        if (Force) {
            info.fPointer = info.fArray = info.fStruct = 0;
        }
        if (info.fPointer) {
            info.size = sizeof(PVOID);
            return;
        }
        if (info.fArray || info.fStruct) {
            return;
        }
        // nothing set
        CString typeName;
        CFieldsArray fields;

        if (GetTypeName(ModuleBase, info.TypeId, typeName) &&
            QueryStructField(typeName, "*", fields) &&
            fields.GetSize()) {
                info.fStruct = 1;
                return;
        }
        if (typeName.IsEmpty()) {
            return;
        }
        if (typeName.ReverseFind('*') == (typeName.GetLength() - 1)) {
            info.fPointer = 1;
            info.size = sizeof(PVOID);
        }
    }

    void ATL_NOINLINE UpdateInfo(ULONG64 ModuleBase, CFieldInfo& Out, bool IsArrayMember = false, UINT Index = 0)
    {
        FIELD_INFO& info = Out;
        CString typeName;
        if (!GetTypeName(ModuleBase, info.TypeId, typeName)) {
            typeName = "<hard to say>";
        }
        Out.Type(typeName);

        m_Result = m_Symbols->GetTypeSize(ModuleBase, info.TypeId, &info.size);
        if (FAILED(m_Result)) {
            Output("Can't get size of %s, error %X\n", Out.Name(), m_Result);
            return;
        }
        if (!IsArrayMember) {
            return;
        }

        CString typeOfEntry = Out.Type();
        INT pos = typeOfEntry.Find('[');
        if (pos <= 0) {
            // the type of Out is not an array
            pos = typeOfEntry.ReverseFind('*');
            if (pos <= 0) {
                // the type of Out is not an pointer
                Output("Type %s can't be indexed\n", Out.Type());
                return;
            }
        }
        typeOfEntry.SetAt(pos, 0);
#if 1
        LOG("Examining typeId of %s", typeOfEntry.GetString());
        m_Result = m_Symbols->GetTypeId(ModuleBase, typeOfEntry, &info.TypeId);
        if (FAILED(m_Result)) {
            Out.Type("<hard to say>");
            info.size = 0;
            return;
        }
        m_Result = m_Symbols->GetTypeSize(ModuleBase, info.TypeId, &info.size);
        if (FAILED(m_Result)) {
            Output("Can't get size of type %s, error %X\n", typeOfEntry.GetString(), m_Result);
            info.size = 0;
            return;
        }
        UpdateDescription(ModuleBase, info, true);
        Out = CFieldInfo(info);
        Out.Type(typeOfEntry);
#else
        CFieldsArray Fields;
        if (!QueryStructField(typeOfEntry, "", Fields)) {
            return;
        }
        // address & name is not set
        Fields[0].Name(Out.Name());
        ((FIELD_INFO *)Fields[0])->address = info.address;
        Out = Fields[0];
#endif
        if (Index) {
            Out.AdjustOffset(Out.Size() * Index);
        }
    }

    // Path[0] = type or name of Base, for visibility only
    // Base might be with (context or resolved) or
    // without address (when only type is given)
    bool ATL_NOINLINE TraverseToField(CFieldInfo& Base, CStringArray& Path, CPureParser& Parser = DummyParser)
    {
        CString type = Base.Type();
        CString combined = Path[0];
        ULONG size = 0;
        bool bOK = true;
        ADDRESS_INFO address(Base.Offset());
        Output("%s:(@0x%p) of %d %s (%s)\n", Path[0].GetString(), (PVOID)Base.Offset(),
            Base.Size(), Base.Type(), Base.Description());
        for (UINT i = 1; i < Path.GetSize(); ++i) {
            LOG("Looking for %s.%s", type.GetString(), Path[i].GetString());
            CFieldsArray fields;
            if (!QueryStructField(type, Path[i], fields, &address)) {
                bOK = false;
                break;
            }
            for (UINT k = 0; k < fields.GetCount(); ++k) {
                auto& f = fields[k];

                // address is an offset in the parent structure
                // or real virtual address of the field
                CString sAddress;
                if (f.IsReal()) {
                    sAddress.Format("(@0x%p)", (PVOID)f.Offset());
                } else {
                    sAddress.Format("offset %d", (ULONG)f.Offset());
                }
                sAddress.MakeLower();

                Output("%s.%s %s of %d, type %s (%s)\n",
                    combined.GetString(),
                    f.Name(),
                    sAddress.GetString(),
                    f.Size(),
                    f.Type(),
                    f.Description());
                if (fields.GetCount() == 1) {
                    // one field of interest
                    if (i == (Path.GetCount() - 1)) {
                        // last field in a chain
                        Parser.Process(f, type);
                    }
                    type = f.Type();
                    combined.AppendFormat(".%s", f.Name());
                }
            }
            if (fields.GetCount() > 1) {
                bOK = false;
                break;
            }
        }
        return bOK;
    }

protected:
    CComPtr<IDebugControl> m_Control;
    CComPtr<IDebugClient>  m_Client;
    CComPtr<IDebugSymbols> m_Symbols;
    CComPtr<IDebugSymbols3> m_Symbols3;
    CComPtr<IDebugControl3> m_Control3;
    CComPtr<IDebugDataSpaces> m_DataSpaces;
    HRESULT m_Result;
    CString m_Module;
    CString m_MainContext;
    bool HasEAPI() const { return ExtensionApis.nSize; }

    void WaitForDebugger()
    {
        Output("Connect debugger NOW!\n");
        while (!IsDebuggerPresent()) {
            Sleep(1000);
        }
        Sleep(10000);
        DebugBreak();
    }
    void GetDataFromContextField(PVOID Context, LPCSTR FieldName, PVOID Buffer, ULONG Length, ULONG& Result)
    {
        if (!HasEAPI()) {
            LOG("WDBG API is not available!");
            Result = INCORRECT_VERSION_INFO;
            return;
        }
        Result = GetFieldData((ULONG_PTR)Context, m_MainContext, FieldName, Length, Buffer);
    }

    // this could be one of more fields depending on FieldName
    bool GetContextFieldProperties(LPCSTR FieldName, /*OUT*/ CFieldsArray& Fields)
    {
        return QueryStructField(m_MainContext, FieldName, Fields);
    }

    bool QueryStruct(LPCSTR StructName, /*OUT*/ CFieldInfo& Field, ULONG64 Offset = 0)
    {
        CFieldsArray fields;
        ADDRESS_INFO address(Offset);
        if (QueryStructField(StructName, "", fields, &address) && fields.GetCount() == 1) {
            Field = fields[0];
            return true;
        }
        return false;
    }

    bool ATL_NOINLINE QueryStructField(LPCSTR StructName, LPCSTR FieldName, CFieldsArray& Fields, ADDRESS_INFO *Address = nullptr)
    {
        bool fArrayMember = false;
        bool fStructArray = false;
        bool fVerboseInCallback = true;
        UINT index = 0;
        INT pos;
        CString type, name;
        ULONG64 address = Address ? Address->Address : 0;

        // remove '[]' if present in Struct
        type = StructName;
        pos = type.Find('[');
        if (pos > 0) {
            type.SetAt(pos, 0);
            // currently nothying to do with it
            fStructArray = true;
        }
        name = FieldName;
        pos = name.Find('[');
        if (pos > 0) {
            index = atoi(name.GetString() + pos + 1);
            name.SetAt(pos, 0);
            fArrayMember = true;
        }

        PSYM_DUMP_FIELD_CALLBACK callback =
            [](FIELD_INFO* Field, PVOID UserContext) -> ULONG ATL_NOINLINE
        {
            CFieldsArray* pFields = (CFieldsArray *)UserContext;
            VERBOSE("Field callback called for %s, address %p", Field->fName, (PVOID)Field->address);
            CFieldInfo f(*Field);
            pFields->Add(f);
            return 0;
        };

        FIELD_INFO flds = {};
        flds.fName = (PUCHAR)name.GetString();
        flds.fOptions = DBG_DUMP_FIELD_RETURN_ADDRESS | DBG_DUMP_FIELD_FULL_NAME;
        flds.fieldCallBack = callback;

        SYM_DUMP_PARAM Sym = {};
        Sym.size = sizeof(Sym);
        Sym.sName = (PUCHAR)type.GetString();
        Sym.Options = DBG_DUMP_NO_PRINT | DBG_DUMP_CALL_FOR_EACH;
        Sym.Context = &Fields;
        Sym.nFields = 1;
        Sym.Fields = &flds;

        // is this wildcard?
        if (strchr(FieldName,'*')) {
            Sym.nFields = 0;
            Sym.CallbackRoutine = callback;
            // no printout from callback when _just_ enumerating fields
            fVerboseInCallback = !!Address;
            Fields.SetFilter(FieldName);
        }
        if (*FieldName == '!') {
            Sym.nFields = 0;
            Sym.CallbackRoutine = callback;
            Sym.addr = address;
        }
        if (*FieldName == 0) {
            Sym.nFields = 0;
            Sym.CallbackRoutine = callback;
            Sym.Options &= ~DBG_DUMP_CALL_FOR_EACH;
        }

        bool saveVerbose = bVerbose;
        bVerbose = fVerboseInCallback;
        ULONG RetVal = Ioctl(IG_DUMP_SYMBOL_INFO, &Sym, Sym.size);
        bVerbose = saveVerbose;
        if (RetVal) {
            Output("Can't get info of %s.%s, error %d(%s)\n", StructName, FieldName, RetVal, Name<eSYMBOL_ERROR>(RetVal));
            return false;
        }

        for (UINT i = 0; i < Fields.GetCount(); ++i) {
            FIELD_INFO& info = Fields[i];
            if (address && !Sym.addr) {
                info.address += address;
            }
            FieldMarkReal(info, Address ? Address->IsReal : false);

            if (fArrayMember) {
                // return original name with [...]
                Fields[i].Name(FieldName);
            }
            bool bUpdateAddress = false;
            if (Fields.GetCount() == 1 && Address) {
                // dereference if this is a pointer
                if (fArrayMember && Fields[i].IsPointer()) {
                    if (!Fields[i].IsReal() || !ReadMemory(info.address, &info.address, sizeof(info.address), NULL)) {
                        info.address = 0;
                        FieldMarkReal(info, false);
                    } else {
                        // dereferenced
                        bUpdateAddress = true;
                    }
                }
                Address->Address = info.address;
                Address->IsReal = Fields[i].IsReal();
            }
            // set the type name
            // if needed: dereference the type and adjust the pointer
            UpdateInfo(Sym.ModBase, Fields[i], fArrayMember, index);
            if (bUpdateAddress) {
                Address->Address = info.address;
            }
        }

        if (*FieldName == 0 && Fields.GetCount() == 0) {
            FIELD_INFO info = {};
            // this field is sometimes set (for ex. PPARANDIS_ADAPTER)
            info.fPointer = Sym.fPointer;
            info.fArray = Sym.fArray;
            info.fConstant = Sym.fConstant;
            info.fStruct = Sym.fStruct;
            info.TypeId = Sym.TypeId;
            info.size = Sym.TypeSize;
            info.fName = (PUCHAR)StructName;
            info.address = address;
            FieldMarkReal(info, Address ? Address->IsReal : false);
            UpdateDescription(Sym.ModBase, info);
            CFieldInfo f(info);
            UpdateInfo(Sym.ModBase, f);
            Fields.Add(f);
        }

        return true;
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

class CDebugExtensionNet : public CDebugExtension
{
public:
    CDebugExtensionNet(PDEBUG_CLIENT Client) : CDebugExtension(Client, "netkvm", "PARANDIS_ADAPTER") {}
    void mp()
    {
        CPtrArray adapters;
        CString version = "Unknown";
        {
            CGetVirtioAdapters cmd(m_Client);
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
                CGetAdapterContext cmd(m_Client, (ULONGLONG)adapters[i]);
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
        for (int i = 0; i < adapters.GetCount(); ++i) {
            ULONG nQueues = 0;
            if (GetAdapterField(adapters[i], "nPathBundles", nQueues)) {
                Output("#%d: %d queues\n", i, nQueues);
                if (adapters.GetCount() == 1) {
                    StaticData.Adapter = adapters[0];
                    Output("Adapter #%d selected\n", i);
                }
            }
        }
    }

    void query(PCSTR Args)
    {
        struct
        {
            PCSTR alias;
            PCSTR field;
        } aliases[] =
        {
            { "queues", "nPathBundles" },
            { "tx0", "pPathBundles[0].txPath" },
            { "tx1", "pPathBundles[1].txPath" },
            { "tx2", "pPathBundles[2].txPath" },
            { "tx3", "pPathBundles[3].txPath" },
        };
        CStringArray params;
        ULONG size = 0;

        Tokenize(Args, " ", params, [](CString& Next) { return true; });
        if (params.GetCount() < 2) {
            Output("!virtio.query <option> <field of %s or global variable>\n", m_MainContext);
            Output("Options:\n");
            Output("   e    evaluate as is\n");
            Output("   g    read global variable\n");
            Output("   t    traverse from type to fields\n");
            Output("   d    read as dword or less\n");
            Output("   p    read as pointer\n");
            Output("   s    get size, offset, type\n");
            Output("   a    get size, offset, type then read\n");
            Output("   l    {path to root entry} - dump list\n");
            Output("   k    {kd data index} [size] - ReadDebuggerData\n");
            return;
        }

        if (!params[0].CompareNoCase("k")) {
            ULONG index = atoi(params[1]);
            size = sizeof(ULONG64);
            if (params.GetCount() > 2) {
                size = atoi(params[2]);
                if (size == 0 || size > sizeof(ULONG64)) {
                    size = sizeof(ULONG64);
                }
            }
            ULONG64 val = 0;
            m_Result = m_DataSpaces->ReadDebuggerData(index, &val, size, NULL);
            if (SUCCEEDED(m_Result)) {
                Output("Data[%d] = %p\n", index, val);
            } else {
                Output("ReadDebuggerData error %X\n", m_Result);
            }
            return;
        }

        if (!StaticData.Adapter && params[0].FindOneOf("sadp") >= 0) {
            Output("Adapter not selected, trying 'mp' first\n");
            mp();
            if (!StaticData.Adapter) {
                Output("Adapter context required for this command\n");
                return;
            }
        }

        CStringArray names;
        CFieldInfo base;
        if (params[1].Find('.') > 0) {
            Tokenize(params[1], ".", names, [](CString& Next) { return true; });
        } else {
            names.Add(params[1]);
        }

        if (!params[0].CompareNoCase("e")) {
            Evaluate(params[1]);
            return;
        }

        if (!params[0].CompareNoCase("s") || !params[0].CompareNoCase("a")) {
            CFieldParser parser;
            CFieldInfo& f = parser.Info();
            names.InsertAt(0, m_MainContext);
            QueryStruct(m_MainContext, base, (ULONG64)StaticData.Adapter);
            TraverseToField(base, names, parser);
            LARGE_INTEGER val = {};
            switch (tolower(params[0].GetAt(0)))
            {
                case 's':
                    return;
                case 'a':
                    if (!f.IsReal() || f.Size() > 8)
                        break;
                    ReadMemory(f.Offset(), &val, f.Size(), NULL);
                    if (f.Size() > 4) {
                        Output("%s = 0x%p\n", f.Name(), (PVOID)val.QuadPart);
                    } else {
                        Output("%s = 0x%x(%d)\n", f.Name(), val.LowPart, val.LowPart);
                    }
                    break;
                default:
                    break;
            }
            return;
        }

        if (!params[0].CompareNoCase("l")) {
            CListParser parser(m_Control3);
            names.InsertAt(0, m_MainContext);
            QueryStruct(m_MainContext, base, (ULONG64)StaticData.Adapter);
            TraverseToField(base, names, parser);
        }

        if (!params[0].CompareNoCase("d") || (size && size <= sizeof(ULONG))) {
            ULONG val = 0;
            GetAdapterField(StaticData.Adapter, params[1], val);
            Output("%s = %d(%X)\n", params[1].GetString(), val, val);
            return;
        }

        if (!params[0].CompareNoCase("p") || size == sizeof(PVOID)) {
            PVOID p = nullptr;
            GetAdapterField(StaticData.Adapter, params[1], p);
            Output("%s = %p\n", params[1].GetString(), p);
            return;
        }

        if (!params[0].CompareNoCase("g") && ResolveSymbol(names[0], base)) {
            TraverseToField(base, names);
        }

        if (!params[0].CompareNoCase("t") && QueryStruct(names[0], base)) {
            TraverseToField(base, names);
        }

        if (size) {
            PVOID p = malloc(size);
            if (GetAdapterField(StaticData.Adapter, params[1], p, size)) {

            }
            free(p);
        }
    }
    void help()
    {
        __super::help();
        Output("mp                  - find miniport and symbols in known places\n");
        Output("query               - read miniport field\n");
    }
    struct CStaticData
    {
        PVOID Adapter = NULL;
        EXCEPTION_RECORD ExcRecords[4];
    };
protected:
    bool GetAdapterField(PVOID Context, LPCSTR Name, PVOID& Value)
    {
        return GetAdapterField(Context, Name, &Value, sizeof(Value));
    }
    bool GetAdapterField(PVOID Context, LPCSTR Name, ULONG& Value)
    {
        return GetAdapterField(Context, Name, &Value, sizeof(Value));
    }
    bool GetAdapterField(PVOID Context, LPCSTR FieldName, PVOID Buffer, ULONG Length)
    {
        LOG("Getting (%p)->%s", Context, FieldName);
        ULONG res;
        GetAdapterDataSafe(Context, FieldName, Buffer, Length, res);
        LOG("result: %X", res);
        if (res) {
            Output("  error %X(%s)\n", res, Name<eSYMBOL_ERROR>(res));
        }
        return !res;
    }
    void GetAdapterDataSafe(PVOID Context, LPCSTR FieldName, PVOID Buffer, ULONG Length, ULONG& Result)
    {
        __try {
            GetDataFromContextField(Context, FieldName, Buffer, Length, Result);
        }
        __except (FilterException(GetExceptionInformation()->ExceptionRecord)) {
            Result = GetExceptionCode();
        }
    }
    static ULONG FilterException(PEXCEPTION_RECORD ERec)
    {
        RtlZeroMemory(&StaticData.ExcRecords, sizeof(StaticData.ExcRecords));
        for (int i = 0; i < RTL_NUMBER_OF(StaticData.ExcRecords); ++i) {
            StaticData.ExcRecords[i] = *ERec;
            if (!ERec->ExceptionRecord) break;
            ERec = ERec->ExceptionRecord;
        }
        return EXCEPTION_EXECUTE_HANDLER;
    }
    static CStaticData StaticData;
};

// CDebugExtensionNet static data
CDebugExtensionNet::CStaticData CDebugExtensionNet::StaticData;

class CDebugExtensionHv : public CDebugExtension
{
public:
    CDebugExtensionHv(PDEBUG_CLIENT Client) : CDebugExtension(Client, "nt", "DUMMY") {}
    template <typename VALTYPE> bool GetValue(LPCSTR Name, VALTYPE& Val)
    {
        CFieldInfo info;
        if (ResolveSymbol(Name, info)) {
            ULONG read = 0;
            ReadMemory(info.Offset(), &Val, sizeof(Val), &read);
            if (sizeof(Val) == read) {
                Output("%s = %X\n", Name, Val);
                return true;
            }
        }
        return false;
    }
    template <typename VALTYPE, typename FLAGTYPE> void DumpFlags(LPCSTR Name)
    {
        VALTYPE flags = 0;
        if (GetValue<VALTYPE>(Name, flags)) {
            auto table = FlagsTable<FLAGTYPE>(flags);
            Output("%s", table.GetString());
        }
    }
    void Dump()
    {
        CIommuDetect d(m_Client);
        bool b = d.Present();
        Output("IOMMU %s\n", b ? "present" : "not present");

        DumpFlags<ULONG, eHV_FLAG_TYPE>("HvlpFlags");
        DumpFlags<ULONG, eHV_ENLIGHTMENT_TYPE>("HvlpEnlightenments");
        ULONG rootFlags = 0;
        GetValue<ULONG>("HvlpRootFlags", rootFlags);
        UCHAR isSecureKernel = 0;
        GetValue<UCHAR>("VslVsmEnabled", isSecureKernel);
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

extern "C" __declspec(dllexport) HRESULT query(IN PDEBUG_CLIENT Client, IN PCSTR Args)
{
    VERBOSE("%s: =>", __FUNCTION__);
    CDebugExtensionNet e(Client);
    e.query(Args);
    VERBOSE("%s: <=", __FUNCTION__);
    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT hv(IN PDEBUG_CLIENT Client, IN PCSTR Args)
{
    VERBOSE("%s: =>", __FUNCTION__);
    CDebugExtensionHv e(Client);
    e.Dump();
    VERBOSE("%s: <=", __FUNCTION__);
    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT cn(IN PDEBUG_CLIENT Client, IN PCSTR Args)
{
    VERBOSE("%s: =>", __FUNCTION__);
    CDebugExtensionNet e(Client);
    e.cn();
    return S_OK;
}


#if !THIS_IS_WINDBG_EXTENSION

extern "C" __declspec(dllexport) HRESULT CALLBACK DebugExtensionInitialize(
    OUT PULONG Version,
    OUT PULONG Flags)
{
    *Version = DEBUG_EXTENSION_VERSION(0, 1);
    *Flags = 0;
    LOG("%s: =>", __FUNCTION__);
    return S_OK;
}

extern "C" __declspec(dllexport) void CALLBACK DebugExtensionUninitialize()
{
    LOG("%s: =>", __FUNCTION__);
}

#else // THIS_IS_WINDBG_EXTENSION

extern "C" __declspec(dllexport)LPEXT_API_VERSION WDBGAPI ExtensionApiVersion()
{
    static EXT_API_VERSION v = { 1, 0, EXT_API_VERSION_NUMBER64, 0 };
    return &v;
}

extern "C" __declspec(dllexport) void WDBGAPI WinDbgExtensionDllInit(
    PWINDBG_EXTENSION_APIS lpExtensionApis,
    USHORT MajorVersion,
    USHORT MinorVersion)
{
    LOG("%s: %d.%d =>", __FUNCTION__, MajorVersion, MinorVersion);
    ULONG toCopy = min(lpExtensionApis->nSize, sizeof(ExtensionApis));
    RtlCopyMemory(&ExtensionApis, lpExtensionApis, toCopy);
    ExtensionApis.nSize = sizeof(ExtensionApis);
    dprintf("WinDbg extension API active\n");
}

#endif

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
