#include "stdafx.h"

CArray<CCommandHandler*> CCommandHandler::m_Handlers;

tConfiguration tConfiguration::m_ConfigurationForCfgOnly;

static int Usage()
{
    auto& handlers = CCommandHandler::m_Handlers;
    puts("DumpWork: various utilities");
    for (UINT i = 0; i < handlers.GetSize(); ++i) {
        auto h = handlers[i];
        printf("%s\t\t\t%s\n", h->Token().GetString(), h->Description().GetString());
    }
    return 1;
}

static bool FindValue(const CString& Param, LPCSTR key, UINT& Value)
{
    CString prefix = key;
    prefix += ':';
    if (Param.Find(prefix) != 0)
        return false;
    CString sVal = Param.Right(Param.GetLength() - prefix.GetLength());
    Value = atoi(sVal);
    CString sVal2;
    sVal2.Format("%d", Value);
    if (!sVal2.Compare(sVal))
        return true;
    LOG("Invalid format: value %s", Param.GetString());
    return false;
}

static void ProcessConfig(CStringArray& a)
{
    tConfiguration& cfg = tConfiguration::m_ConfigurationForCfgOnly;
    for (UINT i = 0; i < a.GetCount(); ++i) {
        auto& s = a[i];
        s.MakeLower();
        if (s[0] == '?' || s.Find("help") == 0) {
            cfg.Help = true;
        } else if (s.Find("wait") == 0) {
            cfg.Wait = true;
        } else if (s.Find("loop") == 0) {
            cfg.Loop = true;
        } else if (FindValue(s, "count", cfg.Count)) {
            // ok
        } else {
            LOG("Unrecognized config %s", s.GetString());
        }
    }
}

int main(int argc, char **argv)
{
    CStringArray params;
    CStringArray config;
    for (int i = 1; i < argc; ++i) {
        CString s = argv[i];
        if (s[0] == '/' || s[0] == '-') {
            s.Delete(0);
            config.Add(s);
        } else if (s[0] == '?') {
            config.Add("?");
        } else {
            params.Add(s);
        }
    }
    ProcessConfig(config);
    if (params.IsEmpty()) {
        return Usage();
    }
    auto& handlers = CCommandHandler::m_Handlers;
    for (UINT i = 0; i < handlers.GetCount(); ++i) {
        auto h = handlers[i];
        CString token = params[0];
        if (!h->Token().CompareNoCase(token)) {
            params.RemoveAt(0);
            UINT paramSize = (UINT)params.GetCount();

            if (Config().Help) {
                return h->Usage();
            } else if (h->MinParams() <= paramSize) {
                return h->Run(params);
            } else {
                ERR("%s requires minimum of %d parameters", token.GetString(), h->MinParams());
                return h->Usage();
            }
        }
    }
    return Usage();
}
