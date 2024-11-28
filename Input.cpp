#include "stdafx.h"
#include <conio.h>

typedef BYTE tKeys[256];

class CThreadProc : public CThreadOwner
{
public:
    CThreadProc(void (*Proc)(const CThreadOwner&) = nullptr) : m_Process(Proc) {}
    void ThreadProc() override
    {
        m_Process(*this);
    }
    void (*m_Process)(const CThreadOwner&) = NULL;
};

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

class CInputHandler : public CCommandHandler
{
public:
    CInputHandler() : CCommandHandler("input", "input related operations", 1)
    {
        DeclareCommand("kbd", MonitorKeys, 0, 0);
        DeclareCommand("mouse", MonitorMouse, 0, 0);
    }
private:
    static int MonitorKeys(const CStringArray& Parameters);
    static int MonitorMouse(const CStringArray& Parameters);
    void Help(CStringArray& a) override
    {
        a.Add("kbd\t\tReport pressed keys");
        a.Add("mouse\t\tReport mouse position");
    }
};

int CInputHandler::MonitorKeys(const CStringArray& Parameters)
{
    CThreadProc t(KbdProcess);
    if (!t.StartThread())
        return 1;
    puts("Hit ENTER to end the keyboard monitoring");
    while (true) {
        int c = _getch();
        if (c == '\r')
            break;
    }
    Sleep(100);

    t.StopThread();
    return 0;
}

int CInputHandler::MonitorMouse(const CStringArray& Parameters)
{
    // not implemented yet
    puts("not implemented");
    return 0;
}

static CInputHandler ih;