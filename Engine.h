#pragma once

class CCommandHandler
{
public:
    CCommandHandler(LPCSTR Token, LPCSTR Description, UINT MinParams) :
        m_Token(Token),
        m_Description(Description),
        m_MinParams(MinParams)
    {
        Register(this);
    }
    virtual ~CCommandHandler() {}
    virtual void Help(CStringArray&) = 0;
    virtual int Run(const CStringArray& Parameters) = 0;
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
private:
    static void Register(CCommandHandler *Handler)
    {
        m_Handlers.Add(Handler);
    }
    CString m_Token;
    CString m_Description;
    UINT    m_MinParams;
public:
    static CArray<CCommandHandler*> m_Handlers;
};

struct tConfiguration
{
    bool Help;
    bool Wait;
    bool Loop;
    UINT Count;
    static tConfiguration m_ConfigurationForCfgOnly;
};

FORCEINLINE const tConfiguration& Config() { return tConfiguration::m_ConfigurationForCfgOnly;  }