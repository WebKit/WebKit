/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/PageBlock.h>

#if OS(UNIX)
#include <unistd.h>
#endif

#if OS(WINDOWS)
#include <malloc.h>
#include <windows.h>
#endif

namespace WTF {

static size_t s_pageSize;

#if OS(UNIX)

inline size_t systemPageSize()
{
    return sysconf(_SC_PAGESIZE);
}

#elif OS(WINDOWS)

inline size_t systemPageSize()
{
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    return system_info.dwPageSize;
}

#endif

size_t pageSize()
{
    if (!s_pageSize) {
        s_pageSize = systemPageSize();
        RELEASE_ASSERT(isPowerOfTwo(s_pageSize));
        RELEASE_ASSERT_WITH_MESSAGE(s_pageSize <= CeilingOnPageSize, "CeilingOnPageSize is too low, raise it in PageBlock.h!");
        RELEASE_ASSERT(roundUpToMultipleOf(s_pageSize, CeilingOnPageSize) == CeilingOnPageSize);
    }
    return s_pageSize;
}

} // namespace WTF
