/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "Gigacage.h"

#include <wtf/Atomics.h>
#include <wtf/PageBlock.h>
#include <wtf/OSAllocator.h>

#if defined(USE_SYSTEM_MALLOC) && USE_SYSTEM_MALLOC

extern "C" {
void* g_gigacageBasePtr;
}

namespace Gigacage {

void* tryMalloc(Kind, size_t size)
{
    auto result = tryFastMalloc(size);
    void* realResult;
    if (result.getValue(realResult))
        return realResult;
    return nullptr;
}

void* tryAllocateVirtualPages(Kind, size_t size)
{
    return OSAllocator::reserveUncommitted(size);
}

void freeVirtualPages(Kind, void* basePtr, size_t size)
{
    OSAllocator::releaseDecommitted(basePtr, size);
}

} // namespace Gigacage
#else
#include <bmalloc/bmalloc.h>

namespace Gigacage {

// FIXME: Pointers into the primitive gigacage must be scrambled right after being returned from malloc,
// and stay scrambled except just before use.
// https://bugs.webkit.org/show_bug.cgi?id=175035

void* tryAlignedMalloc(Kind kind, size_t alignment, size_t size)
{
    void* result = bmalloc::api::tryMemalign(alignment, size, bmalloc::heapKind(kind));
    WTF::compilerFence();
    return result;
}

void alignedFree(Kind kind, void* p)
{
    if (!p)
        return;
    RELEASE_ASSERT(isCaged(kind, p));
    bmalloc::api::free(p, bmalloc::heapKind(kind));
    WTF::compilerFence();
}

void* tryMalloc(Kind kind, size_t size)
{
    void* result = bmalloc::api::tryMalloc(size, bmalloc::heapKind(kind));
    WTF::compilerFence();
    return result;
}

void free(Kind kind, void* p)
{
    if (!p)
        return;
    RELEASE_ASSERT(isCaged(kind, p));
    bmalloc::api::free(p, bmalloc::heapKind(kind));
    WTF::compilerFence();
}

void* tryAllocateVirtualPages(Kind kind, size_t size)
{
    void* result = bmalloc::api::tryLargeMemalignVirtual(WTF::pageSize(), size, bmalloc::heapKind(kind));
    WTF::compilerFence();
    return result;
}

void freeVirtualPages(Kind kind, void* basePtr, size_t)
{
    if (!basePtr)
        return;
    RELEASE_ASSERT(isCaged(kind, basePtr));
    bmalloc::api::freeLargeVirtual(basePtr, bmalloc::heapKind(kind));
    WTF::compilerFence();
}

} // namespace Gigacage
#endif

