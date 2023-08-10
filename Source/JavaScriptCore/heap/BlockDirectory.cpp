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
#include "ConcurrentSweeper.h"
#include "Heap.h"
#include "MarkedSpaceInlines.h"
#include "SubspaceInlines.h"
#include "SuperSampler.h"

#include <wtf/FunctionTraits.h>
#include <wtf/Lock.h>
#include <wtf/SimpleStats.h>

namespace JSC {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(BlockDirectory);

BlockDirectory::BlockDirectory(size_t cellSize)
    : m_cellSize(static_cast<unsigned>(cellSize))
{
}

BlockDirectory::~BlockDirectory()
{
    Locker locker { m_localAllocatorsLock };
    while (!m_localAllocators.isEmpty())
        m_localAllocators.begin()->remove();
}

void BlockDirectory::setSubspace(Subspace* subspace)
{
    m_attributes = subspace->attributes();
    m_subspace = subspace;
}

void BlockDirectory::updatePercentageOfPagedOutPages(SimpleStats& stats)
{
    // FIXME: We should figure out a solution for Windows and PlayStation.
#if OS(UNIX) && !PLATFORM(PLAYSTATION)
    size_t pageSize = WTF::pageSize();
    ASSERT(!(MarkedBlock::blockSize % pageSize));
    auto numberOfPagesInMarkedBlock = MarkedBlock::blockSize / pageSize;
    // For some reason this can be unsigned char or char on different OSes...
    using MincoreBufferType = std::remove_pointer_t<FunctionTraits<decltype(mincore)>::ArgumentType<2>>;
    static_assert(std::is_same_v<std::make_unsigned_t<MincoreBufferType>, unsigned char>);
    Vector<MincoreBufferType, 16> pagedBits(numberOfPagesInMarkedBlock, MincoreBufferType { });

    for (auto* handle : m_blocks) {
        if (!handle)
            continue;

        auto* pageStart = handle->pageStart();
        auto markedBlockSizeInBytes = handle->backingStorageSize();
        RELEASE_ASSERT(markedBlockSizeInBytes / pageSize <= numberOfPagesInMarkedBlock);
        // We could cache this in bulk (e.g. 25 MB chunks) but we haven't seen any data that it actually matters.
        auto result = mincore(pageStart, markedBlockSizeInBytes, pagedBits.data());
        RELEASE_ASSERT(!result);
        constexpr unsigned pageIsResidentAndNotCompressed = 1;
        for (unsigned i = 0; i < numberOfPagesInMarkedBlock; ++i)
            stats.add(!(pagedBits[i] & pageIsResidentAndNotCompressed));
    }
#else
    UNUSED_PARAM(stats);
#endif
}

MarkedBlock::Handle* BlockDirectory::findEmptyBlockToSteal()
{
    Locker locker(bitvectorLock());
    m_emptyCursor = m_bits.empty().findBit(m_emptyCursor, true);
    if (m_emptyCursor >= m_blocks.size())
        return nullptr;
    setIsInUse(locker, m_emptyCursor, true);
    return m_blocks[m_emptyCursor];
}

MarkedBlock::Handle* BlockDirectory::findBlockForAllocation(LocalAllocator& allocator)
{
    Locker locker(bitvectorLock());
    for (;;) {
        allocator.m_allocationCursor = ((m_bits.canAllocateButNotEmpty() | m_bits.empty()) & ~m_bits.inUse()).findBit(allocator.m_allocationCursor, true);
        if (allocator.m_allocationCursor >= m_blocks.size())
            return nullptr;
        
        unsigned blockIndex = allocator.m_allocationCursor++;
        MarkedBlock::Handle* result = m_blocks[blockIndex];
        setIsCanAllocateButNotEmpty(locker, blockIndex, false);
        setIsInUse(locker, blockIndex, true);
        return result;
    }
}

MarkedBlock::Handle* BlockDirectory::tryAllocateBlock(Heap& heap)
{
    MarkedBlock::Handle* handle = MarkedBlock::tryCreate(heap, subspace()->alignedMemoryAllocator());
    if (!handle)
        return nullptr;
    
    markedSpace().didAddBlock(handle);
    
    return handle;
}

void BlockDirectory::addBlock(MarkedBlock::Handle* block)
{
    unsigned index;
    if (m_freeBlockIndices.isEmpty()) {
        index = m_blocks.size();

        size_t oldCapacity = m_blocks.capacity();
        m_blocks.append(block);
        if (m_blocks.capacity() != oldCapacity) {
            ASSERT(m_bits.numBits() == oldCapacity);
            ASSERT(m_blocks.capacity() > oldCapacity);
            
            Locker locker { m_bitvectorLock };
            subspace()->didResizeBits(m_blocks.capacity());
            m_bits.resize(m_blocks.capacity());
        }
    } else {
        index = m_freeBlockIndices.takeLast();
        ASSERT(!m_blocks[index]);
        m_blocks[index] = block;
    }
    
    Locker locker { m_bitvectorLock };
    forEachBitVector(
        locker,
        [&](auto vectorRef) {
            ASSERT_UNUSED(vectorRef, !vectorRef[index]);
        });

    // This is the point at which the block learns of its cellSize() and attributes().
    block->didAddToDirectory(this, index);
    
    setIsLive(locker, index, true);
    setIsEmpty(locker, index, true);
    setIsInUse(locker, index, true);
}

void BlockDirectory::removeBlock(MarkedBlock::Handle* block, WillDeleteBlock willDelete)
{
    ASSERT(block->directory() == this);
    ASSERT(m_blocks[block->index()] == block);
    
    subspace()->didRemoveBlock(block->index());
    
    m_blocks[block->index()] = nullptr;
    m_freeBlockIndices.append(block->index());
    
    forEachBitVector(
        Locker { m_bitvectorLock },
        [&](auto vectorRef) {
            vectorRef[block->index()] = false;
        });

    if (willDelete == WillDeleteBlock::No)
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

    ASSERT(m_bits.inUse().isEmpty());
}

void BlockDirectory::prepareForAllocation()
{
    m_localAllocators.forEach(
        [&] (LocalAllocator* allocator) {
            allocator->prepareForAllocation();
        });
    
    m_unsweptCursor = 0;
    m_emptyCursor = 0;
    
    m_bits.eden().clearAll();

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

    Locker locker { m_localAllocatorsLock };
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
    m_bits.markingNotEmpty().clearAll();
    m_bits.markingRetired().clearAll();
}

void BlockDirectory::endMarking()
{
    ConcurrentSweeper* sweeper = subspace()->space().heap().concurrentSweeper();
    ASSERT_UNUSED(sweeper, !sweeper || sweeper->isSuspended());

    m_bits.allocated().clearAll();
    
    // It's surprising and frustrating to comprehend, but the end-of-marking flip does not need to
    // know what kind of collection it is. That knowledge is already encoded in the m_markingXYZ
    // vectors.
    
    // Sweeper is suspended so we don't need the lock here.
    m_bits.empty() = m_bits.live() & ~m_bits.markingNotEmpty();
    m_bits.canAllocateButNotEmpty() = m_bits.live() & m_bits.markingNotEmpty() & ~m_bits.markingRetired();

    if (needsDestruction()) {
        // There are some blocks that we didn't allocate out of in the last cycle, but we swept them. This
        // will forget that we did that and we will end up sweeping them again and attempting to call their
        // destructors again. That's fine because of zapping. The only time when we cannot forget is when
        // we just allocate a block or when we move a block from one size class to another. That doesn't
        // happen here.
        m_bits.destructible() = m_bits.live();
    }
    
    if (false) {
        dataLog("Bits for ", m_cellSize, ", ", m_attributes, " after endMarking:\n");
        dumpBits(WTF::dataFile());
    }
}

void BlockDirectory::snapshotUnsweptForEdenCollection()
{
    ConcurrentSweeper* sweeper = subspace()->space().heap().concurrentSweeper();
    ASSERT_UNUSED(sweeper, !sweeper || sweeper->isSuspended());
    m_bits.unswept() |= m_bits.eden();
}

void BlockDirectory::snapshotUnsweptForFullCollection()
{
    ConcurrentSweeper* sweeper = subspace()->space().heap().concurrentSweeper();
    ASSERT_UNUSED(sweeper, !sweeper || sweeper->isSuspended());
    m_bits.unswept() = m_bits.live();
}

MarkedBlock::Handle* BlockDirectory::findBlockToSweep(const AbstractLocker& locker, unsigned& unsweptCursor)
{
    unsweptCursor = (m_bits.unswept() & ~m_bits.inUse()).findBit(unsweptCursor, true);
    if (unsweptCursor >= m_blocks.size())
        return nullptr;
    setIsInUse(locker, unsweptCursor, true);
    return m_blocks[unsweptCursor];
}

void BlockDirectory::sweep()
{
    // We need to be careful of a weird race where while we are sweeping a block
    // the concurrent sweeper comes along and takes the inUse bit for a block
    // in the same bit vector word as we're currently scanning. If we did't
    // refresh our view into the word we could see stale data and try to scan
    // a block already in use.

    Locker locker(bitvectorLock());
    for (size_t index = 0; index < m_blocks.size(); ++index) {
        index = (m_bits.unswept() & ~m_bits.inUse()).findBit(index, true);
        if (index >= m_blocks.size())
            break;

        MarkedBlock::Handle* block = m_blocks[index];
        ASSERT(!isInUse(locker, index));
        setIsInUse(locker, index, true);
        {
            DropLockForScope scope(locker);
            block->sweep(nullptr);
        }
        setIsInUse(locker, index, false);
    }
}

void BlockDirectory::shrink()
{
    // We need to be careful of a weird race where while we are sweeping a block
    // the concurrent sweeper comes along and takes the inUse bit for a block
    // in the same bit vector word as we're currently scanning. If we did't
    // refresh our view into the word we could see stale data and try to scan
    // a block already in use.

    Locker locker(bitvectorLock());
    for (size_t index = 0; index < m_blocks.size(); ++index) {
        index = (m_bits.empty() & ~m_bits.destructible() & ~m_bits.inUse()).findBit(index, true);
        if (index >= m_blocks.size())
            break;

        ASSERT(!isInUse(locker, index));
        setIsInUse(locker, index, true);
        {
            DropLockForScope scope(locker);
            markedSpace().freeBlock(m_blocks[index]);
        }
        setIsInUse(locker, index, false);
    }
}

void BlockDirectory::assertNoUnswept()
{
    if (!ASSERT_ENABLED)
        return;
    
    if (m_bits.unswept().isEmpty())
        return;
    
    dataLog("Assertion failed: unswept not empty in ", *this, ".\n");
    dumpBits();
    ASSERT_NOT_REACHED();
}

void BlockDirectory::didFinishUsingBlock(MarkedBlock::Handle* handle)
{
    Locker locker(bitvectorLock());
    if (UNLIKELY(!isInUse(locker, handle))) {
        dataLogLn("Finish using on a block that's not in use");
        dumpBits();
        RELEASE_ASSERT_NOT_REACHED();
    }

    setIsInUse(locker, handle, false);
}

RefPtr<SharedTask<MarkedBlock::Handle*()>> BlockDirectory::parallelNotEmptyBlockSource()
{
    class Task final : public SharedTask<MarkedBlock::Handle*()> {
    public:
        Task(BlockDirectory& directory)
            : m_directory(directory)
        {
        }
        
        MarkedBlock::Handle* run() final
        {
            if (m_done)
                return nullptr;
            Locker locker { m_lock };
            m_index = m_directory.m_bits.markingNotEmpty().findBit(m_index, true);
            if (m_index >= m_directory.m_blocks.size()) {
                m_done = true;
                return nullptr;
            }
            return m_directory.m_blocks[m_index++];
        }
        
    private:
        BlockDirectory& m_directory WTF_GUARDED_BY_LOCK(m_lock);
        size_t m_index WTF_GUARDED_BY_LOCK(m_lock) { 0 };
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
        [&](auto vectorRef, const char* name) {
            UNUSED_PARAM(vectorRef);
            unsigned length = strlen(name);
            maxNameLength = std::max(maxNameLength, length);
        });
    
    forEachBitVectorWithName(
        NoLockingNecessary,
        [&](auto vectorRef, const char* name) {
            out.print("    ", name, ": ");
            for (unsigned i = maxNameLength - strlen(name); i--;)
                out.print(" ");
            out.print(vectorRef, "\n");
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

