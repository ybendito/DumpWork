#include "stdafx.h"
#include <Pdh.h>
#include <PdhMsg.h>
#include <conio.h>

#pragma comment(lib, "pdh.lib")

class CPdhCounter
{
public:
    CPdhCounter() { }
    CPdhCounter(PDH_HQUERY Query, LPCSTR Name, PDH_STATUS& Status) :
        m_Query(Query),
        m_Name(Name)
    {
        m_Status = PdhAddCounter(m_Query, Name, NULL, &m_Counter);
        if (m_Status) {
            ERR("Can't add counter %s, error 0x%X", Name, m_Status);
        } else {
            LOG("Added counter %s", Name);
        }
        Status = m_Status;
    }
    bool Get(long& Value)
    {
        if (!m_Counter)
            return false;
        PDH_FMT_COUNTERVALUE val1 = {};
        PDH_FMT_COUNTERVALUE val2 = {};
        ULONG fmt = PDH_FMT_LONG | PDH_FMT_NOSCALE;
        m_Status = PdhGetFormattedCounterValue(m_Counter, fmt, NULL, &val1);
        if (!m_Status) {
            fmt |= PDH_FMT_NOCAP100;
            m_Status = PdhGetFormattedCounterValue(m_Counter, fmt, NULL, &val2);
        }
        else {
            ERR("Getting counter %s, error 0x%X", m_Name.GetString(), m_Status);
        }
        if (!m_Status) {
            //LOG("Value: trimmed %f, correct %f", val1.doubleValue, val2.doubleValue);
            LOG("%s: trimmed %d, correct %d", m_Name.GetString(), val1.longValue, val2.longValue);
            Value = val2.longValue;
        }
        return !m_Status;
    }
protected:
    PDH_HQUERY m_Query = NULL;
    PDH_HCOUNTER m_Counter = NULL;
    CString m_Name;
    PDH_STATUS m_Status = ERROR_NOT_READY;
};

class CPdhQuery
{
public:
    CPdhQuery()
    {
        PdhOpenQuery(NULL, (ULONG_PTR)this, &m_Query);
    }
    bool Add(LPCSTR Counter)
    {
        PDH_STATUS st = 0;
        CString s;
        if (*Counter == '\\') {
            s = Counter;
        } else {

            s.Format("\\Process(%s)\\%% Processor Time", Counter);
        }
        CPdhCounter counter(m_Query, s, st);
        if (!st)
            m_Counters.Add(counter);

        return !st;
    }
    bool PollQ()
    {
        PDH_STATUS st = PdhCollectQueryData(m_Query);
        if (st) {
            ERR("Query, error 0x%X", st);
        }
        return !st;
    }
    bool Poll(long *Values, ULONG Size)
    {
        bool res = true;
        if (!PollQ()) {
            return false;
        }
        for (ULONG i = 0; i < m_Counters.GetCount(); ++i) {
            long val = 0;
            res = m_Counters[i].Get(val);
            if (res && i < Size) {
                Values[i] = val;
            }
        }
        return res;
    }
    ~CPdhQuery()
    {
        if (m_Query) {
            PdhCloseQuery(m_Query);
        }
    }
protected:
    PDH_HQUERY m_Query = NULL;
    CArray<CPdhCounter> m_Counters;
};

static void RunAction(int argc, char** argv)
{
    if (argc < 3) {
        LOG("nothing to run");
        return;
    }
    CString cmd;
    cmd.Format(argv[2], argv[0]);
    for (int i = 3; i < argc; ++i) {
        cmd += ' ';
        CString arg;
        arg.Format(argv[i], argv[0]);
        cmd += arg;
    }
    LOG("running %s", cmd.GetString());
    system(cmd);
}

int PerfCounter(int argc, char** argv)
{
    ULONG timeToSleep = 100;
    if (argc < 1 || !_stricmp(argv[0], "help")) {
        puts("@1     Counter for monitoring");
        puts("       or executable name without extension");
        puts("@2     Limit to stop monitoring (decimal)");
        puts("@3...  Command and parameters to run");
        puts("       Use %s for @1");
        return 1;
    }
    CPdhQuery q;
    long val = 0;
    long limit = -1;
    if (!q.Add(argv[0])) {
        return 1;
    }

    if (!q.PollQ()) {
        return 1;
    }
    Sleep(timeToSleep);
    if (argc >= 2) {
        limit = atoi(argv[1]);
    }
    if (limit >= 0) {
        LOG("Running till value < %d", limit);
    }
    puts("Hit Escape to stop...");
    while (q.Poll(&val, 1) && !_kbhit() )
    {
        if (limit >= 0 && val >= limit) {
            RunAction(argc, argv);
            break;
        }
        Sleep(timeToSleep);
    }
    return 0;
}
