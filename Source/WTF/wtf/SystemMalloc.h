/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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

#include <cstdlib>
#include <wtf/Assertions.h>
#include <wtf/Platform.h>

// Require that HAVE(TYPE_AWARE_MALLOC) from PlatformHave.h is enabled when
// _MALLOC_TYPE_ENABLED is true.
#if defined __has_include && __has_include(<malloc/malloc.h>)
#include <malloc/malloc.h>
#endif
#if USE(APPLE_INTERNAL_SDK) // FIXME: Remove when OS 26 or later is oldest supported OS.
#if !HAVE(TYPE_AWARE_MALLOC) && (defined(_MALLOC_TYPE_ENABLED) && _MALLOC_TYPE_ENABLED)
#error _MALLOC_TYPE_ENABLED is enabled without HAVE_TYPE_AWARE_MALLOC
#endif
#endif

#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
// Probabilistic Guard Malloc is not really enabled on older platforms but opt those to system malloc too for consistency.
#define HAVE_PROBABILISTIC_GUARD_MALLOC 1
#endif

namespace WTF {

// Non-template tag type for compatibility with MallocSpan.
struct SystemMalloc { };

template<typename T>
struct SystemMallocBase {
    static T* malloc(size_t size)
    {
        T* result = static_cast<T*>(::malloc(size));
        if (!result)
            CRASH();
        return result;
    }

    static T* tryMalloc(size_t size)
    {
        return static_cast<T*>(::malloc(size));
    }

    static T* zeroedMalloc(size_t size)
    {
        T* result = static_cast<T*>(::calloc(1, size));
        if (!result) [[unlikely]]
            CRASH();
        return result;
    }

    static T* tryZeroedMalloc(size_t size)
    {
        T* result = static_cast<T*>(::calloc(1, size));
        if (!result) [[unlikely]]
            return nullptr;
        return result;
    }

    static T* realloc(void* p, size_t size)
    {
        T* result = static_cast<T*>(::realloc(p, size));
        if (!result)
            CRASH();
        return result;
    }

    static T* tryRealloc(void* p, size_t size)
    {
        return static_cast<T*>(::realloc(p, size));
    }

    static void free(void* p)
    {
        ::free(p);
    }

    static constexpr ALWAYS_INLINE size_t nextCapacity(size_t capacity)
    {
        return capacity + capacity / 4 + 1;
    }
};

#if HAVE(PROBABILISTIC_GUARD_MALLOC)
using ProbabilisticGuardMalloc = SystemMalloc;
#endif

}

using WTF::SystemMalloc;
using WTF::SystemMallocBase;
#if HAVE(PROBABILISTIC_GUARD_MALLOC)
using WTF::ProbabilisticGuardMalloc;
#endif
