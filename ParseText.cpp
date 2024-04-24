#include "stdafx.h"

class CIniFile
{
public:
    CIniFile(LPCSTR Profile)
    {
        FILE* f = fopen(Profile, "rt");
        if (!f)
            return;
        char line[1024];
        while (true)
        {
            char* s = fgets(line, sizeof(line), f);
            if (!s)
                break;
            CString line = s;
            line.Remove('\n');
            line.Remove('\r');
            if (line.IsEmpty())
                continue;
            int nPos = line.Find('=');
            if (nPos < 1)
                continue;
            CString key = line.Left(nPos), value = line.Right(line.GetLength() - nPos - 1);
            value.MakeLower();
            m_Map.SetAt(key, value);
        }
        fclose(f);
    }
    bool Read(const CString& Key, CString& Val)
    {
        if (m_Map.Lookup(Key, Val))
            return true;
        Val.Empty();
        return false;
    }
    bool Read(const CString& Key, int& Val)
    {
        return false;
    }
protected:
    CMapStringToString m_Map;
};

class CIniFileParser : public CIniFile
{
public:
    CIniFileParser(LPCSTR Profile) : CIniFile(Profile)
    {
        CString key, val;
        key = "Start";
        Read(key, m_BlockStart);
        LOG("%s=%s", key.GetString(), m_BlockStart.GetString());
        key = "End";
        Read(key, m_BlockEnd);
        LOG("%s=%s", key.GetString(), m_BlockEnd.GetString());
        UINT i = 1;
        while (true) {
            key.Format("Filter%d", i);
            if (!Read(key, val))
                break;
            LOG("%s=%s", key.GetString(), val.GetString());
            m_Filters.Add(val);
            i++;
        }
        if (m_BlockStart.IsEmpty() || m_Filters.IsEmpty()) {
            ERR("Ini file %s is not loaded", Profile);
        }
        puts("");
    }
    void Flush(CStringArray& Lines, bool Print)
    {
        while (Lines.GetCount()) {
            if (Print) {
                printf("%s", Lines[0].GetString());
            }
            Lines.RemoveAt(0);
        }
        if (Print) {
            puts("");
        }
    }
    int Parse(LPCSTR File)
    {
        CStringArray lines;
        if (m_BlockStart.IsEmpty() || m_Filters.IsEmpty()) {
            return 1;
        }
        FILE* f = fopen(File, "rt");
        if (!f) {
            return ERROR_FILE_NOT_FOUND;
        }
        char line[1024];
        bool bBlockStarted = false;
        bool bBlockIncluded = false;
        while (true)
        {
            char* s = fgets(line, sizeof(line), f);
            if (!s)
                break;
            CString line = s;
            line.MakeLower();
            if (strstr(line, m_BlockStart)) {
                Flush(lines, bBlockIncluded);
                bBlockStarted = true;
                bBlockIncluded = false;
                lines.Add(s);
                continue;
            } else if (!m_BlockEnd.IsEmpty() && strstr(line, m_BlockEnd)) {
                lines.Add(s);
                Flush(lines, bBlockIncluded);
                bBlockStarted = false;
                continue;
            } else {
                for (UINT i = 0; i < m_Filters.GetCount(); ++i) {
                    if (strstr(line, m_Filters[i])) {
                        if (bBlockStarted) {
                            bBlockIncluded = true;
                            break;
                        } else {
                            // the line is out of the block,
                            // include only this line
                            CStringArray oneLine;
                            oneLine.Add(s);
                            Flush(oneLine, true);
                        }
                    }
                }
                if (bBlockStarted)
                    lines.Add(s);
            }
        }

        return 0;
    }
private:
    CString m_BlockStart;
    CString m_BlockEnd;
    CStringArray m_Filters;
};

class CTextParseHandler : public CCommandHandler
{
public:
    CTextParseHandler() : CCommandHandler("parse-text", "\bparsing text based on patterns", 2) {}
private:
    int Run(const CStringArray& Parameters) override
    {
        CIniFileParser iniFile(Parameters[0]);
        return iniFile.Parse(Parameters[1]);
    }
    void Help(CStringArray& a) override
    {
        a.Add("<inifile> <textfile> (see Examples)");
    }
};

static CTextParseHandler fh;
