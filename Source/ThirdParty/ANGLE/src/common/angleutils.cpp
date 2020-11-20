//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
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
// dirtyPointer is a special value that will make the comparison with any valid pointer fail and
// force the renderer to re-apply the state.
const uintptr_t DirtyPointer = std::numeric_limits<uintptr_t>::max();
}  // namespace angle

std::string ArrayString(unsigned int i)
{
    // We assume that UINT_MAX and GL_INVALID_INDEX are equal.
    ASSERT(i != UINT_MAX);

    std::stringstream strstr;
    strstr << "[";
    strstr << i;
    strstr << "]";
    return strstr.str();
}

std::string ArrayIndexString(const std::vector<unsigned int> &indices)
{
    std::stringstream strstr;

    for (auto indicesIt = indices.rbegin(); indicesIt != indices.rend(); ++indicesIt)
    {
        // We assume that UINT_MAX and GL_INVALID_INDEX are equal.
        ASSERT(*indicesIt != UINT_MAX);
        strstr << "[";
        strstr << (*indicesIt);
        strstr << "]";
    }

    return strstr.str();
}

size_t FormatStringIntoVector(const char *fmt, va_list vararg, std::vector<char> &outBuffer)
{
    va_list varargCopy;
    va_copy(varargCopy, vararg);

    int len = vsnprintf(nullptr, 0, fmt, vararg);
    ASSERT(len >= 0);

    outBuffer.resize(len + 1, 0);

    len = vsnprintf(outBuffer.data(), outBuffer.size(), fmt, varargCopy);
    va_end(varargCopy);
    ASSERT(len >= 0);
    return static_cast<size_t>(len);
}
