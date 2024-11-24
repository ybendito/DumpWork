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
        }
        else if (!Parameters[0].CompareNoCase("mm")) {
            UINT value = atoi(Parameters[1]);
            LOG("starting timer period for resolution of %d ms", value);
            ULONG err = timeBeginPeriod(value);
            if ( err == TIMERR_NOERROR) {
                puts("Hit ENTER to end the timer period");
                getchar();
                timeEndPeriod(value);
            } else {
                LOG("failed, error %d", err);
                TIMECAPS caps;
                if (timeGetDevCaps(&caps, sizeof(caps)) == TIMERR_NOERROR) {
                    LOG("possible values %d ... %d", caps.wPeriodMin, caps.wPeriodMax);
                }
            }
        } else {
            Measure(Parameters);
        }
        return 0;
    }
    void Help(CStringArray& a) override
    {
        a.Add("\"<command line to run>\"\tRun command line and report execution time");
        a.Add("perf\t\t\tReport performance counter resolution");
        a.Add("mm <millies>\t\tStart multimedia timer");
    }
};

void CTimeHandler::ReportPerfCounter()
{
    LARGE_INTEGER li;
    QueryPerformanceFrequency(&li);
    LOG("Perf counter frequency %I64d", li.QuadPart);

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
