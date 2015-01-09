/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "BPlatform.h"
#include "Environment.h"
#include <cstdlib>
#include <cstring>
#if BOS(DARWIN)
#include <mach-o/dyld.h>
#endif

namespace bmalloc {

static bool isMallocEnvironmentVariableSet()
{
    const char* list[] = {
        "Malloc",
        "MallocLogFile",
        "MallocGuardEdges",
        "MallocDoNotProtectPrelude",
        "MallocDoNotProtectPostlude",
        "MallocStackLogging",
        "MallocStackLoggingNoCompact",
        "MallocStackLoggingDirectory",
        "MallocScribble",
        "MallocCheckHeapStart",
        "MallocCheckHeapEach",
        "MallocCheckHeapSleep",
        "MallocCheckHeapAbort",
        "MallocErrorAbort",
        "MallocCorruptionAbort",
        "MallocHelp"
    };
    size_t size = sizeof(list) / sizeof(const char*);
    
    for (size_t i = 0; i < size; ++i) {
        if (getenv(list[i]))
            return true;
    }

    return false;
}

static bool isLibgmallocEnabled()
{
    char* variable = getenv("DYLD_INSERT_LIBRARIES");
    if (!variable)
        return false;
    if (!strstr(variable, "libgmalloc"))
        return false;
    return true;
}

static bool isASanEnabled()
{
#if BOS(DARWIN)
    uint32_t imageCount = _dyld_image_count();
    for (uint32_t i = 0; i < imageCount; ++i) {
        if (strstr(_dyld_get_image_name(i), "/libclang_rt.asan_"))
            return true;
    }
    return false;
#else
    return false;
#endif
}

Environment::Environment()
    : m_isBmallocEnabled(computeIsBmallocEnabled())
{
}

bool Environment::computeIsBmallocEnabled()
{
    if (isMallocEnvironmentVariableSet())
        return false;
    if (isLibgmallocEnabled())
        return false;
    if (isASanEnabled())
        return false;
    return true;
}

} // namespace bmalloc
