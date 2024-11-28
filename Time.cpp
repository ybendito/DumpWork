#include "stdafx.h"

class tBeginTime
{
public:
    tBeginTime(ULONG Resolution) : m_Resolution(Resolution)
    {
        m_Error = timeBeginPeriod(m_Resolution);
    }
    ~tBeginTime()
    {
        if (!m_Error)
            timeEndPeriod(m_Resolution);
    }
    ULONG Error() const { return m_Error; }
private:
    ULONG m_Resolution;
    ULONG m_Error = 0;
};

class CTimeHandler : public CCommandHandler
{
public:
    CTimeHandler() : CCommandHandler("time", "time related operations", 1)
    {
        DeclareCommand("perf", ReportPerfCounter, 0, 0);
        DeclareCommand("wait", DoWait, 1, fMoreOK);
        DeclareCommand("mm", BeginEnd, 1, 0);
        DeclareCommand("anything", Measure, 1, fMatchesAll | fMoreOK);
    }
private:
    static int ReportPerfCounter(const CStringArray& Parameters);
    static int DoWait(const CStringArray& Parameters);
    static int Measure(const CStringArray& Parameters);
    static int BeginEnd(const CStringArray& Parameters);
    void Help(CStringArray& a) override
    {
        a.Add("\"<command line to run>\"\tRun command line and report execution time");
        a.Add("perf\t\t\tReport performance counter resolution");
        a.Add("mm <millies>\t\tStart multimedia timer period or 0 for query");
        a.Add("wait <millies> [resolution=1]\t\twait with increased resolution, 0 for default");
    }
};

int CTimeHandler::ReportPerfCounter(const CStringArray& Parameters)
{
    LARGE_INTEGER li;
    QueryPerformanceFrequency(&li);
    LOG("Perf counter frequency %I64d", li.QuadPart);
    return 0;
}

int CTimeHandler::Measure(const CStringArray& Parameters)
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
    return res;
}

int CTimeHandler::BeginEnd(const CStringArray& Parameters)
{
    UINT value = atoi(Parameters[0]);
    LOG("starting timer period for resolution of %d ms", value);
    tBeginTime begin(value);
    if (!begin.Error()) {
        puts("Hit ENTER to end the timer period");
        getchar();
    } else {
        LOG("failed, error %d", begin.Error());
        TIMECAPS caps;
        if (timeGetDevCaps(&caps, sizeof(caps)) == TIMERR_NOERROR) {
            LOG("possible values %d ... %d", caps.wPeriodMin, caps.wPeriodMax);
        }
    }

    return 0;
}

int CTimeHandler::DoWait(const CStringArray& Parameters)
{
    LARGE_INTEGER li, pt1, pt2;
    QueryPerformanceFrequency(&li);
    UINT value = atoi(Parameters[0]);
    UINT resolution = 1;
    if (Parameters.GetCount() > 1) {
        resolution = atoi(Parameters[1]);
    }
    LOG("sleeping for %d ms", value);
    tBeginTime begin(resolution);
    QueryPerformanceCounter(&pt1);
    Sleep(value);
    QueryPerformanceCounter(&pt2);
    LOG("done in %d ms", (ULONG)(((pt2.QuadPart - pt1.QuadPart) * 1000) / li.QuadPart));
    return 0;
}


static CTimeHandler timeh;
