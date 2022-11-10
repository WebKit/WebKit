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
#include "StructureID.h"

#if CPU(ADDRESS64) && !ENABLE(STRUCTURE_ID_WITH_SHIFT)
#include <wtf/NeverDestroyed.h>
#endif

#include <wtf/OSAllocator.h>

#if OS(UNIX) && ASSERT_ENABLED
#include <sys/mman.h>
#endif

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

#if CPU(ADDRESS64) && !ENABLE(STRUCTURE_ID_WITH_SHIFT)

class StructureMemoryManager {
public:
    StructureMemoryManager()
    {
        // Don't use the first page because zero is used as the empty StructureID and the first allocation will conflict.
        m_usedBlocks.set(0);

        uintptr_t mappedHeapSize = structureHeapAddressSize;
        for (unsigned i = 0; i < 8; ++i) {
            g_jscConfig.startOfStructureHeap = reinterpret_cast<uintptr_t>(OSAllocator::tryReserveUncommittedAligned(mappedHeapSize, structureHeapAddressSize, OSAllocator::FastMallocPages));
            if (g_jscConfig.startOfStructureHeap)
                break;
            mappedHeapSize /= 2;
        }
        g_jscConfig.sizeOfStructureHeap = mappedHeapSize;
        RELEASE_ASSERT(g_jscConfig.startOfStructureHeap && ((g_jscConfig.startOfStructureHeap & ~StructureID::structureIDMask) == g_jscConfig.startOfStructureHeap));
    }

    void* tryMallocStructureBlock()
    {
        size_t freeIndex;
        {
            Locker locker(m_lock);
            constexpr size_t startIndex = 0;
            freeIndex = m_usedBlocks.findBit(startIndex, 0);
            ASSERT(freeIndex <= m_usedBlocks.bitCount());
            RELEASE_ASSERT(g_jscConfig.sizeOfStructureHeap <= structureHeapAddressSize);
            if (freeIndex * MarkedBlock::blockSize >= g_jscConfig.sizeOfStructureHeap)
                return nullptr;
            // If we can't find a free block then `freeIndex == m_usedBlocks.bitCount()` and this set will grow the bit vector.
            m_usedBlocks.set(freeIndex);
        }

        auto* block = reinterpret_cast<uint8_t*>(g_jscConfig.startOfStructureHeap) + freeIndex * MarkedBlock::blockSize;
        commitBlock(block);
        return block;
    }

    void freeStructureBlock(void* blockPtr)
    {
        decommitBlock(blockPtr);
        uintptr_t block = reinterpret_cast<uintptr_t>(blockPtr);
        RELEASE_ASSERT(g_jscConfig.startOfStructureHeap <= block && block < g_jscConfig.startOfStructureHeap + g_jscConfig.sizeOfStructureHeap);
        RELEASE_ASSERT(roundUpToMultipleOf<MarkedBlock::blockSize>(block) == block);

        Locker locker(m_lock);
        m_usedBlocks.quickClear((block - g_jscConfig.startOfStructureHeap) / MarkedBlock::blockSize);
    }

    static void commitBlock(void* block)
    {
#if OS(UNIX) && ASSERT_ENABLED
        constexpr bool readable = true;
        constexpr bool writable = true;
        OSAllocator::protect(block, MarkedBlock::blockSize, readable, writable);
#else
        constexpr bool writable = true;
        constexpr bool executable = false;
        OSAllocator::commit(block, MarkedBlock::blockSize, writable, executable);
#endif
    }

    static void decommitBlock(void* block)
    {
#if OS(UNIX) && ASSERT_ENABLED
        constexpr bool readable = false;
        constexpr bool writable = false;
        OSAllocator::protect(block, MarkedBlock::blockSize, readable, writable);
#else
        OSAllocator::decommit(block, MarkedBlock::blockSize);
#endif
    }

private:
    Lock m_lock;
    BitVector m_usedBlocks;
};

static LazyNeverDestroyed<StructureMemoryManager> s_structureMemoryManager;

void StructureAlignedMemoryAllocator::initializeStructureAddressSpace()
{
    static_assert(hasOneBitSet(structureHeapAddressSize));
    s_structureMemoryManager.construct();
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
    StructureMemoryManager::commitBlock(block);
}

void StructureAlignedMemoryAllocator::decommitBlock(void* block)
{
    StructureMemoryManager::decommitBlock(block);
}

#else // not CPU(ADDRESS64)

// FIXME: This is the same as IsoAlignedMemoryAllocator maybe we should just use that for 32-bit.

void StructureAlignedMemoryAllocator::initializeStructureAddressSpace()
{
    g_jscConfig.startOfStructureHeap = 0;
    g_jscConfig.sizeOfStructureHeap = UINTPTR_MAX;
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

