/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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
#include "AlignedMemoryAllocator.h"

#include "BlockDirectory.h"
#include "HeapInlines.h"
#include "Options.h"
#include "Subspace.h"
#include <wtf/OSAllocator.h>
#include <wtf/StdLibExtras.h>

namespace JSC {

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(AlignedMemoryAllocator::Chunk);

AlignedMemoryAllocator::AlignedMemoryAllocator() = default;

// Concrete allocators must call releaseFreeChunks() from their own destructor: freeAlignedMemory()
// is virtual and is no longer dispatchable once the derived part of the object is gone.
AlignedMemoryAllocator::~AlignedMemoryAllocator()
{
    ASSERT(m_chunks.isEmpty());
}

unsigned AlignedMemoryAllocator::spansPerChunk() const
{
    return std::min(Options::markedBlockSpansPerChunk(), maxSpansPerChunk);
}

auto AlignedMemoryAllocator::tryAllocateChunk(size_t spanSize, size_t chunkSize) -> Chunk*
{
    void* base = tryAllocateAlignedMemory(chunkSize, chunkSize);
    if (!base)
        return nullptr;

    std::unique_ptr<Chunk>& entry = m_chunks.add(base, nullptr).iterator->value;
    RELEASE_ASSERT(!entry);
    entry = makeUnique<Chunk>();
    Chunk* result = entry.get();

    unsigned count = chunkSize / spanSize;
    ASSERT(count && count <= maxSpansPerChunk);
    result->base = base;
    result->freeBits = count == maxSpansPerChunk ? ~0ULL : (1ULL << count) - 1;
    result->freeCount = count;
    result->totalCount = count;

    // Fault the chunk in with one ascending pass. Doing it here rather than lazily per span
    // measures faster: the faults come back to back instead of interleaved with allocation work.
    for (unsigned i = 0; i < count; ++i)
        *reinterpret_cast<uint64_t*>(static_cast<char*>(base) + i * spanSize) = 0;

    m_partialChunks.append(base);
    return result;
}

void* AlignedMemoryAllocator::tryAllocateSpan(size_t spanSize)
{
    unsigned perChunk = spansPerChunk();
    if (perChunk <= 1)
        return tryAllocateAlignedMemory(spanSize, spanSize);

    size_t chunkSize = spanSize * perChunk;
    void* span;
    {
        Locker locker { m_chunkLock };

        Chunk* chunk = nullptr;
        while (!m_partialChunks.isEmpty()) {
            auto it = m_chunks.find(m_partialChunks.last());
            if (it != m_chunks.end() && it->value->freeCount) {
                chunk = it->value.get();
                break;
            }
            m_partialChunks.removeLast();
        }

        if (!chunk) {
            chunk = tryAllocateChunk(spanSize, chunkSize);
            // A chunk-sized request can fail while a span-sized one still succeeds, so fall back
            // rather than reporting exhaustion.
            if (!chunk)
                return tryAllocateAlignedMemory(spanSize, spanSize);
        }

        // Lowest free index first, so a directory's blocks stay in ascending address order.
        unsigned index = ctz(chunk->freeBits);
        chunk->freeBits &= ~(1ULL << index);
        span = static_cast<char*>(chunk->base) + index * spanSize;
        if (!--chunk->freeCount)
            m_partialChunks.removeLast();
    }
    return span;
}

void AlignedMemoryAllocator::freeSpan(void* basePtr, size_t spanSize)
{
    unsigned perChunk = spansPerChunk();
    if (perChunk <= 1) {
        freeAlignedMemory(basePtr);
        return;
    }

    size_t chunkSize = spanSize * perChunk;
    void* chunkBase = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(basePtr) & ~(chunkSize - 1));

    {
        Locker locker { m_chunkLock };
        auto it = m_chunks.find(chunkBase);
        if (it == m_chunks.end()) {
            // Allocated by the single-span fallback above, so it is not part of any chunk.
            freeAlignedMemory(basePtr);
            return;
        }

        Chunk& chunk = *it->value;
        unsigned index = (static_cast<char*>(basePtr) - static_cast<char*>(chunk.base)) / spanSize;
        ASSERT(index < chunk.totalCount);
        ASSERT(!(chunk.freeBits & (1ULL << index)));
        chunk.freeBits |= 1ULL << index;
        if (!chunk.freeCount++)
            m_partialChunks.append(chunkBase);

        if (chunk.freeCount != chunk.totalCount)
            return;

        // The chunk is entirely unused, so give it back rather than pinning its pages.
        m_chunks.remove(it);
        m_partialChunks.removeAll(chunkBase);
    }
    freeAlignedMemory(chunkBase);
}

void AlignedMemoryAllocator::purgeFreeSpans(size_t spanSize)
{
    unsigned perChunk = spansPerChunk();
    if (perChunk <= 1)
        return;

    Vector<void*> emptyChunks;
    Vector<std::pair<void*, unsigned>> toDecommit;
    {
        Locker locker { m_chunkLock };
        for (auto& entry : m_chunks) {
            Chunk& chunk = *entry.value;
            if (chunk.freeCount == chunk.totalCount) {
                emptyChunks.append(entry.key);
                continue;
            }
            for (uint64_t bits = chunk.freeBits; bits; bits &= bits - 1)
                toDecommit.append({ static_cast<char*>(chunk.base) + ctz(bits) * spanSize, 1 });
        }
        for (void* base : emptyChunks) {
            m_chunks.remove(base);
            m_partialChunks.removeAll(base);
        }
    }
    for (void* base : emptyChunks)
        freeAlignedMemory(base);
    // Only the physical pages go; the spans stay owned and on the free bitmap, so reusing one
    // simply faults it back in.
    for (auto& [span, count] : toDecommit)
        OSAllocator::decommit(span, spanSize * count);
}

void AlignedMemoryAllocator::releaseFreeChunks()
{
    Vector<void*> toFree;
    {
        Locker locker { m_chunkLock };
        for (auto& entry : m_chunks) {
            if (entry.value->freeCount == entry.value->totalCount)
                toFree.append(entry.key);
        }
        for (void* base : toFree) {
            m_chunks.remove(base);
            m_partialChunks.removeAll(base);
        }
    }
    for (void* base : toFree)
        freeAlignedMemory(base);
}

void AlignedMemoryAllocator::registerDirectory(JSC::Heap& heap, BlockDirectory* directory)
{
    RELEASE_ASSERT(!directory->nextDirectoryInAlignedMemoryAllocator());
    
    if (m_directories.isEmpty()) {
        ASSERT_UNUSED(heap, !Thread::mayBeGCThread() || heap.worldIsStopped());
        for (Subspace* subspace = m_subspaces.first(); subspace; subspace = subspace->nextSubspaceInAlignedMemoryAllocator())
            subspace->didCreateFirstDirectory(directory);
    }
    
    m_directories.append(std::mem_fn(&BlockDirectory::setNextDirectoryInAlignedMemoryAllocator), directory);
}

void AlignedMemoryAllocator::registerSubspace(Subspace* subspace)
{
    RELEASE_ASSERT(!subspace->nextSubspaceInAlignedMemoryAllocator());
    m_subspaces.append(std::mem_fn(&Subspace::setNextSubspaceInAlignedMemoryAllocator), subspace);
}

} // namespace JSC


