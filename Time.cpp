#include "stdafx.h"

class CTimeHandler : public CCommandHandler
{
public:
    CTimeHandler() : CCommandHandler("time", "time related operations", 1) {}
private:
    void ReportPerfCounter();
    void Measure(const CStringArray& Parameters);
    int Run(const CStringArray& Parameters) override
    {
        if (!Parameters[0].CompareNoCase("perf")) {
            ReportPerfCounter();
            return 0;
        } else {
            Measure(Parameters);
            return 0;
        }
    }
    void Help(CStringArray& a) override
    {
        a.Add("\"<command line to run>\"\tRun command line and report execution time");
        a.Add("perf\t\t\tReport performance counter resolution");
    }
};

void CTimeHandler::ReportPerfCounter()
{

}

void CTimeHandler::Measure(const CStringArray& Parameters)
{
    CString s = Parameters[0];
    for (UINT i = 1; i < Parameters.GetCount(); ++i) {
        s += ' ';
        s += Parameters[i];
    }

    LOG("Running %s", s.GetString());
    ULONG t1 = GetTickCount();
    int res = system(s);
    ULONG t2 = GetTickCount();
    LOG("Done(%d) in %d ms", res, t2 - t1);
}

static CTimeHandler timeh;
