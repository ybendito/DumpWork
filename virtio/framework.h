#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define _CRT_SECURE_NO_WARNINGS  1

#include <afx.h>
#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions

#include <iostream>

#define KDEXT_64BIT
#include <WDBGEXTS.H>
#include <dbgeng.h>
#include <dbghelp.h>
//#include <engextcpp.hpp>
#include "names.h"

extern bool bVerbose;

#define __Log(fmt, ...) { CStringA _s_; _s_.Format(fmt "\n", __VA_ARGS__); OutputDebugStringA(_s_); }
#define ERR(fmt, ...) __Log("Error dbgext:" ## fmt, __VA_ARGS__)
#define LOG(fmt, ...) __Log("dbgext:" ## fmt, __VA_ARGS__)
#define VERBOSE(fmt, ...) { if (bVerbose) LOG(fmt, __VA_ARGS__); }

#define ELEMENTS_IN(x) sizeof(x)/sizeof(x[0])

#include "..\utils.h"
