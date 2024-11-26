#include "stdafx.h"

typedef BYTE tKeys[256];

class CInputHandler : public CCommandHandler, public CThreadOwner
{
public:
    CInputHandler() : CCommandHandler("input", "input related operations", 1) {}
private:
    void MonitorKeys();
    void MonitorMouse();
    int Run(const CStringArray& Parameters) override
    {
        if (!Parameters[0].CompareNoCase("kbd")) {
            MonitorKeys();
        } else if (!Parameters[0].CompareNoCase("mouse")) {
            MonitorMouse();
        }
        return 0;
    }
    void Help(CStringArray& a) override
    {
        a.Add("kbd\t\t\tReport pressed keys");
        a.Add("mouse\t\tReport mouse position");
    }
    virtual void ThreadProc();
    void (*m_Process)(const CThreadOwner&) = NULL;
};

void CInputHandler::ThreadProc()
{
    m_Process(*this);
}

CString KeyName(UINT ScanCode)
{
    CString s;
    char buf[64];
    if (ScanCode) {
        LONG param = ScanCode << 16;
        if (GetKeyNameText(param, buf, ARRAYSIZE(buf))) {
            s = buf;
        }
    }
    return s;
}

static void Compare(tKeys& Old, tKeys& New)
{
    for (UINT i = 0; i < ARRAYSIZE(New); ++i) {
        if (New[i] == Old[i])
            continue;
        LONG scan = MapVirtualKey(i, MAPVK_VK_TO_VSC);
        CString s = KeyName(scan);
        const char* sp = New[i] ? "p" : "unp";
        if (s.IsEmpty()) s = "Unknown";
        LOG("Virtual key %d(%s),scan %d %sressed", i, s.GetString(), scan, sp);
    }
}

static void KbdProcess(const CThreadOwner& o)
{
    tKeys current = {}, prev = {};
    auto GetState = [](tKeys& keys) {
        for (UINT i = 0; i < ARRAYSIZE(current); ++i) {
            SHORT val = GetAsyncKeyState(i);
            keys[i] = (val & 0x8000) || val;
        }
    };
    // flush queued keyboard events
    for (UINT i = 0; i < 10; ++i) { GetState(prev); }
    while (o.ShouldContinueRunning()) {
        GetState(current);
        Compare(prev, current);
        memcpy(prev, current, sizeof(prev));
    }
}

void CInputHandler::MonitorKeys()
{
    m_Process = KbdProcess;
    if (!StartThread())
        return;
    puts("Hit ENTER to end the keyboard monitoring");
    getchar();
    StopThread();
}

void CInputHandler::MonitorMouse()
{
    // not implemented yet
}

static CInputHandler ih;