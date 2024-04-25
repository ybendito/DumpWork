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
        a.Add("<dump file>");
    }
};

int CKd::Run(const CStringArray& Parameters)
{
    SetConsoleCtrlHandler(HandlerRoutine, true);

    CString prog = "c:\\Program Files (x86)\\Windows Kits\\10\\Debuggers\\x64\\kd.exe";
    CString cmd;
    cmd.Format("\"%s\"", prog.GetString());
    // add dump file and output file
    cmd.AppendFormat(" -z \"%s\" -logo \"%s.kd.log\"", Parameters[0].GetString(), Parameters[0].GetString());
    CProcessRunner r(false);
    r.RunProcess(cmd);
    //LOG("Done %s", cmd.GetString());

    return 0;
}

static CKd kd;
