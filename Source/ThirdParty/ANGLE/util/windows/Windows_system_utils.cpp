//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Windows_system_utils.cpp: Implementation of OS-specific functions for Windows

#include "system_utils.h"

#include <stdarg.h>
#include <windows.h>
#include <array>
#include <vector>

namespace angle
{

namespace
{

std::string GetExecutablePathImpl()
{
    std::array<char, MAX_PATH> executableFileBuf;
    DWORD executablePathLen = GetModuleFileNameA(NULL, executableFileBuf.data(),
                                                 static_cast<DWORD>(executableFileBuf.size()));
    return (executablePathLen > 0 ? std::string(executableFileBuf.data()) : "");
}

std::string GetExecutableDirectoryImpl()
{
    std::string executablePath = GetExecutablePath();
    size_t lastPathSepLoc = executablePath.find_last_of("\\/");
    return (lastPathSepLoc != std::string::npos) ? executablePath.substr(0, lastPathSepLoc) : "";
}

}  // anonymous namespace

const char *GetExecutablePath()
{
    const static std::string &exePath = GetExecutablePathImpl();
    return exePath.c_str();
}

const char *GetExecutableDirectory()
{
    const static std::string &exeDir = GetExecutableDirectoryImpl();
    return exeDir.c_str();
}

const char *GetSharedLibraryExtension()
{
    return "dll";
}

void Sleep(unsigned int milliseconds)
{
    ::Sleep(static_cast<DWORD>(milliseconds));
}

void WriteDebugMessage(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int size = vsnprintf(nullptr, 0, format, args);
    va_end(args);

    std::vector<char> buffer(size + 2);
    va_start(args, format);
    vsnprintf(buffer.data(), size + 1, format, args);
    va_end(args);

    OutputDebugStringA(buffer.data());
}

}  // namespace angle
