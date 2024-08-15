#include "stdafx.h"
#include <WinHvPlatform.h>
#include "winternl.h"

#pragma comment(lib, "ntdll.lib")

class CHvFeatures : public CCommandHandler
{
public:
    CHvFeatures() : CCommandHandler("hv", "present Hyper-V features", 1)
    {
    }
private:
    int Run(const CStringArray& Parameters) override
    {
        GetCapabilities();
        return 0;
    }
    void Help(CStringArray& a) override
    {
        a.Add("<feature>");
    }
    void GetCapabilities();
};

void CHvFeatures::GetCapabilities()
{
}

static CHvFeatures hvf;
