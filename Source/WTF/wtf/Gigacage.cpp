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
const void* g_gigacageBasePtr;
}

namespace Gigacage {

void* tryMalloc(size_t size)
{
    auto result = tryFastMalloc(size);
    void* realResult;
    if (result.getValue(realResult))
        return realResult;
    return nullptr;
}

void* tryAllocateVirtualPages(size_t size)
{
    return OSAllocator::reserveUncommitted(size);
}

void freeVirtualPages(void* basePtr, size_t size)
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

void* tryAlignedMalloc(size_t alignment, size_t size)
{
    void* result = bmalloc::api::tryMemalign(alignment, size, bmalloc::HeapKind::Gigacage);
    WTF::compilerFence();
    return result;
}

void alignedFree(void* p)
{
    bmalloc::api::free(p, bmalloc::HeapKind::Gigacage);
    WTF::compilerFence();
}

void* tryMalloc(size_t size)
{
    void* result = bmalloc::api::tryMalloc(size, bmalloc::HeapKind::Gigacage);
    WTF::compilerFence();
    return result;
}

void free(void* p)
{
    bmalloc::api::free(p, bmalloc::HeapKind::Gigacage);
    WTF::compilerFence();
}

void* tryAllocateVirtualPages(size_t size)
{
    void* result = bmalloc::api::tryLargeMemalignVirtual(WTF::pageSize(), size, bmalloc::HeapKind::Gigacage);
    WTF::compilerFence();
    return result;
}

void freeVirtualPages(void* basePtr, size_t)
{
    bmalloc::api::freeLargeVirtual(basePtr, bmalloc::HeapKind::Gigacage);
    WTF::compilerFence();
}

} // namespace Gigacage
#endif

