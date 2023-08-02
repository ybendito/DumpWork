// DumpWork.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "stdafx.h"

static int Usage()
{
    puts("DumpWork action parameters...");
    puts("Actions:");
    puts("\tcr\t\t\tCompare 2 runaway files");
    puts("\t\t\t\tParameters: file1 file2");
    puts("\tcrd\t\t\tCompare all the runaway files in a directory");
    puts("\t\t\t\tParameters: directory");
    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 2)
        return Usage();
    if (!_stricmp(argv[1], "cr")) {
        return CompareRunaways(argc - 2, argv + 2, false);
    } else if (!_stricmp(argv[1], "crd")) {
        return CompareRunaways(argc - 2, argv + 2, true);
    }
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
