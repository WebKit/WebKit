//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "path_utils.h"
#include <array>
#include <windows.h>

std::string GetExecutablePath()
{
    std::array<char, MAX_PATH> executableFileBuf;
    DWORD executablePathLen = GetModuleFileNameA(NULL, executableFileBuf.data(), executableFileBuf.size());
    return (executablePathLen > 0 ? std::string(executableFileBuf.data()) : "");
}

std::string GetExecutableDirectory()
{
    std::string executablePath = GetExecutablePath();
    size_t lastPathSepLoc = executablePath.find_last_of("\\/");
    return (lastPathSepLoc != std::string::npos) ? executablePath.substr(0, lastPathSepLoc) : "";
}
