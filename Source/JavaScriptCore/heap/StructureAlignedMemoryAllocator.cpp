/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
#include "StructureAlignedMemoryAllocator.h"

#include "JSCConfig.h"
#include "MarkedBlock.h"

#include <wtf/OSAllocator.h>

namespace JSC {

StructureAlignedMemoryAllocator::StructureAlignedMemoryAllocator(CString name)
    : Base(name)
{
}

StructureAlignedMemoryAllocator::~StructureAlignedMemoryAllocator()
{
    releaseMemoryFromSubclassDestructor();
}

void StructureAlignedMemoryAllocator::dump(PrintStream& out) const
{
    out.print("Structure(", RawPointer(this), ")");
}

void* StructureAlignedMemoryAllocator::tryAllocateMemory(size_t)
{
    return nullptr;
}

void StructureAlignedMemoryAllocator::freeMemory(void*)
{
    // Structures do not support Precise allocations right now.
    RELEASE_ASSERT_NOT_REACHED();
}

void* StructureAlignedMemoryAllocator::tryReallocateMemory(void*, size_t)
{
    // Structures do not support Precise allocations right now.
    RELEASE_ASSERT_NOT_REACHED();
}

#if CPU(ADDRESS64)

class StructureMemoryManager {
public:
    StructureMemoryManager()
    {
        // Don't use the first page because zero is used as the empty StructureID and the first allocation will conflict.
        m_usedBlocks.set(0);
    }

    void* tryMallocStructureBlock()
    {
        size_t freeIndex;
        {
            Locker locker(m_lock);
            constexpr size_t startIndex = 0;
            freeIndex = m_usedBlocks.findBit(startIndex, 0);
            ASSERT(freeIndex <= m_usedBlocks.bitCount());
            if (freeIndex * MarkedBlock::blockSize >= structureHeapAddressSize)
                return nullptr;
            m_usedBlocks.set(freeIndex);
        }

        MarkedBlock* block = reinterpret_cast<MarkedBlock*>(g_jscConfig.startOfStructureHeap) + freeIndex * MarkedBlock::blockSize;
        constexpr bool writable = true;
        constexpr bool executable = false;
        OSAllocator::commit(block, MarkedBlock::blockSize, writable, executable);
        return block;
    }

    void freeStructureBlock(void* blockPtr)
    {
        OSAllocator::decommit(blockPtr, MarkedBlock::blockSize);
        uintptr_t block = reinterpret_cast<uintptr_t>(blockPtr);
        RELEASE_ASSERT(g_jscConfig.startOfStructureHeap <= block && block < g_jscConfig.startOfStructureHeap + structureHeapAddressSize);
        RELEASE_ASSERT(roundUpToMultipleOf<MarkedBlock::blockSize>(block) == block);

        Locker locker(m_lock);
        m_usedBlocks.quickClear((block - g_jscConfig.startOfStructureHeap) / MarkedBlock::blockSize);
    }

private:
    Lock m_lock;
    BitVector m_usedBlocks;
};

static LazyNeverDestroyed<StructureMemoryManager> s_structureMemoryManager;

void StructureAlignedMemoryAllocator::initializeStructureAddressSpace()
{
    static_assert(hasOneBitSet(structureHeapAddressSize));

    g_jscConfig.startOfStructureHeap = reinterpret_cast<uintptr_t>(OSAllocator::reserveUncommittedAligned(structureHeapAddressSize, OSAllocator::FastMallocPages));
    s_structureMemoryManager.construct();

    ASSERT((g_jscConfig.startOfStructureHeap & ~structureIDMask) == g_jscConfig.startOfStructureHeap);
}

void* StructureAlignedMemoryAllocator::tryMallocBlock()
{
    return s_structureMemoryManager->tryMallocStructureBlock();
}

void StructureAlignedMemoryAllocator::freeBlock(void* block)
{
    s_structureMemoryManager->freeStructureBlock(block);
}

void StructureAlignedMemoryAllocator::commitBlock(void* block)
{
    constexpr bool writable = true;
    constexpr bool executable = false;
    OSAllocator::commit(block, MarkedBlock::blockSize, writable, executable);
}

void StructureAlignedMemoryAllocator::decommitBlock(void* block)
{
    OSAllocator::decommit(block, MarkedBlock::blockSize);
}

#else // not CPU(ADDRESS64)

// FIXME: This is the same as IsoAlignedMemoryAllocator maybe we should just use that for 32-bit.

void StructureAlignedMemoryAllocator::initializeStructureAddressSpace()
{
    g_jscConfig.startOfStructureHeap = 0;
}

void* StructureAlignedMemoryAllocator::tryMallocBlock()
{
    return tryFastAlignedMalloc(MarkedBlock::blockSize, MarkedBlock::blockSize);
}

void StructureAlignedMemoryAllocator::freeBlock(void* block)
{
    fastAlignedFree(block);
}

void StructureAlignedMemoryAllocator::commitBlock(void* block)
{
    WTF::fastCommitAlignedMemory(block, MarkedBlock::blockSize);
}

void StructureAlignedMemoryAllocator::decommitBlock(void* block)
{
    WTF::fastDecommitAlignedMemory(block, MarkedBlock::blockSize);
}

#endif // CPU(ADDRESS64)

} // namespace JSC

