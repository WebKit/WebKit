//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "common/angleutils.h"
#include "common/debug.h"

#include <stdio.h>

#include <limits>
#include <vector>

namespace angle
{
const uintptr_t DirtyPointer = std::numeric_limits<uintptr_t>::max();
}

size_t FormatStringIntoVector(const char *fmt, va_list vararg, std::vector<char>& outBuffer)
{
    // The state of the va_list passed to vsnprintf is undefined after the call, do a copy in case
    // we need to grow the buffer.
    va_list varargCopy;
    va_copy(varargCopy, vararg);

    // Attempt to just print to the current buffer
    int len = vsnprintf(&(outBuffer.front()), outBuffer.size(), fmt, varargCopy);
    va_end(varargCopy);

    if (len < 0 || static_cast<size_t>(len) >= outBuffer.size())
    {
        // Buffer was not large enough, calculate the required size and resize the buffer
        len = vsnprintf(NULL, 0, fmt, vararg);
        outBuffer.resize(len + 1);

        // Print again
        va_copy(varargCopy, vararg);
        len = vsnprintf(&(outBuffer.front()), outBuffer.size(), fmt, varargCopy);
        va_end(varargCopy);
    }
    ASSERT(len >= 0);
    return static_cast<size_t>(len);
}

std::string FormatString(const char *fmt, va_list vararg)
{
    static std::vector<char> buffer(512);

    size_t len = FormatStringIntoVector(fmt, vararg, buffer);
    return std::string(&buffer[0], len);
}

std::string FormatString(const char *fmt, ...)
{
    va_list vararg;
    va_start(vararg, fmt);
    std::string result = FormatString(fmt, vararg);
    va_end(vararg);
    return result;
}
