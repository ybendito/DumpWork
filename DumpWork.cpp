#include "stdafx.h"

struct tCommandEntry
{
    LPCSTR Shortcut;
    LPCSTR Desc;
    LPCSTR Help;
    int (*Proc)(int argc, char** argv);
};

tCommandEntry Commands[] =
{
    { "cr", "Compare 2 runaways files", "<file1> <file2>", [](int argc, char** argv) -> int { return CompareRunaways(argc, argv, false); }},
    { "crd", "Compare all runaways files in a directory", "<directory>", [](int argc, char** argv) -> int { return CompareRunaways(argc, argv, true); }},
    { "load", "Load CPU by threads", "<num of threads>", CreateThreads },
    { "perf", "Monitor perf counters", "<name> ... <name>", PerfCounter },
    { "patch", "Patch binary", "<name> <pattern> <pattern> [target chunk]", PatchBin },
};

static int Usage(LPCSTR ShortCut = NULL)
{
    for (UINT i = 0; i < ARRAYSIZE(Commands); ++i) {
        auto& cmd = Commands[i];
        if (!ShortCut) {
            printf("%s\t%s\n", cmd.Shortcut, cmd.Desc);
        } else if (!_stricmp(ShortCut, cmd.Shortcut)) {
            printf("%s\n", cmd.Desc);
            printf("%s %s\n", cmd.Shortcut, cmd.Help);
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 2)
        return Usage();
    for (UINT i = 0; i < ARRAYSIZE(Commands); ++i) {
        auto& cmd = Commands[i];
        if (!_stricmp(argv[1], cmd.Shortcut)) {
            if (argc > 2 && strchr(argv[2], '?')) {
                return Usage(cmd.Shortcut);
            }
            return cmd.Proc(argc - 2, argv + 2);
        }
    }
    return Usage();
}

