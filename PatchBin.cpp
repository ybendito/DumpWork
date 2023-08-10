#include "stdafx.h"

struct tBinaryBuffer
{
    UCHAR buf[8];
    UCHAR len;
};

static bool ReadBuf(tBinaryBuffer& Buf, const char* Text)
{
    ULONG buf[sizeof(Buf.buf)] = {};
    int res = sscanf(Text, "%02X %02X %02X %02X %02X %02X %02X %02X",
        buf, buf + 1, buf + 2, buf + 3, buf + 4, buf + 5, buf + 6, buf + 7);
    LOG("Got %d bytes from \"%s\"", res, Text);
    if (!res) {
        return false;
    }
    Buf.len = res;
    for (UINT i = 0; i < Buf.len; ++i) {
        Buf.buf[i] = (UCHAR)buf[i];
    }
    return true;
}

static LONG Find(UCHAR* FileBuffer, INT Size, const tBinaryBuffer& Pattern, LONG Offset)
{
    if (Size < (Pattern.len + Offset))
        return -1;
    for (LONG i = Offset; i < (Size - Pattern.len); ++i) {
        if (!memcmp((FileBuffer + i), Pattern.buf, Pattern.len)) {
            return i;
        }
    }
    return -1;
}

static long File2Memory(FILE* File, UCHAR* Buffer, LONG Size)
{
    long res = 0;
    while (Size) {
        LONG done = (LONG)fread(Buffer, 1, Size, File);
        if (done > 0) {
            Buffer += done;
            Size -= done;
            res += done;
        } else {
            ERR("%d read of %d, errno %d", done, Size, errno);
            return res;
        }
    }
    return res;
}

static int PatchBin(const char *BinFile, const char *Src, const char* Dest, const char* TargetChunk)
{
    FILE* f = NULL;
    int res = 1;
    UCHAR* fbuf = NULL;
    ULONG offset = 0, offsetFound = 0;
    ULONG nResults = 0;
    ULONG nTargetChunk = 0;
    long size;

    if (TargetChunk) {
        nTargetChunk = atoi(TargetChunk);
        LOG("Target chunk %d", nTargetChunk);
    }
    f = fopen(BinFile, "r+b");
    if (!f) {
        ERR("Cannot open %s", BinFile);
        return 1;
    }
    tBinaryBuffer src = {}, dest = {};
    if (!ReadBuf(src, Src) || !ReadBuf(dest, Dest) || src.len != dest.len) {
        goto done;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    if (size <= src.len) {
        ERR("The file is smaller than the pattern");
        goto done;
    }
    fseek(f, 0, SEEK_SET);
    fbuf = (UCHAR *)malloc(size);
    if (!fbuf) {
        ERR("Cannot allocate the memory for the file");
        goto done;
    }
    res = File2Memory(f, fbuf, size);
    if (res != size) {
        ERR("Cannot copy the file to the memory, size is %d, loaded %d", size, res);
        res = 1;
        goto done;
    }
    LOG("Loaded file image of %d", size);
    do
    {
        LONG next;
        next = Find(fbuf, size, src, offset);
        if (next < 0)
            break;
        nResults++;
        offsetFound = next;
        offset = next + 1;
        LOG("Found the source pattern at %X", offsetFound);
        if (nResults == nTargetChunk)
            break;
    } while (true);

    if (!nResults) {
        LOG("Source pattern not found");
        goto done;
    }
    if (nResults == 1 || nResults == nTargetChunk) {
        memcpy(fbuf + offsetFound, dest.buf, dest.len);
        fseek(f, 0, SEEK_SET);
        long written = (long)fwrite(fbuf, 1, size, f);
        LOG("Written image of %d", written);
        res = 0;
    }
done:
    if (f) {
        fclose(f);
    }
    if (fbuf) {
        free(fbuf);
    }
    return res;
}

class CPatchHandler : public CCommandHandler
{
public:
    CPatchHandler() : CCommandHandler("patch", "Patch binary", 3) {}
private:
    int Run(const CStringArray& Parameters) override
    {
        const char* target = (Parameters.GetCount() > 3) ? Parameters[3] : NULL;
        return PatchBin(Parameters[0], Parameters[1], Parameters[2], target);
    }
    void Help(CStringArray& a) override
    {
        a.Add("<name> <pattern> <pattern> [target chunk]");
    }
};

static CPatchHandler ph;
