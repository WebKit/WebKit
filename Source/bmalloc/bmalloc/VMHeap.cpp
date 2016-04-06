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

#include "LargeObject.h"
#include "PerProcess.h"
#include "VMHeap.h"
#include <thread>

namespace bmalloc {

VMHeap::VMHeap()
    : m_largeObjects(VMState::HasPhysical::False)
{
}

LargeObject VMHeap::allocateChunk(std::lock_guard<StaticMutex>& lock)
{
    Chunk* chunk =
        new (vmAllocate(chunkSize, chunkSize)) Chunk(lock, ObjectType::Large);

#if BOS(DARWIN)
    m_zone.addChunk(chunk);
#endif

    size_t alignment = largeAlignment;
    size_t metadataSize = roundUpToMultipleOf(alignment, sizeof(Chunk));

    Range range(chunk->bytes() + metadataSize, chunkSize - metadataSize);
    BASSERT(range.size() <= largeObjectMax);

    BeginTag* beginTag = Chunk::beginTag(range.begin());
    beginTag->setRange(range);
    beginTag->setFree(true);
    beginTag->setVMState(VMState::Virtual);

    EndTag* endTag = Chunk::endTag(range.begin(), range.size());
    endTag->init(beginTag);

    // Mark the left and right edges of our range as allocated. This naturally
    // prevents merging logic from overflowing left (into metadata) or right
    // (beyond our chunk), without requiring special-case checks.

    EndTag* leftSentinel = beginTag->prev();
    BASSERT(leftSentinel >= chunk->boundaryTags().begin());
    BASSERT(leftSentinel < chunk->boundaryTags().end());
    leftSentinel->initSentinel();

    BeginTag* rightSentinel = endTag->next();
    BASSERT(rightSentinel >= chunk->boundaryTags().begin());
    BASSERT(rightSentinel < chunk->boundaryTags().end());
    rightSentinel->initSentinel();

    return LargeObject(range.begin());
}

void VMHeap::allocateSmallChunk(std::lock_guard<StaticMutex>& lock, size_t pageClass)
{
    Chunk* chunk =
        new (vmAllocate(chunkSize, chunkSize)) Chunk(lock, ObjectType::Small);

#if BOS(DARWIN)
    m_zone.addChunk(chunk);
#endif

    size_t pageSize = bmalloc::pageSize(pageClass);
    size_t smallPageCount = pageSize / smallPageSize;

    // If our page size is a power of two, we align to it in order to guarantee
    // that we can service aligned allocation requests at the same power of two.
    size_t alignment = vmPageSizePhysical();
    if (isPowerOfTwo(pageSize))
        alignment = pageSize;
    size_t metadataSize = roundUpToMultipleOf(alignment, sizeof(Chunk));

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
