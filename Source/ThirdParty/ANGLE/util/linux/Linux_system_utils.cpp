//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Linux_system_utils.cpp: Implementation of OS-specific functions for Linux

#include "system_utils.h"

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

namespace angle
{

namespace
{

std::string GetExecutablePathImpl()
{
    // We cannot use lstat to get the size of /proc/self/exe as it always returns 0
    // so we just use a big buffer and hope the path fits in it.
    char path[4096];

    ssize_t result = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (result < 0 || static_cast<size_t>(result) >= sizeof(path) - 1)
    {
        return "";
    }

    path[result] = '\0';
    return path;
}

std::string GetExecutableDirectoryImpl()
{
    std::string executablePath = GetExecutablePath();
    size_t lastPathSepLoc = executablePath.find_last_of("/");
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
    return "so";
}

} // namespace angle
