#include "stdafx.h"
#include <SetupAPI.h>
#include <devpkey.h>
#include <newdev.h>
#include <pciprop.h>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "uuid.lib")

static void LogBoolResult(const char* Name, bool LogSuccess, BOOL& result)
{
    if (!result) {
        LOG("error on %s = 0x%X", Name, GetLastError());
    }
    else if (LogSuccess) {
        LOG("%s OK", Name);
    }
}

#if USE_NEWLIB
// keeping this for the history
#pragma comment(lib, "newdev.lib")
extern "C" __declspec(dllimport) BOOL WINAPI InstallSelectedDriver(
    HWND  hwndParent,
    HDEVINFO  DeviceInfoSet,
    LPCWSTR  Reserved,
    BOOL  Backup,
    PDWORD  pReboot
);

static BOOL DoInstallDriver(HDEVINFO hDevInfo, PSP_DEVINFO_DATA pDevData, PSP_DRVINFO_DATA_V2 pDrvData, PBOOL pReboot)
{
    BOOL res = false;
    if (0) {
        res = DiInstallDevice(NULL, hDevInfo, pDevData, pDrvData, 0, pReboot);
        LogBoolResult("DiInstallDevice", false, res);
    } else {
        DWORD reboot = 0;
        res = InstallSelectedDriver(NULL, hDevInfo, NULL, TRUE, &reboot);
        *pReboot = reboot != 0;
        LogBoolResult("InstallSelectedDriver", true, res);
    }
    return res;
}
#endif

static void CStringAtoW(const CStringA& a, CStringW& w)
{
    for (INT i = 0; i < a.GetLength(); ++i) {
        w += (char)tolower(a[i]);
    }
}

// config manager
class CDeviceInfo
{
public:
    CDeviceInfo()
    {
        m_Devinfo = NULL;
        RtlZeroMemory(&m_Data, sizeof(m_Data));
    }
    CDeviceInfo(HDEVINFO DevInfo, SP_DEVINFO_DATA& Data)
    {
        m_Devinfo = DevInfo;
        m_Data = Data;
        WCHAR buffer[256];
        DWORD bufferSize = ARRAYSIZE(buffer);
        if (SetupDiGetDeviceInstanceIdW(
            m_Devinfo, &m_Data,
            buffer,
            bufferSize,
            &bufferSize)) {
            m_DevInstId = buffer;
            //LOG("Found %S", buffer);
            //Log("\tDescription: %S", Description().GetString());
            //LOG("\tFriendly name: %S", FriendlyName().GetString());
            //Log("\tStatus: %08X", Status());
        }
    }
    const CStringW& Id() const { return m_DevInstId; }
    CStringW InfFile() const
    {
        return GetDeviceStringProperty(DEVPKEY_Device_DriverInfPath);
    }
    CStringW Description() const
    {
        return GetDeviceStringProperty(DEVPKEY_Device_DeviceDesc);
    }
    CStringW FriendlyName() const
    {
        return GetDeviceStringProperty(DEVPKEY_Device_FriendlyName);
    }
    UINT Status() const
    {
        return GetDeviceUlongProperty(DEVPKEY_Device_DevNodeStatus);
    }
    CStringA PCIProp(const DEVPROPKEY& PropKey, LPCSTR Name) const
    {
        UINT32 val = GetDeviceUlongProperty(PropKey);
        CStringA s;
        s.Format("%s=0x%X", Name, val);
        return s;
    }
    static CString DriverInfoToString(const FILETIME& Date, ULONGLONG& Version, LPWSTR Inf)
    {
        CString s;
        LARGE_INTEGER li;
        SYSTEMTIME sysTime;
        li.QuadPart = Version;
        s.Format("%S:%d.%d.%d.%d:", Inf, LOWORD(li.LowPart), HIWORD(li.LowPart), LOWORD(li.HighPart), HIWORD(li.HighPart));
        if (FileTimeToSystemTime(&Date, &sysTime)) {
            s.AppendFormat("%02d.%02d.%d", sysTime.wDay, sysTime.wMonth, sysTime.wYear);
        }
        return s;
    }
    template<typename TFunctor> void EnumerateDrivers(TFunctor Func) const
    {
        SP_DRVINFO_DATA_V2_W data;
        SP_DRVINFO_DETAIL_DATA_W detail;
        ULONG index = 0;
        data.cbSize = sizeof(data);
        if (!SetupDiBuildDriverInfoList(m_Devinfo, &m_Data, SPDIT_COMPATDRIVER)) {
            ERR("Error %d on SetupDiBuildDriverInfoList", GetLastError());
        }
        do {
            if (SetupDiEnumDriverInfoW(m_Devinfo, &m_Data, SPDIT_COMPATDRIVER, index, &data)) {
                ULONG size = 0;
                detail.cbSize = sizeof(detail);
                SetupDiGetDriverInfoDetailW(m_Devinfo, &m_Data, &data, &detail, sizeof(detail), &size);
                if (size >= sizeof(detail)) {
                    CString s = DriverInfoToString(data.DriverDate, data.DriverVersion, detail.InfFileName);
                    Func(s, data);
                    //LOG("Got SetupDiGetDriverInfoDetailW size %d", size);
                }
                else {
                    ERR("SetupDiGetDriverInfoDetailW size %d", size);
                }
                index++;
            }
            else {
                break;
            }
        } while (true);
    }
    void GetDrivers(CStringArray& Drivers) const
    {
        EnumerateDrivers([&](CString& s, SP_DRVINFO_DATA_V2_W& data) {
            Drivers.Add(s);
        });
    }
    void SetSelected(const CString& Unique) const
    {
        ULONG found = 0;
        EnumerateDrivers([&](CString& s, SP_DRVINFO_DATA_V2_W& data) {
            if (s.Find(Unique) >= 0) {
                found++;
            }
            else {
                SP_DRVINSTALL_PARAMS params = {};
                params.cbSize = sizeof(params);
                params.Rank = 0xffffffff;
                params.Flags = DNF_BAD_DRIVER;
                BOOL res = SetupDiSetDriverInstallParamsW(m_Devinfo, &m_Data, &data, &params);
                LogBoolResult("SetupDiSetDriverInstallParamsW", false, res);
            }
        });

        switch (found) {
            case 1:
                {
                    BOOL reboot = false;
                    SP_DEVINSTALL_PARAMS params;
                    params.cbSize = sizeof(params);
                    BOOL res = SetupDiSelectBestCompatDrv(m_Devinfo, &m_Data);
                    LogBoolResult("SetupDiSelectBestCompatDrv", true, res);
                    res = SetupDiInstallDevice(m_Devinfo, &m_Data);
                    LogBoolResult("SetupDiInstallDevice", true, res);
                    if (res) {
                        res = SetupDiGetDeviceInstallParams(m_Devinfo, &m_Data, &params);
                        LogBoolResult("SetupDiGetDeviceInstallParams", false, res);
                    }
                    if (res && (params.Flags & (DI_NEEDREBOOT | DI_NEEDRESTART))) {
                        LOG("Reboot required");
                    }
                }
                break;
            case 0:
                LOG("No suitable driver found");
                break;
            default:
                LOG("%d suitable drivers found", found);
                break;
        }
    }
    void Restart()
    {
        CallClass(DICS_DISABLE);
        CallClass(DICS_ENABLE);
    }
    bool Match(const CStringW& Filter) const
    {
        CArray<CStringW> strings;
        bool b = GetDeviceStringListProperty(DEVPKEY_Device_HardwareIds, strings);
        if (!b)
            return false;
        for (UINT i = 0; i < strings.GetCount(); i++) {
            strings[i].MakeLower();
            if (strings[i].Find(Filter) >= 0)
                return true;
        }
        return false;
    }
protected:
    mutable SP_DEVINFO_DATA m_Data;
    HDEVINFO        m_Devinfo;
    CStringW         m_DevInstId;
    bool GetDeviceStringListProperty(const DEVPROPKEY& PropKey, CArray<CStringW>& Strings) const
    {
        BYTE buffer[4096];
        DWORD bufferSize = sizeof(buffer);
        DEVPROPTYPE propType;

        if (SetupDiGetDevicePropertyW(m_Devinfo, &m_Data, &PropKey, &propType, buffer, bufferSize, &bufferSize, 0) &&
            propType == DEVPROP_TYPE_STRING_LIST) {
            WCHAR* p = (WCHAR*)buffer;
            while (*p) {
                CStringW s(p);
                Strings.Add(s);
                p += s.GetLength() + 1;
            }
        }
        return Strings.GetCount() > 0;
    }
    CStringW GetDeviceStringProperty(const DEVPROPKEY& PropKey) const
    {
        BYTE buffer[512];
        DWORD bufferSize = sizeof(buffer);
        DEVPROPTYPE propType;
        CStringW s;

        if (SetupDiGetDevicePropertyW(m_Devinfo, &m_Data, &PropKey, &propType, buffer, bufferSize, &bufferSize, 0) &&
            propType == DEVPROP_TYPE_STRING) {
            s = (LPCWSTR)buffer;
        }
        return s;
    }
    ULONG GetDeviceUlongProperty(const DEVPROPKEY& PropKey) const
    {
        ULONG val = 0;
        DWORD bufferSize = sizeof(val);
        DEVPROPTYPE propType;

        if (!SetupDiGetDevicePropertyW(m_Devinfo, &m_Data, &PropKey, &propType, (BYTE*)&val, bufferSize, &bufferSize, 0) ||
            propType == DEVPROP_TYPE_UINT32) {
            //error
        }
        return val;
    }
    void CallClass(ULONG Code)
    {
        SP_PROPCHANGE_PARAMS pcp;
        pcp.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
        pcp.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
        pcp.StateChange = Code;
        pcp.Scope = DICS_FLAG_CONFIGSPECIFIC;
        pcp.HwProfile = 0;
        if (!SetupDiSetClassInstallParams(m_Devinfo, &m_Data, &pcp.ClassInstallHeader, sizeof(pcp))) {
            LOG("%s preparing failed for code %d, error %d", __FUNCTION__, Code, GetLastError());
            return;
        }
        if (!SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, m_Devinfo, &m_Data)) {
            LOG("%s failed for code %d, error %d", __FUNCTION__, Code, GetLastError());
        }
    }
};

class CInfoSet
{
public:
    CInfoSet(const CString& Filter)
    {
        CStringAtoW(Filter, m_Filter);
        m_Devinfo = SetupDiGetClassDevsEx(
            nullptr,
            nullptr,
            nullptr,
            DIGCF_ALLCLASSES,
            nullptr,
            nullptr,
            nullptr);
        if (m_Devinfo == INVALID_HANDLE_VALUE) {
            m_Devinfo = NULL;
            ERR("%s error %d creating device list", __FUNCTION__, GetLastError());
        }
        else {
            Enumerate();
        }
    }
    ~CInfoSet()
    {
        if (m_Devinfo) {
            SetupDiDestroyDeviceInfoList(m_Devinfo);
        }
    }
    void Enumerate()
    {
        if (!m_Devinfo) {
            return;
        }
        SP_DEVINFO_DATA devInfo;
        devInfo.cbSize = sizeof(devInfo);
        for (ULONG i = 0; SetupDiEnumDeviceInfo(m_Devinfo, i, &devInfo); i++) {
            CDeviceInfo dev(m_Devinfo, devInfo);
            if (dev.Match(m_Filter)) {
                m_Devices.Add(dev);
            }
        }
    }
    const CArray<CDeviceInfo>& Devices() const { return m_Devices; }
protected:
    HDEVINFO m_Devinfo;
    CArray<CDeviceInfo> m_Devices;
    CStringW m_Filter;
};


class CPnpHandler : public CCommandHandler
{
public:
    CPnpHandler() : CCommandHandler("pnp", "PNP related operations", 2) {}
private:
    int Run(const CStringArray& Parameters) override
    {
        if (!Parameters[0].CompareNoCase("enum")) {
            Enumerate(Parameters[1]);
            return 0;
        } else if (!Parameters[0].CompareNoCase("install") && Parameters.GetCount() > 2) {
            InstallDriver(Parameters[1], Parameters[2]);
        } else if (!Parameters[0].CompareNoCase("pci") && Parameters.GetCount() > 1) {
            EnumeratePCIProp(Parameters[1]);
            return 0;
        }
        return 1;
    }
    template<typename TFunc> void Enum(const CString& PnpId, TFunc Functor)
    {
        CInfoSet infoSet(PnpId);
        auto& devs = infoSet.Devices();
        for (INT i = 0; i < devs.GetCount(); ++i) {
            auto& d = devs[i];
            auto desc = d.Description();
            LOG("Found: %S\n\t%S", d.Id().GetString(), desc.GetString());
            Functor(d);
        }
    }
    void Enumerate(const CString& PnpId);
    void EnumeratePCIProp(const CString& PnpId);
    void InstallDriver(const CString& PnpId, const CString& DriverUnique);
    void Help(CStringArray& a) override
    {
        a.Add("enum <PnpID>\t\tenumerate devices and drivers");
        a.Add("install <PnpID> <driver-string>\t\tinstall driver");
        a.Add("pci <PnpID>\t\tquery PCI properties");
    }
    static CStringW GetDriverStoreLocation(const CStringW& InfName)
    {
        WCHAR buffer[MAX_PATH] = L"Unknown";
        BOOL b = SetupGetInfDriverStoreLocationW(InfName, NULL, NULL, buffer, MAX_PATH, NULL);
        return buffer;
    }
};

void CPnpHandler::Enumerate(const CString& PnpId)
{
    Enum(PnpId,
        [](const CDeviceInfo& d)
        {
            CStringArray drivers;
            d.GetDrivers(drivers);
            for (INT j = 0; j < drivers.GetCount(); ++j) {
                LOG("\t%d: %s", j, drivers[j].GetString());
            }
            CStringW infName = d.InfFile();
            LOG("\tinstalled: %S", infName.GetString());
            CStringW location = GetDriverStoreLocation(infName);
            LOG("\t\tat: %S", location.GetString());
        }
    );
}

void CPnpHandler::EnumeratePCIProp(const CString& PnpId)
{
    Enum(PnpId,
        [](const CDeviceInfo& d)
        {
            DEVPROPKEY prop = DEVPKEY_PciDevice_AtsSupport;
            CStringA s = d.PCIProp(prop, "ATS");
            LOG("%s", s.GetString());

            prop = DEVPKEY_Device_DMARemapping;
            s = d.PCIProp(prop, "DMAR");
            LOG("%s", s.GetString());
        }
    );
}


void CPnpHandler::InstallDriver(const CString& PnpId, const CString& DriverUnique)
{
    CInfoSet infoSet(PnpId);
    auto& devs = infoSet.Devices();
    if (devs.GetCount() != 1) {
        LOG("This is for single device only");
    } else {
        auto& d = devs[0];
        d.SetSelected(DriverUnique);
    }
}

static CPnpHandler PnpHandler;