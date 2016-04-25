/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

#include "PerProcess.h"
#include "VMHeap.h"
#include <thread>

namespace bmalloc {

XLargeRange VMHeap::tryAllocateLargeChunk(std::lock_guard<StaticMutex>& lock, size_t alignment, size_t size)
{
    // We allocate VM in aligned multiples to increase the chances that
    // the OS will provide contiguous ranges that we can merge.
    alignment = roundUpToMultipleOf<chunkSize>(alignment);
    size = roundUpToMultipleOf<chunkSize>(size);

    void* memory = tryVMAllocate(alignment, size);
    if (!memory)
        return XLargeRange();

    Chunk* chunk = new (memory) Chunk(lock);
    
#if BOS(DARWIN)
    m_zone.addChunk(chunk);
#endif

    return XLargeRange(chunk->bytes(), size, 0);
}

void VMHeap::allocateSmallChunk(std::lock_guard<StaticMutex>& lock, size_t pageClass)
{
    Chunk* chunk =
        new (vmAllocate(chunkSize, chunkSize)) Chunk(lock);

#if BOS(DARWIN)
    m_zone.addChunk(chunk);
#endif

    size_t pageSize = bmalloc::pageSize(pageClass);
    size_t smallPageCount = pageSize / smallPageSize;

    // We align to our page size in order to guarantee that we can service
    // aligned allocation requests at equal and smaller powers of two.
    size_t metadataSize = divideRoundingUp(sizeof(Chunk), pageSize) * pageSize;

    Object begin(chunk, metadataSize);
    Object end(chunk, chunkSize);

    for (Object it = begin; it + pageSize <= end; it = it + pageSize) {
        SmallPage* page = it.page();
        new (page) SmallPage;

        for (size_t i = 0; i < smallPageCount; ++i)
            page[i].setSlide(i);

        m_smallPages[pageClass].push(page);
    }
}

} // namespace bmalloc
