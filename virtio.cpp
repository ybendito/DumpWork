#include "stdafx.h"

typedef struct _tFeatureEntry
{
    ULONG     Value;
    bool      Legacy;
    LPCSTR    Name;
} tFeatureEntry;
#define FEATURE(x) { x, false, #x}
#define LEGACY_FEATURE(x) { x, true, #x}
#define ENDOFLIST  { 0, NULL }  

// common (transport) features
#define VIRTIO_F_NOTIFY_ON_EMPTY     24  // legacy
#define VIRTIO_F_ANY_LAYOUT		     27  // legacy
#define VIRTIO_RING_F_INDIRECT_DESC  28
#define VIRTIO_RING_F_EVENT_IDX      29
#define VIRTIO_F_UNUSED              30       
#define VIRTIO_F_VERSION_1		     32
#define VIRTIO_F_IOMMU_PLATFORM      33 // a.k.a. VIRTIO_F_ACCESS_PLATFORM
#define VIRTIO_F_RING_PACKED         34
#define VIRTIO_F_IN_ORDER            35
#define VIRTIO_F_ORDER_PLATFORM      36
#define VIRTIO_F_SR_IOV              37
#define VIRTIO_F_NOTIFICATION_DATA   38
#define VIRTIO_F_NOTIF_CONFIG_DATA   39
#define VIRTIO_F_RING_RESET          40
#define VIRTIO_F_ADMIN_VQ            41

const tFeatureEntry CommonFeatures[] =
{
    LEGACY_FEATURE(VIRTIO_F_NOTIFY_ON_EMPTY),
    LEGACY_FEATURE(VIRTIO_F_ANY_LAYOUT),
    FEATURE(VIRTIO_F_VERSION_1),
    FEATURE(VIRTIO_F_RING_PACKED),
    FEATURE(VIRTIO_F_IN_ORDER),
    FEATURE(VIRTIO_F_ORDER_PLATFORM),
    FEATURE(VIRTIO_F_SR_IOV),
    FEATURE(VIRTIO_F_NOTIFICATION_DATA),
    FEATURE(VIRTIO_F_NOTIF_CONFIG_DATA),
    FEATURE(VIRTIO_F_RING_RESET),
    FEATURE(VIRTIO_F_ADMIN_VQ),
    FEATURE(VIRTIO_RING_F_INDIRECT_DESC),
    FEATURE(VIRTIO_F_UNUSED),
    ENDOFLIST
};

// net
/* The feature bitmap for virtio net */
#define VIRTIO_NET_F_CSUM            0  /* Host handles pkts w/ partial csum */
#define VIRTIO_NET_F_GUEST_CSUM      1  /* Guest handles pkts w/ partial csum */
#define VIRTIO_NET_F_CTRL_GUEST_OFFLOADS 2 /* Dynamic offload configuration. */
#define VIRTIO_NET_F_MTU             3  /* Initial MTU advice */
#define VIRTIO_NET_F_MAC             5  /* Host has given MAC address. */
#define VIRTIO_NET_F_GUEST_TSO4      7  /* Guest can handle TSOv4 in. */
#define VIRTIO_NET_F_GUEST_TSO6      8  /* Guest can handle TSOv6 in. */
#define VIRTIO_NET_F_GUEST_ECN       9  /* Guest can handle TSO[6] w/ ECN in. */
#define VIRTIO_NET_F_GUEST_UFO       10	/* Guest can handle UFO in. */
#define VIRTIO_NET_F_HOST_TSO4       11 /* Host can handle TSOv4 in. */
#define VIRTIO_NET_F_HOST_TSO6       12 /* Host can handle TSOv6 in. */
#define VIRTIO_NET_F_HOST_ECN        13 /* Host can handle TSO[6] w/ ECN in. */
#define VIRTIO_NET_F_HOST_UFO        14 /* Host can handle UFO in. */
#define VIRTIO_NET_F_MRG_RXBUF       15 /* Host can merge receive buffers. */
#define VIRTIO_NET_F_STATUS	         16 /* virtio_net_config.status available */
#define VIRTIO_NET_F_CTRL_VQ         17 /* Control channel available */
#define VIRTIO_NET_F_CTRL_RX         18 /* Control channel RX mode support */
#define VIRTIO_NET_F_CTRL_VLAN       19 /* Control channel VLAN filtering */
#define VIRTIO_NET_F_CTRL_RX_EXTRA   20 /* Extra RX mode control support */
#define VIRTIO_NET_F_GUEST_ANNOUNCE  21 /* Guest can announce device on the network */
#define VIRTIO_NET_F_MQ	             22 /* Device supports Receive Flow Steering */
#define VIRTIO_NET_F_CTRL_MAC_ADDR   23 /* Set MAC address */
#define VIRTIO_NET_F_HASH_TUNNEL     51 /* Device supports inner header hash for encapsulated packets */
#define VIRTIO_NET_F_VQ_NOTF_COAL    52 /* Device supports virtqueue notification coalescing */
#define VIRTIO_NET_F_NOTF_COAL       53 /* Device supports notifications coalescing */
#define VIRTIO_NET_F_GUEST_USO4      54 /* Guest can handle USOv4 in. */
#define VIRTIO_NET_F_GUEST_USO6      55 /* Guest can handle USOv6 in. */
#define VIRTIO_NET_F_HOST_USO        56 /* Host can handle USO in. */
#define VIRTIO_NET_F_HASH_REPORT     57 /* Supports hash report */
#define VIRTIO_NET_F_GUEST_HDRLEN    59 /* Guest provides the exact hdr_len value. */
#define VIRTIO_NET_F_RSS             60 /* Supports RSS RX steering */
#define VIRTIO_NET_F_RSC_EXT         61 /* extended coalescing info */
#define VIRTIO_NET_F_STANDBY         62 /* Act as standby for another device with the same MAC. */
#define VIRTIO_NET_F_SPEED_DUPLEX    63 /* Device set linkspeed and duplex */
#define VIRTIO_NET_F_GSO             6  /* legacy, Host handles pkts w/ any GSO type */

const tFeatureEntry NetFeatures[] =
{
    FEATURE(VIRTIO_NET_F_CSUM),
    FEATURE(VIRTIO_NET_F_GUEST_CSUM),
    FEATURE(VIRTIO_NET_F_CTRL_GUEST_OFFLOADS),
    FEATURE(VIRTIO_NET_F_MTU),
    FEATURE(VIRTIO_NET_F_MAC),
    FEATURE(VIRTIO_NET_F_GUEST_TSO4),
    FEATURE(VIRTIO_NET_F_GUEST_TSO6),
    FEATURE(VIRTIO_NET_F_GUEST_ECN),
    FEATURE(VIRTIO_NET_F_GUEST_UFO),
    FEATURE(VIRTIO_NET_F_HOST_TSO4),
    FEATURE(VIRTIO_NET_F_HOST_TSO6),
    FEATURE(VIRTIO_NET_F_HOST_ECN),
    FEATURE(VIRTIO_NET_F_HOST_UFO),
    FEATURE(VIRTIO_NET_F_MRG_RXBUF),
    FEATURE(VIRTIO_NET_F_STATUS),
    FEATURE(VIRTIO_NET_F_CTRL_VQ),
    FEATURE(VIRTIO_NET_F_CTRL_RX),
    FEATURE(VIRTIO_NET_F_CTRL_VLAN),
    FEATURE(VIRTIO_NET_F_CTRL_RX_EXTRA),
    FEATURE(VIRTIO_NET_F_GUEST_ANNOUNCE),
    FEATURE(VIRTIO_NET_F_MQ),
    FEATURE(VIRTIO_NET_F_CTRL_MAC_ADDR),
    FEATURE(VIRTIO_NET_F_HASH_TUNNEL),
    FEATURE(VIRTIO_NET_F_VQ_NOTF_COAL),
    FEATURE(VIRTIO_NET_F_NOTF_COAL),
    FEATURE(VIRTIO_NET_F_GUEST_USO4),
    FEATURE(VIRTIO_NET_F_GUEST_USO6),
    FEATURE(VIRTIO_NET_F_HOST_USO),
    FEATURE(VIRTIO_NET_F_HASH_REPORT),
    FEATURE(VIRTIO_NET_F_GUEST_HDRLEN),
    FEATURE(VIRTIO_NET_F_RSS),
    FEATURE(VIRTIO_NET_F_RSC_EXT),
    FEATURE(VIRTIO_NET_F_STANDBY),
    FEATURE(VIRTIO_NET_F_SPEED_DUPLEX),
    FEATURE(VIRTIO_NET_F_HASH_TUNNEL),
    LEGACY_FEATURE(VIRTIO_NET_F_GSO),
    ENDOFLIST
};

#define VIRTIO_BALLOON_F_MUST_TELL_HOST     0  // Host has to be told before pages from the balloon are used.
#define VIRTIO_BALLOON_F_STATS_VQ           1  //A virtqueue for reporting guest memory statistics is present
#define VIRTIO_BALLOON_F_DEFLATE_ON_OOM     2  // Deflate balloon on guest out of memory condition.
#define VIRTIO_BALLOON_F_FREE_PAGE_HINT     3  //The device has support for free page hinting.
#define VIRTIO_BALLOON_F_PAGE_POISON        4  // A hint to the device, that the driver will immediately write poison_val to pages after deflating them.Configuration field poison_val is valid.
#define VIRTIO_BALLOON_F_PAGE_REPORTING     5  //The device has support for free page reporting.A virtqueue for reporting free guest memory is present.

const tFeatureEntry BaloonFeatures[] =
{
    FEATURE(VIRTIO_BALLOON_F_MUST_TELL_HOST),
    FEATURE(VIRTIO_BALLOON_F_STATS_VQ),
    FEATURE(VIRTIO_BALLOON_F_DEFLATE_ON_OOM),
    FEATURE(VIRTIO_BALLOON_F_FREE_PAGE_HINT),
    FEATURE(VIRTIO_BALLOON_F_PAGE_POISON),
    FEATURE(VIRTIO_BALLOON_F_PAGE_REPORTING),
    ENDOFLIST
};

typedef struct _tClassTableEntry
{
    LPCSTR ClassName;
    const tFeatureEntry* Table;
} tClassTable;

const tClassTable ClassTable[] =
{
    { "Network", NetFeatures },
    { "Balloon", BaloonFeatures },
    ENDOFLIST
};

class CVirtioFeatures : public CCommandHandler
{
public:
    CVirtioFeatures() : CCommandHandler("virtio", "present virtio features", 2)
    {
        CString s;
        for (const tClassTable* temp = ClassTable; ; temp++) {
            if (!temp->Table)
                break;
            s.AppendFormat(" %s", temp->ClassName);
        }
        m_Classes = s;
    }
private:
    int Run(const CStringArray& Parameters) override
    {
        return Parse(Parameters[0], Parameters[1]);
    }
    void Help(CStringArray& a) override
    {
        a.Add("<class> <features in hex>");
        a.Add("class: " + m_Classes);
    }
    int Parse(const CString& Class, const CString& Mask);
    CString m_Classes;
};

static void ParseTable(const tFeatureEntry* Table, LPCSTR Header, LONGLONG& Mask)
{
    puts(Header);
    while (true) {
        if (!Table->Name)
            break;
        ULONG val = Table->Value;
        LONGLONG mask = 1LL << val;
        if (Mask & mask) {
            printf("\t%s(%d)%s\n", Table->Name, val, Table->Legacy ? " Legacy" : "");
            Mask &= ~mask;
        }
        Table++;
    }
}

int CVirtioFeatures::Parse(const CString& Class, const CString& Mask)
{
    CString mask = Mask;
    LPCSTR  className = NULL;
    mask.MakeLower();
    if (mask[0] == '0' && mask[1] == 'x') {
        mask.Delete(0, 2);
    }
    char* endptr;
    LONGLONG bitmask = strtoll(Mask, &endptr, 16);

    if (*endptr) {
        printf("warning: provided bitmask %s is not valid\n", Mask.GetString());
        printf("Parsing bitmask %llX\n", bitmask);
    }

    const tFeatureEntry* devTable = NULL;
    for (const tClassTable* temp = ClassTable; devTable == NULL; temp++) {
        if (!temp->Table)
            break;
        if (!Class.CompareNoCase(temp->ClassName)) {
            devTable = temp->Table;
            className = temp->ClassName;
        }
    }
    if (!devTable) {
        printf("warning: Unaware of device-specific features for class '%s'\n", Class.GetString());
        printf("Classes: %s\n", m_Classes.GetString());
    }

    ParseTable(CommonFeatures, "Common features", bitmask);
    if (devTable) {
        CString s;
        s.Format("%s-specific features", className);
        ParseTable(devTable, s, bitmask);
    }
    if (bitmask) {
        CString s = "Unknown features:";
        for (int i = 0; i < 64; ++i) {
            if ((1LL << i) & bitmask)
                s.AppendFormat(" %d", i);
        }
        puts(s);
    }
    return 0;
}

static CVirtioFeatures vf;
