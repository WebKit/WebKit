/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
// Probabilistic Guard Malloc is not really enabled on older platforms but opt those to system malloc too for consistency.
#define HAVE_PROBABILISTIC_GUARD_MALLOC 1
#endif

namespace WTF {

struct SystemMalloc {
    static void* malloc(size_t size)
    {
        auto* result = ::malloc(size);
        if (!result)
            CRASH();
        return result;
    }

    static void* tryMalloc(size_t size)
    {
        return ::malloc(size);
    }

    static void* zeroedMalloc(size_t size)
    {
        auto* result = ::malloc(size);
        if (!result)
            CRASH();
        memset(result, 0, size);
        return result;
    }

    static void* tryZeroedMalloc(size_t size)
    {
        auto* result = ::malloc(size);
        if (!result)
            return nullptr;
        memset(result, 0, size);
        return result;
    }

    static void* realloc(void* p, size_t size)
    {
        auto* result = ::realloc(p, size);
        if (!result)
            CRASH();
        return result;
    }

    static void* tryRealloc(void* p, size_t size)
    {
        return ::realloc(p, size);
    }

    static void free(void* p)
    {
        ::free(p);
    }
};

#if HAVE(PROBABILISTIC_GUARD_MALLOC)
using ProbabilisticGuardMalloc = SystemMalloc;
#endif

}

using WTF::SystemMalloc;
#if HAVE(PROBABILISTIC_GUARD_MALLOC)
using WTF::ProbabilisticGuardMalloc;
#endif
