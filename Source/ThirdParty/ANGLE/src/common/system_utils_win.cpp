//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
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

std::string GetExecutablePathImpl()
{
    std::array<char, MAX_PATH> executableFileBuf;
    DWORD executablePathLen = GetModuleFileNameA(nullptr, executableFileBuf.data(),
                                                 static_cast<DWORD>(executableFileBuf.size()));
    return (executablePathLen > 0 ? std::string(executableFileBuf.data()) : "");
}

std::string GetExecutableDirectoryImpl()
{
    std::string executablePath = GetExecutablePath();
    size_t lastPathSepLoc      = executablePath.find_last_of("\\/");
    return (lastPathSepLoc != std::string::npos) ? executablePath.substr(0, lastPathSepLoc) : "";
}

}  // anonymous namespace

const char *GetExecutablePath()
{
    // TODO(jmadill): Make global static string thread-safe.
    const static std::string &exePath = GetExecutablePathImpl();
    return exePath.c_str();
}

const char *GetExecutableDirectory()
{
    // TODO(jmadill): Make global static string thread-safe.
    const static std::string &exeDir = GetExecutableDirectoryImpl();
    return exeDir.c_str();
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

bool SetEnvironmentVar(const char *variableName, const char *value)
{
    return (SetEnvironmentVariableA(variableName, value) == TRUE);
}

}  // namespace angle
