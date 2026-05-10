#include "stdafx.h"
#include <intrin.h>

static void do_cpuid(unsigned int leaf, unsigned int subleaf,
    unsigned int regs[4]) {
    int info[4] = { 0, 0, 0, 0 };
    __cpuidex(info, (int)leaf, (int)subleaf);
    regs[0] = (unsigned int)info[0]; /* EAX */
    regs[1] = (unsigned int)info[1]; /* EBX */
    regs[2] = (unsigned int)info[2]; /* ECX */
    regs[3] = (unsigned int)info[3]; /* EDX */
}

static int test_bit(unsigned int value, unsigned int bit) {
    return (value >> bit) & 1U;
}

static int getcpufeatures(const CStringArray& Parameters) {
    unsigned int regs[4] = { 0, 0, 0, 0 };
    unsigned int max_basic = 0;
    unsigned int max_ext = 0;

    do_cpuid(0, 0, regs);
    max_basic = regs[0];

    do_cpuid(0x80000000u, 0, regs);
    max_ext = regs[0];

    LOG("CPUID max basic leaf: 0x%08X", max_basic);
    LOG("CPUID max extended leaf: 0x%08X", max_ext);

    if (max_ext >= 0x80000004u) {
        unsigned int brand_regs[12];
        char brand[49];
        unsigned int i = 0;

        do_cpuid(0x80000002u, 0, regs);
        for (i = 0; i < 4; ++i) {
            brand_regs[i] = regs[i];
        }
        do_cpuid(0x80000003u, 0, regs);
        for (i = 0; i < 4; ++i) {
            brand_regs[4 + i] = regs[i];
        }
        do_cpuid(0x80000004u, 0, regs);
        for (i = 0; i < 4; ++i) {
            brand_regs[8 + i] = regs[i];
        }

        memcpy(brand, brand_regs, sizeof(brand_regs));
        brand[48] = '\0';
        LOG("CPU brand string: %s", brand);
    }
    else {
        LOG("CPU brand string: unavailable");
    }

    LOG("");

    if (max_basic >= 1) {
        do_cpuid(1, 0, regs);
        LOG("Leaf 0x00000001:");
        LOG("  FXSAVE/FXRSTOR support (EDX[24]): %s",
            test_bit(regs[3], 24) ? "yes" : "no");
        LOG("  XSAVE support (ECX[26]): %s",
            test_bit(regs[2], 26) ? "yes" : "no");
        LOG("  OSXSAVE enabled (ECX[27]): %s",
            test_bit(regs[2], 27) ? "yes" : "no");
        LOG("");
    }
    else {
        LOG("Leaf 0x00000001 not supported.\n");
    }

    if (max_basic >= 0x0D) {
        do_cpuid(0x0D, 1, regs);
        LOG("Leaf 0x0000000D, subleaf 1:");
        LOG("  XSAVEOPT (EAX[0]): %s",
            test_bit(regs[0], 0) ? "yes" : "no");
        LOG("  XSAVEC (EAX[1]): %s",
            test_bit(regs[0], 1) ? "yes" : "no");
        LOG("  XSAVES (EAX[3]): %s",
            test_bit(regs[0], 3) ? "yes" : "no");
        LOG("");
    }
    else {
        LOG("Leaf 0x0000000D not supported.\n");
    }

    if (max_ext >= 0x80000001u) {
        do_cpuid(0x80000001u, 0, regs);
        LOG("Leaf 0x80000001:");
        LOG("  FFXSR fast FXSAVE/FXRSTOR (EDX[25]): %s",
            test_bit(regs[3], 25) ? "yes" : "no");
        LOG("");
    }
    return 0;
}

class CCPUInfo : public CCommandHandler
{
public:
    CCPUInfo() : CCommandHandler("cpu", "retrieve info", 1)
    {
        DeclareCommand("info", getcpufeatures, 0, 0);
    }
private:
    void Help(CStringArray& a) override
    {
        a.Add("info                      \tGet CPU features");
    }
};

static CCPUInfo ci;
