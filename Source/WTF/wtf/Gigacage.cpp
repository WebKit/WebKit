/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
#include <wtf/Gigacage.h>

#include <wtf/Atomics.h>
#include <wtf/PageBlock.h>
#include <wtf/OSAllocator.h>

#if defined(USE_SYSTEM_MALLOC) && USE_SYSTEM_MALLOC

namespace Gigacage {

void* tryMalloc(Kind, size_t size)
{
    return FastMalloc::tryMalloc(size);
}

void* tryRealloc(Kind, void* pointer, size_t size)
{
    return FastMalloc::tryRealloc(pointer, size);
}

void* tryAllocateZeroedVirtualPages(Kind, size_t requestedSize)
{
    size_t size = roundUpToMultipleOf(WTF::pageSize(), requestedSize);
    RELEASE_ASSERT(size >= requestedSize);
    void* result = OSAllocator::reserveAndCommit(size);
#if ASSERT_ENABLED
    if (result) {
        for (size_t i = 0; i < size / sizeof(uintptr_t); ++i)
            ASSERT(static_cast<uintptr_t*>(result)[i] == 0);
    }
#endif
    return result;
}

void freeVirtualPages(Kind, void* basePtr, size_t size)
{
    OSAllocator::decommitAndRelease(basePtr, size);
}

} // namespace Gigacage
#else // defined(USE_SYSTEM_MALLOC) && USE_SYSTEM_MALLOC
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

void* tryRealloc(Kind kind, void* pointer, size_t size)
{
    void* result = bmalloc::api::tryRealloc(pointer, size, bmalloc::heapKind(kind));
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

void* tryAllocateZeroedVirtualPages(Kind kind, size_t size)
{
    void* result = bmalloc::api::tryLargeZeroedMemalignVirtual(WTF::pageSize(), size, bmalloc::heapKind(kind));
    WTF::compilerFence();
    return result;
}

void freeVirtualPages(Kind kind, void* basePtr, size_t size)
{
    if (!basePtr)
        return;
    RELEASE_ASSERT(isCaged(kind, basePtr));
    bmalloc::api::freeLargeVirtual(basePtr, size, bmalloc::heapKind(kind));
    WTF::compilerFence();
}

} // namespace Gigacage
#endif

namespace Gigacage {

void* tryMallocArray(Kind kind, size_t numElements, size_t elementSize)
{
    Checked<size_t, RecordOverflow> checkedSize = elementSize;
    checkedSize *= numElements;
    if (checkedSize.hasOverflowed())
        return nullptr;
    return tryMalloc(kind, checkedSize.unsafeGet());
}

void* malloc(Kind kind, size_t size)
{
    void* result = tryMalloc(kind, size);
    RELEASE_ASSERT(result);
    return result;
}

void* mallocArray(Kind kind, size_t numElements, size_t elementSize)
{
    void* result = tryMallocArray(kind, numElements, elementSize);
    RELEASE_ASSERT(result);
    return result;
}

} // namespace Gigacage

