//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// system_utils_win.cpp: Implementation of OS-specific functions for Windows

#include "system_utils.h"

#include <stdarg.h>
#include <windows.h>
#include <array>
#include <vector>

namespace angle
{

namespace
{

std::string GetPath(HMODULE module)
{
    std::array<char, MAX_PATH> executableFileBuf;
    DWORD executablePathLen = GetModuleFileNameA(module, executableFileBuf.data(),
                                                 static_cast<DWORD>(executableFileBuf.size()));
    return (executablePathLen > 0 ? std::string(executableFileBuf.data()) : "");
}

std::string GetDirectory(HMODULE module)
{
    std::string executablePath = GetPath(module);
    size_t lastPathSepLoc      = executablePath.find_last_of("\\/");
    return (lastPathSepLoc != std::string::npos) ? executablePath.substr(0, lastPathSepLoc) : "";
}

}  // anonymous namespace

std::string GetExecutablePath()
{
    return GetPath(nullptr);
}

std::string GetExecutableDirectory()
{
    return GetDirectory(nullptr);
}

const char *GetSharedLibraryExtension()
{
    return "dll";
}

Optional<std::string> GetCWD()
{
    std::array<char, MAX_PATH> pathBuf;
    DWORD result = GetCurrentDirectoryA(static_cast<DWORD>(pathBuf.size()), pathBuf.data());
    if (result == 0)
    {
        return Optional<std::string>::Invalid();
    }
    return std::string(pathBuf.data());
}

bool SetCWD(const char *dirName)
{
    return (SetCurrentDirectoryA(dirName) == TRUE);
}

const char *GetPathSeparatorForEnvironmentVar()
{
    return ";";
}

double GetCurrentSystemTime()
{
    LARGE_INTEGER frequency = {};
    QueryPerformanceFrequency(&frequency);

    LARGE_INTEGER curTime;
    QueryPerformanceCounter(&curTime);

    return static_cast<double>(curTime.QuadPart) / frequency.QuadPart;
}

double GetCurrentProcessCpuTime()
{
    FILETIME creationTime = {};
    FILETIME exitTime     = {};
    FILETIME kernelTime   = {};
    FILETIME userTime     = {};

    // Note this will not give accurate results if the current thread is
    // scheduled for less than the tick rate, which is often 15 ms. In that
    // case, GetProcessTimes will not return different values, making it
    // possible to end up with 0 ms for a process that takes 93% of a core
    // (14/15 ms)!  An alternative is QueryProcessCycleTime but there is no
    // simple way to convert cycles back to seconds, and on top of that, it's
    // not supported pre-Windows Vista.

    // Returns 100-ns intervals, so we want to divide by 1e7 to get seconds
    GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime);

    ULARGE_INTEGER kernelInt64;
    kernelInt64.LowPart      = kernelTime.dwLowDateTime;
    kernelInt64.HighPart     = kernelTime.dwHighDateTime;
    double systemTimeSeconds = static_cast<double>(kernelInt64.QuadPart) * 1e-7;

    ULARGE_INTEGER userInt64;
    userInt64.LowPart      = userTime.dwLowDateTime;
    userInt64.HighPart     = userTime.dwHighDateTime;
    double userTimeSeconds = static_cast<double>(userInt64.QuadPart) * 1e-7;

    return systemTimeSeconds + userTimeSeconds;
}

bool IsDirectory(const char *filename)
{
    WIN32_FILE_ATTRIBUTE_DATA fileInformation;

    BOOL result = GetFileAttributesExA(filename, GetFileExInfoStandard, &fileInformation);
    if (result)
    {
        DWORD attribs = fileInformation.dwFileAttributes;
        return (attribs != INVALID_FILE_ATTRIBUTES) && ((attribs & FILE_ATTRIBUTE_DIRECTORY) > 0);
    }

    return false;
}

bool IsDebuggerAttached()
{
    return !!::IsDebuggerPresent();
}

void BreakDebugger()
{
    __debugbreak();
}

const char *GetExecutableExtension()
{
    return ".exe";
}

char GetPathSeparator()
{
    return '\\';
}

std::string GetModuleDirectory()
{
// GetModuleHandleEx is unavailable on UWP
#if !defined(ANGLE_IS_WINUWP)
    static int placeholderSymbol = 0;
    HMODULE module               = nullptr;
    if (GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&placeholderSymbol), &module))
    {
        return GetDirectory(module);
    }
#endif
    return GetDirectory(nullptr);
}

std::string GetRootDirectory()
{
    return "C:\\";
}

}  // namespace angle
