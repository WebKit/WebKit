/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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
#include "BlockDirectory.h"

#include "BlockDirectoryInlines.h"
#include "GCActivityCallback.h"
#include "Heap.h"
#include "IncrementalSweeper.h"
#include "JSCInlines.h"
#include "MarkedBlockInlines.h"
#include "SubspaceInlines.h"
#include "SuperSampler.h"
#include "VM.h"

namespace JSC {

BlockDirectory::BlockDirectory(Heap* heap, size_t cellSize)
    : m_cellSize(static_cast<unsigned>(cellSize))
    , m_heap(heap)
{
}

BlockDirectory::~BlockDirectory()
{
    auto locker = holdLock(m_localAllocatorsLock);
    while (!m_localAllocators.isEmpty())
        m_localAllocators.begin()->remove();
}

void BlockDirectory::setSubspace(Subspace* subspace)
{
    m_attributes = subspace->attributes();
    m_subspace = subspace;
}

bool BlockDirectory::isPagedOut(MonotonicTime deadline)
{
    unsigned itersSinceLastTimeCheck = 0;
    for (auto* block : m_blocks) {
        if (block)
            block->block().populatePage();
        ++itersSinceLastTimeCheck;
        if (itersSinceLastTimeCheck >= Heap::s_timeCheckResolution) {
            MonotonicTime currentTime = MonotonicTime::now();
            if (currentTime > deadline)
                return true;
            itersSinceLastTimeCheck = 0;
        }
    }
    return false;
}

MarkedBlock::Handle* BlockDirectory::findEmptyBlockToSteal()
{
    m_emptyCursor = m_empty.findBit(m_emptyCursor, true);
    if (m_emptyCursor >= m_blocks.size())
        return nullptr;
    return m_blocks[m_emptyCursor];
}

MarkedBlock::Handle* BlockDirectory::findBlockForAllocation(LocalAllocator& allocator)
{
    for (;;) {
        allocator.m_allocationCursor = (m_canAllocateButNotEmpty | m_empty).findBit(allocator.m_allocationCursor, true);
        if (allocator.m_allocationCursor >= m_blocks.size())
            return nullptr;
        
        size_t blockIndex = allocator.m_allocationCursor++;
        MarkedBlock::Handle* result = m_blocks[blockIndex];
        setIsCanAllocateButNotEmpty(NoLockingNecessary, blockIndex, false);
        return result;
    }
}

MarkedBlock::Handle* BlockDirectory::tryAllocateBlock()
{
    SuperSamplerScope superSamplerScope(false);
    
    MarkedBlock::Handle* handle = MarkedBlock::tryCreate(*m_heap, subspace()->alignedMemoryAllocator());
    if (!handle)
        return nullptr;
    
    markedSpace().didAddBlock(handle);
    
    return handle;
}

void BlockDirectory::addBlock(MarkedBlock::Handle* block)
{
    size_t index;
    if (m_freeBlockIndices.isEmpty()) {
        index = m_blocks.size();

        size_t oldCapacity = m_blocks.capacity();
        m_blocks.append(block);
        if (m_blocks.capacity() != oldCapacity) {
            forEachBitVector(
                NoLockingNecessary,
                [&] (FastBitVector& vector) {
                    ASSERT_UNUSED(vector, vector.numBits() == oldCapacity);
                });
            
            ASSERT(m_blocks.capacity() > oldCapacity);
            
            LockHolder locker(m_bitvectorLock);
            subspace()->didResizeBits(m_blocks.capacity());
            forEachBitVector(
                locker,
                [&] (FastBitVector& vector) {
                    vector.resize(m_blocks.capacity());
                });
        }
    } else {
        index = m_freeBlockIndices.takeLast();
        ASSERT(!m_blocks[index]);
        m_blocks[index] = block;
    }
    
    forEachBitVector(
        NoLockingNecessary,
        [&] (FastBitVector& vector) {
            ASSERT_UNUSED(vector, !vector[index]);
        });

    // This is the point at which the block learns of its cellSize() and attributes().
    block->didAddToDirectory(this, index);
    
    setIsLive(NoLockingNecessary, index, true);
    setIsEmpty(NoLockingNecessary, index, true);
}

void BlockDirectory::removeBlock(MarkedBlock::Handle* block)
{
    ASSERT(block->directory() == this);
    ASSERT(m_blocks[block->index()] == block);
    
    subspace()->didRemoveBlock(block->index());
    
    m_blocks[block->index()] = nullptr;
    m_freeBlockIndices.append(block->index());
    
    forEachBitVector(
        holdLock(m_bitvectorLock),
        [&] (FastBitVector& vector) {
            vector[block->index()] = false;
        });
    
    block->didRemoveFromDirectory();
}

void BlockDirectory::stopAllocating()
{
    if (false)
        dataLog(RawPointer(this), ": BlockDirectory::stopAllocating!\n");
    m_localAllocators.forEach(
        [&] (LocalAllocator* allocator) {
            allocator->stopAllocating();
        });
}

void BlockDirectory::prepareForAllocation()
{
    m_localAllocators.forEach(
        [&] (LocalAllocator* allocator) {
            allocator->prepareForAllocation();
        });
    
    m_unsweptCursor = 0;
    m_emptyCursor = 0;
    
    m_eden.clearAll();

    if (UNLIKELY(Options::useImmortalObjects())) {
        // FIXME: Make this work again.
        // https://bugs.webkit.org/show_bug.cgi?id=162296
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void BlockDirectory::stopAllocatingForGood()
{
    if (false)
        dataLog(RawPointer(this), ": BlockDirectory::stopAllocatingForGood!\n");
    
    m_localAllocators.forEach(
        [&] (LocalAllocator* allocator) {
            allocator->stopAllocatingForGood();
        });

    auto locker = holdLock(m_localAllocatorsLock);
    while (!m_localAllocators.isEmpty())
        m_localAllocators.begin()->remove();
}

void BlockDirectory::lastChanceToFinalize()
{
    forEachBlock(
        [&] (MarkedBlock::Handle* block) {
            block->lastChanceToFinalize();
        });
}

void BlockDirectory::resumeAllocating()
{
    m_localAllocators.forEach(
        [&] (LocalAllocator* allocator) {
            allocator->resumeAllocating();
        });
}

void BlockDirectory::beginMarkingForFullCollection()
{
    // Mark bits are sticky and so is our summary of mark bits. We only clear these during full
    // collections, so if you survived the last collection you will survive the next one so long
    // as the next one is eden.
    m_markingNotEmpty.clearAll();
    m_markingRetired.clearAll();
}

void BlockDirectory::endMarking()
{
    m_allocated.clearAll();
    
    // It's surprising and frustrating to comprehend, but the end-of-marking flip does not need to
    // know what kind of collection it is. That knowledge is already encoded in the m_markingXYZ
    // vectors.
    
    m_empty = m_live & ~m_markingNotEmpty;
    m_canAllocateButNotEmpty = m_live & m_markingNotEmpty & ~m_markingRetired;

    if (needsDestruction()) {
        // There are some blocks that we didn't allocate out of in the last cycle, but we swept them. This
        // will forget that we did that and we will end up sweeping them again and attempting to call their
        // destructors again. That's fine because of zapping. The only time when we cannot forget is when
        // we just allocate a block or when we move a block from one size class to another. That doesn't
        // happen here.
        m_destructible = m_live;
    }
    
    if (false) {
        dataLog("Bits for ", m_cellSize, ", ", m_attributes, " after endMarking:\n");
        dumpBits(WTF::dataFile());
    }
}

void BlockDirectory::snapshotUnsweptForEdenCollection()
{
    m_unswept |= m_eden;
}

void BlockDirectory::snapshotUnsweptForFullCollection()
{
    m_unswept = m_live;
}

MarkedBlock::Handle* BlockDirectory::findBlockToSweep()
{
    m_unsweptCursor = m_unswept.findBit(m_unsweptCursor, true);
    if (m_unsweptCursor >= m_blocks.size())
        return nullptr;
    return m_blocks[m_unsweptCursor];
}

void BlockDirectory::sweep()
{
    m_unswept.forEachSetBit(
        [&] (size_t index) {
            MarkedBlock::Handle* block = m_blocks[index];
            block->sweep(nullptr);
        });
}

void BlockDirectory::shrink()
{
    (m_empty & ~m_destructible).forEachSetBit(
        [&] (size_t index) {
            markedSpace().freeBlock(m_blocks[index]);
        });
}

void BlockDirectory::assertNoUnswept()
{
    if (ASSERT_DISABLED)
        return;
    
    if (m_unswept.isEmpty())
        return;
    
    dataLog("Assertion failed: unswept not empty in ", *this, ".\n");
    dumpBits();
    ASSERT_NOT_REACHED();
}

RefPtr<SharedTask<MarkedBlock::Handle*()>> BlockDirectory::parallelNotEmptyBlockSource()
{
    class Task : public SharedTask<MarkedBlock::Handle*()> {
    public:
        Task(BlockDirectory& directory)
            : m_directory(directory)
        {
        }
        
        MarkedBlock::Handle* run() override
        {
            if (m_done)
                return nullptr;
            auto locker = holdLock(m_lock);
            m_index = m_directory.m_markingNotEmpty.findBit(m_index, true);
            if (m_index >= m_directory.m_blocks.size()) {
                m_done = true;
                return nullptr;
            }
            return m_directory.m_blocks[m_index++];
        }
        
    private:
        BlockDirectory& m_directory;
        size_t m_index { 0 };
        Lock m_lock;
        bool m_done { false };
    };
    
    return adoptRef(new Task(*this));
}

void BlockDirectory::dump(PrintStream& out) const
{
    out.print(RawPointer(this), ":", m_cellSize, "/", m_attributes);
}

void BlockDirectory::dumpBits(PrintStream& out)
{
    unsigned maxNameLength = 0;
    forEachBitVectorWithName(
        NoLockingNecessary,
        [&] (FastBitVector&, const char* name) {
            unsigned length = strlen(name);
            maxNameLength = std::max(maxNameLength, length);
        });
    
    forEachBitVectorWithName(
        NoLockingNecessary,
        [&] (FastBitVector& vector, const char* name) {
            out.print("    ", name, ": ");
            for (unsigned i = maxNameLength - strlen(name); i--;)
                out.print(" ");
            out.print(vector, "\n");
        });
}

MarkedSpace& BlockDirectory::markedSpace() const
{
    return m_subspace->space();
}

bool BlockDirectory::isFreeListedCell(const void* target)
{
    bool result = false;
    m_localAllocators.forEach(
        [&] (LocalAllocator* allocator) {
            result |= allocator->isFreeListedCell(target);
        });
    return result;
}

} // namespace JSC

