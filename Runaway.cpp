#include "stdafx.h"

class CFileFinder
{
public:
    CFileFinder(const CString& WildCard) : m_WildCard(WildCard) {};
    template<typename T> bool Process(T Functor)
    {
        HANDLE h = FindFirstFile(m_WildCard, &m_fd);
        if (h == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        while (Functor(m_fd.cFileName) && FindNextFile(h, &m_fd)) {}
        FindClose(h);
        return true;
    }
private:
    WIN32_FIND_DATA m_fd = {};
    const CString& m_WildCard;
};

struct CThreadData
{
public:
    CThreadData()
    {
        no = -1;
        id = elapsed = user = kernel = 0;
    }
    int no;
    UINT id;
    ULONG elapsed;
    ULONG user;
    ULONG kernel;
    void SetTime(ULONG Time, char mode)
    {
        switch (mode)
        {
            case 'u': user = Time; break;
            case 'k': kernel = Time; break;
            case 'e': elapsed = Time; break;
            default:
                break;
        }
    }
};

static ULONG MakeTime(UINT Days, UINT Hours, UINT Mins, UINT Sec, UINT Ms)
{
    Hours += 24 * Days;
    Mins  += 60 * Hours;
    Sec   += 60 * Mins;
    Ms += 1000 * Sec;
    return Ms;
}

class CThreadArray : public CArray<CThreadData>
{
public:
    CThreadArray() : m_File(NULL)
    {
        for (INT i = 0; i < 1024; ++i) {
            CThreadData td;
            Add(td);
        }
    }
    CThreadData* FindByNo(int No)
    {
        if (No >= GetCount()) {
            while (No >= GetCount()) {
                CThreadData td;
                Add(td);
            }
            return NULL;
        }
        CThreadData& td = GetAt(No);
        if (td.no != No) {
            return NULL;
        }
        return &td;
    }
    const CThreadData* FindById(UINT Id) const
    {
        for (INT i = 0; i < GetCount(); ++i) {
            const CThreadData& td = GetAt(i);
            if (td.no != i)
                return NULL; //we've done wityh valid threads
            if (td.id == Id) {
                return &td;
            }
        }
        return NULL;
    }
    bool Load(const char* Name)
    {
        char line[1024], *s = line;
        char mode = 'n';
        m_Filename = Name;
        m_File = fopen(Name, "rt");
        if (!m_File) {
            ERR("Can't open %s", m_Filename.GetString());
            return false;
        }
        while (s) {
            s = fgets(line, sizeof(line), m_File);
            if (!s)
                break;
            UINT d, h, m, sec, ms, id, no;
            int res = sscanf(s, "%d:%X %d days %d:%d:%d.%d",
                &no, &id, &d, &h, &m, &sec, &ms);
            if (res == 7) {
                ULONG t = MakeTime(d, h, m, sec, ms);
                CThreadData* ptd = FindByNo(no);
                if (ptd) {
                    ptd->SetTime(t, mode);
                } else {
                    CThreadData td;
                    td.no = no;
                    td.id = (int)id;
                    td.SetTime(t, mode);
                    SetAt(no, td);
                }

            } else if (strstr(s, "User Mode")) {
                mode = 'u';
            }
            else if (strstr(s, "Kernel Mode")) {
                mode = 'k';
            }
            else if (strstr(s, "Elapsed")) {
                mode = 'e';
            }
        }
        return true;
    }
    // res = (Arr1 is later) ? 1 : -1
    static int Compare(const CThreadArray& Arr1, const CThreadArray& Arr2);
    // Arr1 is later
    static void Diff(const CThreadArray& Arr1, const CThreadArray& Arr2);
    void Validate() const;
    void MakeBaseFor(const CThreadArray& Arr)
    {
        m_Filename = "<dummy>";
        for (INT i = 0; i < Arr.GetCount(); ++i) {
            const CThreadData& td = Arr.GetAt(i);
            if (td.no < 0)
                continue;
            CThreadData *dummy = FindByNo(td.no);
            if (!dummy) {
                CThreadData tmp = td;
                tmp.elapsed = 0;
                tmp.kernel = 0;
                tmp.user = 0;
                SetAt(td.no, tmp);
            }
        }
    }
    ~CThreadArray()
    {
        if (m_File) {
            fclose(m_File);
        }
    }
protected:
    CString m_Filename;
    FILE* m_File;
};

void CThreadArray::Validate() const
{
    LPCSTR filename = m_Filename.GetString();
    ULONG elapsed = 0;
    bool  lastvalid = true;
    for (INT i = 0; i < GetCount(); ++i) {
        const CThreadData& td = GetAt(i);
        bool valid = td.no == i;
        if (valid && !lastvalid) {
            LOG("\tInconsistency at %d in %s", i, filename);
            break;
        }
        if (valid && !td.id) {
            LOG("\tInconsistency of ID at %d in %s", i, filename);
            continue;
        }
        if (!valid && lastvalid) {
            lastvalid = false;
            continue;
        }
        if (valid && (td.kernel + td.user) > td.elapsed) {
            LOG("\tInconsistency of times at %d in %s", i, filename);
        }
    }
}

// return 1 is Arr1 is later than Arr2, otherwise -1
// return 0 if they are equal or inconsistent
int CThreadArray::Compare(const CThreadArray& Arr1, const CThreadArray& Arr2)
{
    Arr1.Validate();
    Arr2.Validate();
    int result = 0;
    for (INT i = 0; i < Arr1.GetCount(); ++i) {
        const CThreadData& td1 = Arr1.GetAt(i);
        if (td1.no < 0)
            break;
        const CThreadData* td2 = Arr2.FindById(td1.id);
        if (!td2)
            continue;
        LONG diffElapsed = td1.elapsed - td2->elapsed;
        if (result == 0 && diffElapsed) {
            result = diffElapsed > 0 ? 1 : -1;
        } else if (result > 0 && diffElapsed < 0) {
            return 0;
        } else if (result < 0 && diffElapsed > 0) {
            return 0;
        }
    }
    return result;
}

static ULONG Percentage(ULONG Elapsed, ULONG Kernel, ULONG User)
{
    if (!Elapsed)
        return 0;
    return ((Kernel + User) * 100) / Elapsed;
}

void CThreadArray::Diff(const CThreadArray& Arr1, const CThreadArray& Arr2)
{
    for (INT i = 0; i < Arr1.GetCount(); ++i) {
        const CThreadData& td1 = Arr1.GetAt(i);
        if (td1.no < 0)
            break;
        const CThreadData* td2 = Arr2.FindById(td1.id);
        if (!td2) {
            // the thread was created
            LOG("[L]%04d:%04X created, elapsed %6d, kernel %6d, user %6d, total %3d%%",
                td1.no, td1.id,
                td1.elapsed, td1.kernel, td1.user, Percentage(td1.elapsed, td1.kernel, td1.user));
            continue;
        }
        LONG diffElapsed = td1.elapsed - td2->elapsed;
        LONG diffUser = td1.user - td2->user;
        LONG diffKernel = td1.kernel - td2->kernel;
        CString s;
        bool active = diffKernel || diffUser;
        if (active) {
            LOG("[L]%04d:%04X vs [E]%04d:%04X, elapsed %6d, kernel %6d, user %6d, total %3d%%",
                td1.no, td1.id, td2->no, td2->id,
                diffElapsed, diffKernel, diffUser, Percentage(diffElapsed, diffKernel, diffUser));
        } else {
            bool started = td1.user || td1.kernel;
            LOG("[L]%04d:%04X vs [E]%04d:%04X, elapsed %6d, no activity %s",
                td1.no, td1.id, td2->no, td2->id, diffElapsed,
                started ? " " : "(not started)");
        }
    }
    for (INT i = 0; i < Arr2.GetCount(); ++i) {
        const CThreadData& td2 = Arr2.GetAt(i);
        if (td2.no < 0)
            break;
        const CThreadData* td1 = Arr1.FindById(td2.id);
        if (!td1) {
            // the thread was deleted
            LOG("[L] deleted %04d:%04X", td2.no, td2.id);
        }
    }
}

static int CompareDir(char* dir)
{
    CString sWildCard = dir;
    sWildCard += "\\*.txt";
    CFileFinder f(sWildCard);
    CStringArray files;
    f.Process([&](const char* Name)
        {
            CString s = dir;
            s += "\\";
            s += Name;
            files.Add(s);
            return true;
        });
    if (!files.GetCount()) {
        LOG("Directory %s in empty", dir);
        return 1;
    }
    qsort(files.GetData(), files.GetSize(), sizeof(CString), [](const void* p1, const void* p2) -> int
    {
        CString* name1 = (CString*)p1, *name2 = (CString*)p2;
        CThreadArray a1, a2;
        if (!a1.Load(name1->GetString()) || !a2.Load(name2->GetString())) {
            return 0;
        }
        return CThreadArray::Compare(a1, a2);
    });
    for (INT i = 0; i < files.GetCount(); ++i) {
        int res;
        if (i == 0) {
            char* s = (char *)files[i].GetString();
            res = CompareRunaways(1, &s, false);
            continue;
        }
        char* s[2];
        s[0] = (char*)files[i - 1].GetString();
        s[1] = (char*)files[i].GetString();
        puts("");
        puts("----------------------------------------------------------------");
        res = CompareRunaways(2, s, false);
    }
    return 0;
}

int CompareRunaways(int argc, char** argv, bool directory)
{
    CThreadArray a1, a2;
    switch (argc) {
        case 2:
            if (!a1.Load(argv[0]) || !a2.Load(argv[1])) {
                return 1;
            }
            break;
        case 1:
            if (directory) {
                return CompareDir(argv[0]);
            } else if (!a1.Load(argv[0])) {
                return 1;
            }
            a2.MakeBaseFor(a1);
            break;
        default:
            ERR("cr <filename> [filename]");
            return 1;
    }

    LOG("Comparing data:");
    LOG("[1]: %s", argv[0]);
    LOG("[2]: %s", argc > 1 ? argv[1] : "<dummy>");
    int diff = CThreadArray::Compare(a1, a2);
    if (!diff) {
        LOG("Can't compare results");
        return 1;
    } else if (diff > 0) {
        LOG("First one is later, comparing");
        CThreadArray::Diff(a1, a2);
    } else if (diff < 0) {
        LOG("First one is earlier, comparing");
        CThreadArray::Diff(a2, a1);
    }
    return 0;
}