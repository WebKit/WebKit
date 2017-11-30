//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// system_utils_osx.cpp: Implementation of OS-specific functions for OSX

#include "system_utils.h"

#include <unistd.h>

#include <cstdlib>
#include <mach-o/dyld.h>
#include <vector>

#include <array>

namespace angle
{

namespace
{

std::string GetExecutablePathImpl()
{
    std::string result;

    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);

    std::vector<char> buffer;
    buffer.resize(size + 1);

    _NSGetExecutablePath(buffer.data(), &size);
    buffer[size] = '\0';

    if (!strrchr(buffer.data(), '/'))
    {
        return "";
    }
    return buffer.data();
}

std::string GetExecutableDirectoryImpl()
{
    std::string executablePath = GetExecutablePath();
    size_t lastPathSepLoc      = executablePath.find_last_of("/");
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
    return "dylib";
}

Optional<std::string> GetCWD()
{
    std::array<char, 4096> pathBuf;
    char *result = getcwd(pathBuf.data(), pathBuf.size());
    if (result == nullptr)
    {
        return Optional<std::string>::Invalid();
    }
    return std::string(pathBuf.data());
}

bool SetCWD(const char *dirName)
{
    return (chdir(dirName) == 0);
}

bool SetEnvironmentVar(const char *variableName, const char *value)
{
    return (setenv(variableName, value, 1) == 0);
}

}  // namespace angle
