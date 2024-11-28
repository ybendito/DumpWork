#pragma once

using tSubcommandProcessor = int (*)(const CStringArray& Parameters);

typedef struct tagCCommandsTableEntry
{
    LPCSTR token;
    tSubcommandProcessor proc;
    UINT minNumberOfParams;
    bool variableNumberOfParams;
    bool matchesAll;
} CCommandsTableEntry;

using CCommandsTable = CArray<CCommandsTableEntry>;

class CCommandHandler
{
public:
    enum Flags { fMoreOK = 1, fMatchesAll = 2 };
    bool HasCommands() const { return m_CommandTable.GetCount(); }
    int ProcessCommand(CStringArray& Parameters)
    {
        CString token;
        if (Parameters.GetCount() == 0) {
            Parameters.Add(CString(""));
        }
        token = Parameters[0];
        for (UINT i = 0; i < m_CommandTable.GetCount(); ++i) {
            CCommandsTableEntry& e = m_CommandTable.GetAt(i);
            if (!token.CompareNoCase(e.token) || e.matchesAll) {
                if (!e.matchesAll) {
                    Parameters.RemoveAt(0);
                }
                UINT nofParams = (UINT)Parameters.GetCount();
                bool nofOK = nofParams == e.minNumberOfParams;
                if (!nofOK) {
                    nofOK = nofParams >= e.minNumberOfParams && e.variableNumberOfParams;
                }
                if (nofOK)
                    return e.proc(Parameters);

                ERR("%s %s requires minimum of %d parameters, provided %d",
                        m_Token.GetString(),
                        token.GetString(),
                        e.minNumberOfParams,
                        nofParams);
            }
        }
        return Usage();
    }
    virtual ~CCommandHandler() {}
    virtual void Help(CStringArray&) = 0;
    virtual int Run(const CStringArray& Parameters)
    {
        printf("%s:The Run method is not implemented\n", m_Token.GetString());
        return 1;
    }
    int Usage()
    {
        CStringArray help;
        Help(help);
        puts(m_Description);
        switch (help.GetCount())
        {
            case 0:
                printf("%s\t(No command-specific help available)\n", m_Token.GetString());
                break;
            case 1:
                printf("%s\t%s\n", m_Token.GetString(), help[0].GetString());
                break;
            default:
                printf("%s ...\n", m_Token.GetString());
                for (UINT i = 0; i < help.GetCount(); ++i) {
                    CString s;
                    s += '\t';
                    s += help.GetAt(i);
                    puts(s);
                }
                break;
        }
        return 1;
    }
    static const CArray<CCommandHandler*>& Handlers()
    {
        return m_Handlers;
    }
    const CString& Token() const { return m_Token; }
    const CString& Description() const { return m_Description; }
    UINT MinParams() const { return m_MinParams; }
protected:
    CCommandHandler(LPCSTR Token, LPCSTR Description, UINT MinParams) :
        m_Token(Token),
        m_Description(Description),
        m_MinParams(MinParams)
    {
        Register(this);
    }
    void DeclareCommand(LPCSTR Token, tSubcommandProcessor Proc, UINT NumberOfParams, ULONG Flags)
    {
        CCommandsTableEntry e;
        e.token = Token;
        e.proc = Proc;
        e.minNumberOfParams = NumberOfParams;
        e.variableNumberOfParams = Flags & fMoreOK;
        e.matchesAll = Flags & fMatchesAll;
        m_CommandTable.Add(e);
    }
private:
    static void Register(CCommandHandler *Handler)
    {
        m_Handlers.Add(Handler);
    }
    CString m_Token;
    CString m_Description;
    UINT    m_MinParams;
    CCommandsTable m_CommandTable;
public:
    static CArray<CCommandHandler*> m_Handlers;
};

struct tConfiguration
{
    bool Help;
    bool Wait;
    bool Loop;
    bool Gui;
    UINT Count;
    static tConfiguration m_ConfigurationForCfgOnly;
};

FORCEINLINE const tConfiguration& Config() { return tConfiguration::m_ConfigurationForCfgOnly;  }