#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#include <afx.h>
#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions

#include <iostream>

#include <dbgeng.h>
#include <dbghelp.h>

#define Log(fmt, ...) { CStringA _s_; _s_.Format(fmt "\n", __VA_ARGS__); OutputDebugStringA(_s_); }
#define ERR(fmt, ...) Log("Error dbgext:" ## fmt ## "\n", __VA_ARGS__)
#define LOG(fmt, ...) Log("dbgext:" ## fmt ## "\n", __VA_ARGS__)
