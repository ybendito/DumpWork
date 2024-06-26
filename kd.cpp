#include "stdafx.h"

static BOOL WINAPI HandlerRoutine(_In_ DWORD dwCtrlType)
{
    return true;
}

class CKd : public CCommandHandler
{
public:
    CKd() : CCommandHandler("kd", "run KD on dump file", 1) {}
private:
    int Run(const CStringArray& Parameters) override;
    void Help(CStringArray& a) override
    {
        a.Add("[-gui] <dump file>");
    }
};

int CKd::Run(const CStringArray& Parameters)
{
    SetConsoleCtrlHandler(HandlerRoutine, true);

    CString dir = "c:\\Program Files (x86)\\Windows Kits\\10\\Debuggers\\x64\\";
    CString exe = Config().Gui ? "windbg.exe" : "kd.exe";
    CString prog = dir + exe;
    CString cmd;
    cmd.Format("\"%s\"", prog.GetString());
    // add dump file and output file
    cmd.AppendFormat(" -z \"%s\" -logo \"%s.kd.log\"", Parameters[0].GetString(), Parameters[0].GetString());
    const char* env_var = "_NT_SYMBOL_PATH";
    // env_var = "_NT_ALT_SYMBOL_PATH"; //this would do the same
    CString sValue = "srv*;";
    sValue += DirectoryOf(Parameters[0]);
    SetEnvironmentVariable(env_var, sValue);
    CProcessRunner r(false);
    r.RunProcess(cmd);
    //LOG("Done %s", cmd.GetString());

    return 0;
}

static CKd kd;
