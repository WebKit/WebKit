/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/FastMalloc.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/PrintStream.h>
#include <wtf/SinglyLinkedListWithTail.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace JSC {

class BlockDirectory;
class Heap;
class Subspace;

class AlignedMemoryAllocator {
    WTF_MAKE_NONCOPYABLE(AlignedMemoryAllocator);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(AlignedMemoryAllocator);
public:
    AlignedMemoryAllocator();
    virtual ~AlignedMemoryAllocator();

    virtual void* tryAllocateAlignedMemory(size_t alignment, size_t size) = 0;
    virtual void freeAlignedMemory(void*) = 0;

    // MarkedBlock-sized spans are carved out of much larger chunks. Asking the underlying
    // allocator for one block at a time costs ~500ns, because a block is too large for any
    // size-class fast path and so takes a general variable-size allocation path on every call;
    // amortizing that over a whole chunk turns block acquisition into a bit scan. Spans freed
    // back here also keep their pages resident, so reusing one skips the fault the OS would
    // otherwise charge for a fresh page.
    //
    // The cache deliberately lives here, per allocator (and so per VM), rather than process
    // wide: a block handed back to the thread that just used it is still warm in that core's
    // caches, which a shared pool would throw away.
    void* tryAllocateSpan(size_t spanSize);
    void freeSpan(void*, size_t spanSize);
    // Hands every fully free chunk back, and releases the physical pages behind the free spans
    // of chunks that are still partly in use. For memory pressure, where footprint beats speed.
    void purgeFreeSpans(size_t spanSize);
    void releaseFreeChunks();

    // This can't be pure virtual as it breaks our Dumpable concept.
    // FIXME: Make this virtual after we stop suppporting the Montery Clang.
    virtual void dump(PrintStream&) const { }

    void registerDirectory(Heap&, BlockDirectory*);
    BlockDirectory* firstDirectory() const LIFETIME_BOUND { return m_directories.first(); }

    void registerSubspace(Subspace*);

    // Some of derived memory allocators do not have these features because they do not use them.
    // For example, IsoAlignedMemoryAllocator does not have "realloc" feature since it never extends / shrinks the allocated memory region.
    virtual void* tryAllocateMemory(size_t) = 0;
    virtual void freeMemory(void*) = 0;
    virtual void* tryReallocateMemory(void*, size_t) = 0;

protected:
    // An allocator that cannot satisfy a request larger than a single span returns 1, which
    // routes every span straight to tryAllocateAlignedMemory().
    virtual unsigned spansPerChunk() const;

private:
    // Which spans are free is tracked out of line, in a bitmap, so that a free span holds no
    // metadata of ours and its pages can be handed back to the OS under memory pressure.
    static constexpr unsigned maxSpansPerChunk = 64;

    struct Chunk {
        WTF_MAKE_STRUCT_TZONE_ALLOCATED(Chunk);
        void* base { nullptr };
        uint64_t freeBits { 0 };
        unsigned freeCount { 0 };
        unsigned totalCount { 0 };
    };

    Chunk* tryAllocateChunk(size_t spanSize, size_t chunkSize) WTF_REQUIRES_LOCK(m_chunkLock);

    Lock m_chunkLock;
    // Keyed by chunk base address, which a span recovers by masking off the chunk size.
    UncheckedKeyHashMap<void*, std::unique_ptr<Chunk>> m_chunks WTF_GUARDED_BY_LOCK(m_chunkLock);
    // Chunks with at least one free span, so allocation never scans the whole map.
    Vector<void*> m_partialChunks WTF_GUARDED_BY_LOCK(m_chunkLock);

    SinglyLinkedListWithTail<BlockDirectory> m_directories;
    SinglyLinkedListWithTail<Subspace> m_subspaces;
};

} // namespace WTF

