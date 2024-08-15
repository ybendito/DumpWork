#include "stdafx.h"

class CBitsParser : public CCommandHandler
{
public:
    CBitsParser() : CCommandHandler("bits", "parse bitmask", 2)
    {
    }
private:
    int Run(const CStringArray& Parameters) override
    {
        if (!Parameters[0].CompareNoCase("iommu")) {
            return ParseIommu(Parameters[1], Parameters[2]);
        }
        return 1;
    }
    void Help(CStringArray& a) override
    {
        a.Add("iommu <64bits caps> <64bits ecaps>");
    }
    int ParseIommu(const CString& Caps, const CString& ECaps);
};

static bool Convert(const CString& Text, ULONGLONG& Value)
{
    char* endptr;
    LONGLONG val = strtoll(Text, &endptr, 16);
    if (*endptr) {
        ERR("error parsing %s", Text.GetString());
        return false;
    }
    Value = (ULONGLONG)val;
    return true;
}

#define DUMP_FIELD(x) LOG("\t%s=%d", #x, (int)bits.##x)

typedef union _tagIommuCaps
{
    struct
    {
        ULONGLONG num_domains : 3; // 0..2
        ULONGLONG reserved3 : 1; // 3
        ULONGLONG rvbf : 1; // 4
        ULONGLONG plmr : 1; // 5
        ULONGLONG phmr : 1; // 6
        ULONGLONG caching : 1; // 7
        ULONGLONG sagaw : 5; // 8..12
        ULONGLONG reserved13 : 3; // 13..15
        ULONGLONG mgaw : 6; // 16..21
        ULONGLONG zlr : 1; // 22
        ULONGLONG dep23 : 1; // 23
        ULONGLONG fro : 10; // 24..33
        ULONGLONG sslps : 4; // 34..37
        ULONGLONG reserved38 : 1; // 38
        ULONGLONG psi : 1; // 39
        ULONGLONG nfr : 8; // 40..47
        ULONGLONG mamv : 6; //48..53
        ULONGLONG dwd  : 1; // 54
        ULONGLONG drd  : 1; // 55
        ULONGLONG fs1gp : 1; // 56
        ULONGLONG reserved57 : 2; // 57..58
        ULONGLONG pi : 1; // 59
        ULONGLONG fs5lp : 1; // 60
        ULONGLONG ecmds : 1; // 61
        ULONGLONG esirpts : 1; // 62
        ULONGLONG esrtps : 1; // 63
    } bits;
    ULONGLONG value;
    void Dump()
    {
        LOG("IOMMU CAPS:");
        DUMP_FIELD(num_domains);
        DUMP_FIELD(rvbf);
        DUMP_FIELD(plmr);
        DUMP_FIELD(phmr);
        DUMP_FIELD(caching);
        DUMP_FIELD(sagaw);
        DUMP_FIELD(mgaw);
        DUMP_FIELD(zlr);
        DUMP_FIELD(fro);
        DUMP_FIELD(sslps);
        DUMP_FIELD(psi);
        DUMP_FIELD(nfr);
        DUMP_FIELD(mamv);
        DUMP_FIELD(dwd);
        DUMP_FIELD(drd);
        DUMP_FIELD(fs1gp);
        DUMP_FIELD(pi);
        DUMP_FIELD(fs5lp);
        DUMP_FIELD(ecmds);
        DUMP_FIELD(esirpts);
        DUMP_FIELD(esrtps);
    }
} tIommuCaps;

typedef union _tagIommuEcaps
{
    struct
    {
        ULONGLONG coherency : 1; // 0
        ULONGLONG qinv : 1; // 1
        ULONGLONG dt : 1; // 2
        ULONGLONG ir : 1; // 3
        ULONGLONG eim : 1; // 4
        ULONGLONG reserved5 : 1; // 5
        ULONGLONG passthrough : 1; // 6
        ULONGLONG snoop : 1; // 7
        ULONGLONG iro : 10; // 8..17
        ULONGLONG reserved18 : 2; // 18..19
        ULONGLONG mhmv : 4; // 20..23
        ULONGLONG reserved24 : 1; // 24
        ULONGLONG mts : 1; // 25
        ULONGLONG nest : 1; // 26
        ULONGLONG reserved27 : 1; // 27
        ULONGLONG reserved28 : 1; // 28
        ULONGLONG prs : 1; // 29
        ULONGLONG ers : 1; // 30
        ULONGLONG srs : 1; // 31
        ULONGLONG reserved32 : 1; // 32
        ULONGLONG nwfs : 1; // 33
        ULONGLONG eafs : 1; // 34
        ULONGLONG pss : 5; // 35..39
        ULONGLONG pasid : 1; // 40
        ULONGLONG dit : 1; // 41
        ULONGLONG pds : 1; // 42
        ULONGLONG smts : 1; // 43
        ULONGLONG vcs : 1; // 44
        ULONGLONG ssads : 1; // 45
        ULONGLONG ssts : 1; // 46
        ULONGLONG fsts : 1; // 47
        ULONGLONG scal_pwc : 1; // 48
        ULONGLONG rid_pasid : 1; // 49
        ULONGLONG reserved50 : 1; // 50
        ULONGLONG perf : 1; // 51
        ULONGLONG abort : 1; // 52
        ULONGLONG rid_priv : 1; // 53
        ULONGLONG reserved54 : 4; // 54..57
        ULONGLONG sms : 1; // 58
        ULONGLONG reserved59 : 5; // 59..63
    } bits;
    ULONGLONG value;
    void Dump()
    {
        LOG("IOMMU ECAPS:");
        DUMP_FIELD(coherency);
        DUMP_FIELD(qinv);
        DUMP_FIELD(dt);
        DUMP_FIELD(ir);
        DUMP_FIELD(eim);
        DUMP_FIELD(passthrough);
        DUMP_FIELD(snoop);
        DUMP_FIELD(iro);
        DUMP_FIELD(mhmv);
        DUMP_FIELD(mts);
        DUMP_FIELD(nest);
        DUMP_FIELD(prs);
        DUMP_FIELD(ers);
        DUMP_FIELD(srs);
        DUMP_FIELD(nwfs);
        DUMP_FIELD(eafs);
        DUMP_FIELD(pss);
        DUMP_FIELD(pasid);
        DUMP_FIELD(dit);
        DUMP_FIELD(pds);
        DUMP_FIELD(smts);
        DUMP_FIELD(vcs);
        DUMP_FIELD(ssads);
        DUMP_FIELD(ssts);
        DUMP_FIELD(fsts);
        DUMP_FIELD(scal_pwc);
        DUMP_FIELD(rid_pasid);
        DUMP_FIELD(perf);
        DUMP_FIELD(abort);
        DUMP_FIELD(rid_priv);
        DUMP_FIELD(sms);
    }
} tIommuEcaps;


int CBitsParser::ParseIommu(const CString& Caps, const CString& ECaps)
{
    tIommuCaps caps;
    tIommuEcaps ecaps;
    if (!Convert(Caps, caps.value) || !Convert(ECaps, ecaps.value)) {
        return 1;
    }
    caps.Dump();
    ecaps.Dump();
    return 0;
}

static CBitsParser bitsHandler;
