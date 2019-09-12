//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Windows_system_utils.cpp: Implementation of OS-specific functions for Windows

#include "util/system_utils.h"

#include <stdarg.h>
#include <windows.h>
#include <array>
#include <vector>

namespace angle
{

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
