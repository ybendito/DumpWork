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
        m_Client.QueryInterface<IDebugControl>(&m_Control);
        m_Client->GetOutputCallbacks(&m_Previous);
        m_Client->SetOutputCallbacks(this);
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
    CComPtr<IDebugClient> m_Client;
    CComPtr<IDebugControl> m_Control;
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
    CExternalCommandParser(PDEBUG_CLIENT Client, LPCSTR Command) :
        CDebugOutputCallback(Client),
        m_Command(Command)
    {
        // at this point the output callback is set, previous one is saved
    }
    virtual HRESULT Run()
    {
        bool saveVerbose = bVerbose;
        if (m_Verbose) {
            bVerbose = true;
        }
        HRESULT res = m_Control->Execute(DEBUG_OUTPUT_NORMAL, m_Command, DEBUG_EXECUTE_NO_REPEAT);
        if (m_Verbose) {
            bVerbose = saveVerbose;
        }
        return res;
    }
protected:
    CString m_Command;
    void BeVerbose() { m_Verbose = true; }
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
            if (m_Output[i].Find(".dmp")) {
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

typedef CArray<CFieldInfo> CFieldsArray;

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
        ULONG remaindexIndex = 0;
        m_Result = m_Control->Evaluate(Expression, DEBUG_VALUE_INVALID, &v, &remaindexIndex);
        if (FAILED(m_Result)) {
            Output("evaluation failed, error %X, index %d\n", m_Result, remaindexIndex);
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
        Output("findpdb <directory> - recursive find and append to symbol path\n");
        verbose(m_Client, "--help");
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
    void GetTypeName(ULONG64 moduleBase, ULONG typeId, CString& Name)
    {
        char buffer[1024];
        if (FAILED(m_Symbols->GetTypeName(moduleBase, typeId, buffer, sizeof(buffer), nullptr))) {
            Name = "hard to say>";
        } else {
            Name = buffer;
        }
    }
    bool ResolveSymbol(LPCSTR Name, ULONG& Size, CString& Type, ULONG64& Offset)
    {
        CString sFullName;
        ULONG64 moduleBase;
        ULONG typeId;
        if (strchr(Name, '!')) {
            sFullName = Name;
        }
        else {
            sFullName.Format("%s!%s", m_Module.GetString(), Name);
        }
        m_Result = m_Symbols->GetOffsetByName(sFullName, &Offset);
        if (FAILED(m_Result)) {
            Output("Can't find %s, error %X\n", sFullName.GetString(), m_Result);
            return false;
        }
        m_Result = m_Symbols->GetSymbolTypeId(sFullName, &typeId, &moduleBase);
        if (FAILED(m_Result)) {
            Type = "<unknown>";
            return true;

        }
        GetTypeName(moduleBase, typeId, Type);

        m_Result = m_Symbols->GetTypeSize(moduleBase, typeId, &Size);
        if (FAILED(m_Result)) {
            Output("Can't get size of %, error %X\n", Name, m_Result);
        }
        return true;
    }
protected:
    CComPtr<IDebugControl> m_Control;
    CComPtr<IDebugClient>  m_Client;
    CComPtr<IDebugSymbols> m_Symbols;
    CComPtr<IDebugSymbols3> m_Symbols3;
    CComPtr<IDebugControl3> m_Control3;
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
        Result = GetFieldData((ULONG_PTR)Context, m_MainContext, FieldName, Length, Buffer);
    }
    bool GetContextFieldProperties(LPCSTR FieldName, CFieldInfo& Info)
    {
        return GetStructFieldProperties(m_MainContext, FieldName, Info);
    }
    bool GetStructFieldProperties(LPCSTR StructName, LPCSTR FieldName, CFieldInfo& Info)
    {
        return QueryStructField(
            StructName,
            FieldName,
            [&](const CFieldInfo& Field) { Info = Field; });
    }

    template <typename TCallback> bool QueryStructField(
        LPCSTR StructName,
        LPCSTR FieldName,
        TCallback Callback)
    {
        CArray<CFieldInfo> fields;

        PSYM_DUMP_FIELD_CALLBACK callback =
            [](FIELD_INFO* Field, PVOID UserContext) -> ULONG
        {
            decltype(fields)* pFields = (decltype(fields)*)UserContext;
            LOG("Field callback called for %s", Field->fName);
            CFieldInfo f(*Field);
            pFields->Add(f);
            return 0;
        };

        FIELD_INFO flds = {};
        flds.fName = (PUCHAR)FieldName;
        flds.fOptions = DBG_DUMP_FIELD_RETURN_ADDRESS;
        flds.fieldCallBack = callback;

        SYM_DUMP_PARAM Sym = {};
        Sym.size = sizeof(Sym);
        Sym.sName = (PUCHAR)StructName;
        Sym.Options = DBG_DUMP_NO_PRINT | DBG_DUMP_CALL_FOR_EACH;
        Sym.Context = &fields;
        Sym.nFields = 1;
        Sym.Fields = &flds;

        if (*FieldName == '*') {
            Sym.nFields = 0;
            Sym.CallbackRoutine = callback;
        }

        ULONG RetVal = Ioctl(IG_DUMP_SYMBOL_INFO, &Sym, Sym.size);
        if (RetVal) {
            Output("Can't get info of %s.%s, error %d(%s)\n", FieldName, StructName, RetVal, Name<eSYMBOL_ERROR>(RetVal));
            return false;
        }

        for (UINT i = 0; i < fields.GetCount(); ++i) {
            CString type;
            GetTypeName(Sym.ModBase, FIELD_INFO(fields[i]).TypeId, type);
            fields[i].Type(type);
            Callback(fields[i]);
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
        if (!StaticData.Adapter) {
            Output("Adapter not selected, use 'mp' first\n");
            return;
        }
        struct
        {
            PCSTR alias;
            PCSTR field;
        } aliases[] =
        {
            { "queues", "nPathBundles" },
            { "tx0", "pPathBundles->txPath" },
        };
        CStringArray params;
        ULONG size = 0;

        Tokenize(Args, " ", params, [](CString& Next) { return true; });
        if (params.GetCount() < 2) {
            Output("!virtio.query <option> <field of %s or global variable>\n", m_MainContext);
            Output("Options:\n");
            Output("   e    evaluate as is\n");
            Output("   g    read global variable\n");
            Output("   d    read as dword or less\n");
            Output("   p    read as pointer\n");
            Output("   s    get size, offset, type\n");
            return;
        }

        if (!params[0].CompareNoCase("e")) {
            Evaluate(params[1]);
            return;
        }

        if (!params[0].CompareNoCase("s") || !params[0].CompareNoCase("a")) {
            CFieldInfo info;
            if (!GetContextFieldProperties(params[1], info)) {
                return;
            }
            Output("%s @%p, size %d, type %s (%s)\n",
                params[1].GetString(),
                (PVOID)(info.Offset() + (ULONG64)StaticData.Adapter),
                info.Size(),
                info.Type(),
                info.Description());
            size = info.Size();
            if (!params[0].CompareNoCase("s")) {
                return;
            }
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

        if (!params[0].CompareNoCase("g") && m_Symbols) {
            ULONG64 offset = 0;
            CString type;
            CStringArray names;
            if (params[1].Find('.') > 0) {
                Tokenize(params[1], ".", names, [](CString& Next) { return true; });
            } else {
                names.Add(params[1]);
            }
            if (ResolveSymbol(names[0], size, type, offset)) {
                Output("%s: %p of %d(%s)\n", names[0].GetString(), (PVOID)offset, size, type.GetString());
                CString combined = names[0];
                for (UINT i = 1; i < names.GetSize(); ++i) {
                    LOG("Looking for %s.%s", type.GetString(), names[i].GetString());
                    bool multiple = *names[i].GetString() == '*';
                    QueryStructField(
                        type,
                        names[i],
                        [&](const CFieldInfo& F)
                        {
                            const FIELD_INFO* f = F;
                            // address is an offset in the parent structure
                            if (!multiple) {
                                offset += f->address;
                                size = f->size;
                                combined += '.';
                                combined += (LPCSTR)f->fName;
                                type = F.Type();
                                Output("%s @%p size %d, type %s (%s)\n",
                                    combined.GetString(),
                                    (PVOID)offset,
                                    f->size,
                                    F.Type(),
                                    F.Description());
                            }
                            else
                            {
                                Output("%s.%s offset @%d size %d, type %s (%s)\n",
                                    combined.GetString(),
                                    (LPCSTR)f->fName,
                                    (ULONG)f->address,
                                    f->size,
                                    F.Type(),
                                    F.Description());
                            }
                        }
                    );
                }
            }
            return;
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
        Output("mp - find miniport and symbols in known places\n");
        Output("query - read miniport field\n");
        __super::help();
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
        if (!HasEAPI())
            return false;
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
