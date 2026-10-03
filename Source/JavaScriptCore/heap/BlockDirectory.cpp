/*
 * Copyright (C) 2012-2024, 2026 Apple Inc. All rights reserved.
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

#include "AlignedMemoryAllocator.h"
#include "BlockDirectoryInlines.h"
#include "Heap.h"
#include "HeapInlines.h"
#include "MarkedSpaceInlines.h"
#include "SubspaceInlines.h"
#include "SuperSampler.h"

#include <wtf/FunctionTraits.h>
#include <wtf/Lock.h>

namespace JSC {

namespace BlockDirectoryInternal {
static constexpr bool verbose = false;
}

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

void BlockDirectory::noteBlockMayBeStealable(unsigned index)
{
    assertInUse(index);

    // Read while the block is still ours; once it is released these bits describe whoever claims it next.
    bool stealable = isStealable(index);
    // Must stay the last bit written for this block; see claimInUse().
    releaseInUse(index);
    if (!stealable)
        return;
    // The cursor only moves forward, so a block that falls empty behind it would stay invisible for
    // the rest of the collection cycle and the heap would grow instead of reusing it.
    m_emptyCursor.storeRelaxed(std::min(m_emptyCursor.loadRelaxed(), index));
    subspace()->alignedMemoryAllocator()->addDirectoryWithEmptyBlocks(this);
}

MarkedBlock::Handle* BlockDirectory::findEmptyBlockToSteal()
{
    Locker locker { m_bitvectorLock.mutate() };
    unsigned cursor = m_emptyCursor.loadRelaxed();
    for (;; ++cursor) {
        cursor = (stealableBits() & ~inUseBitsView()).findBit(cursor, true);
        if (cursor >= m_blocks.size()) {
            m_emptyCursor.storeRelaxed(cursor);
            return nullptr;
        }
        if (!claimInUse(cursor))
            continue;
        assertInUse(cursor);

        // Recheck under the claim; see claimInUse().
        if (isStealable(cursor)) [[likely]] {
            dataLogLnIf(BlockDirectoryInternal::verbose, "Setting block ", cursor, " in use (findEmptyBlockToSteal) for ", *this);
            m_emptyCursor.storeRelaxed(cursor);
            return m_blocks[cursor].first;
        }
        releaseInUse(cursor);
    }
}

MarkedBlock::Handle* BlockDirectory::findBlockForAllocation(LocalAllocator& allocator)
{
    Locker locker { m_bitvectorLock.mutate() };
    for (;;) {
        allocator.m_allocationCursor = (canAllocateBitsView() & ~inUseBitsView()).findBit(allocator.m_allocationCursor, true);
        if (allocator.m_allocationCursor >= m_blocks.size())
            return nullptr;
        
        unsigned blockIndex = allocator.m_allocationCursor++;
        if (!claimInUse(blockIndex))
            continue;
        // Recheck under the claim; see claimInUse().
        if (!isCanAllocate(blockIndex)) [[unlikely]] {
            releaseInUse(blockIndex);
            continue;
        }
        assertInUse(blockIndex);
        // This block is about to be swept to build a free list, which reads the header. Start the
        // fetch here so it overlaps the bitvector updates and the lock release below.
        __builtin_prefetch(m_blocks[blockIndex].second);
        setIsCanAllocate(blockIndex, false);
        dataLogLnIf(BlockDirectoryInternal::verbose, "Setting block ", blockIndex, " in use (findBlockForAllocation) for ", *this);
        return m_blocks[blockIndex].first;
    }
}

MarkedBlock::Handle* BlockDirectory::tryAllocateBlock(JSC::Heap& heap)
{
    MarkedBlock::Handle* handle = MarkedBlock::tryCreate(heap, subspace()->alignedMemoryAllocator());
    if (!handle)
        return nullptr;
    
    markedSpace().didAddBlock(handle);
    
    return handle;
}

void BlockDirectory::addBlock(MarkedBlock::Handle* block)
{
    ASSERT(markedSpace().heap().vm().currentThreadIsHoldingAPILock());

    Locker locker { m_bitvectorLock.grow() };
    unsigned index;
    if (m_freeBlockIndices.isEmpty()) {
        index = m_blocks.size();

        size_t oldCapacity = m_blocks.capacity();
        m_blocks.append({ block, &block->block() });
        if (m_blocks.capacity() != oldCapacity) {
            ASSERT(m_bits.numBits() == oldCapacity);
            ASSERT(m_blocks.capacity() > oldCapacity);
            
            subspace()->didResizeBits(m_blocks.capacity());
            m_bits.resize(m_blocks.capacity());
        }
    } else {
        index = m_freeBlockIndices.takeLast();
        ASSERT(!m_blocks[index].first);
        m_blocks[index] = { block, &block->block() };
    }
    
    forEachBitVector(
        [&](auto vectorRef) {
            ASSERT_UNUSED(vectorRef, !vectorRef[index]);
        });

    // This is the point at which the block learns of its cellSize() and attributes().
    block->didAddToDirectory(this, index);
    
    dataLogLnIf(BlockDirectoryInternal::verbose, "Setting block ", index, " in use (addBlock) for ", *this);
    // Plain rather than claimInUse(): growth is exclusive, so no other thread can be holding the bits
    // for mutate, and a block being added has no previous holder whose writes we would need to see.
    setIsInUse(index, true);
    assertInUse(index);

    setIsLive(index, true);
    setIsEmpty(index, true);
}

void BlockDirectory::removeBlock(MarkedBlock::Handle* block, WillDeleteBlock willDelete)
{
    Locker locker { m_bitvectorLock.grow() };
    ASSERT(block->directory() == this);
    ASSERT(m_blocks[block->index()].first == block);
    assertInUse(block->index());

    subspace()->didRemoveBlock(block->index());

    m_blocks[block->index()] = { nullptr, nullptr };
    m_freeBlockIndices.append(block->index());

    clearBitsForRemovedBlock(block->index());

    if (willDelete == WillDeleteBlock::No)
        block->didRemoveFromDirectory();
}

bool BlockDirectory::isFreeListedCell(const void* cell)
{
    Locker locker { m_localAllocatorsLock };
    bool result = false;
    m_localAllocators.forEach(
        [&] (LocalAllocator* allocator) {
            if (allocator->isFreeListedCell(cell))
                result = true;
        });
    return result;
}

void BlockDirectory::stopAllocating()
{
    dataLogLnIf(BlockDirectoryInternal::verbose, RawPointer(this), ": BlockDirectory::stopAllocating!");
    m_localAllocators.forEach(
        [&] (LocalAllocator* allocator) {
            allocator->stopAllocating();
        });

#if ASSERT_ENABLED
    assertIsMutatorOrMutatorIsStopped();
    if (!inUseBitsView().isEmpty()) [[unlikely]] {
        dataLogLn("Not all inUse bits are clear at stopAllocating");
        dataLogLn(*this);
        dumpBits();
        RELEASE_ASSERT_NOT_REACHED();
    }
#endif
}

void BlockDirectory::prepareForAllocation()
{
    m_localAllocators.forEach(
        [&] (LocalAllocator* allocator) {
            allocator->prepareForAllocation();
        });

    m_unsweptCursor.storeRelaxed(0);
    m_emptyCursor.storeRelaxed(0);

    assertWorldIsStopped();
    // endMarking recomputes the empty bits wholesale rather than block by block, so none of the blocks
    // that fell empty there announced themselves the way didFinishUsingBlock does. Re-derive
    // membership from the bits here, once m_emptyCursor above has been rewound to match them.
    if (!stealableBits().isEmpty())
        subspace()->alignedMemoryAllocator()->addDirectoryWithEmptyBlocks(this);
    edenBits().clearAll();

    if (Options::useImmortalObjects()) [[unlikely]] {
        // FIXME: Make this work again.
        // https://bugs.webkit.org/show_bug.cgi?id=162296
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void BlockDirectory::stopAllocatingForGood()
{
    dataLogLnIf(BlockDirectoryInternal::verbose, RawPointer(this), ": BlockDirectory::stopAllocatingForGood!");
    
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
    dataLogLnIf(BlockDirectoryInternal::verbose, RawPointer(this), ": BlockDirectory::resumeAllocating!");
    m_localAllocators.forEach(
        [&] (LocalAllocator* allocator) {
            allocator->resumeAllocating();
        });
}

void BlockDirectory::beginMarkingForFullCollection()
{
    assertWorldIsStopped();

    // Mark bits are sticky and so is our summary of mark bits. We only clear these during full
    // collections, so if you survived the last collection you will survive the next one so long
    // as the next one is eden.
    markingNotEmptyBits().clearAll();
    markingRetiredBits().clearAll();
}

void BlockDirectory::endMarking()
{
    assertWorldIsStopped();

    allocatedBits().clearAll();
    
#if ASSERT_ENABLED
    if (!inUseBitsView().isEmpty()) [[unlikely]] {
        dataLogLn("Block is inUse at end marking.");
        dataLogLn(*this);
        dumpBits();
        RELEASE_ASSERT_NOT_REACHED();
    }
#endif

    // It's surprising and frustrating to comprehend, but the end-of-marking flip does not need to
    // know what kind of collection it is. That knowledge is already encoded in the m_markingXYZ
    // vectors.
    
    emptyBits() = liveBits() & ~markingNotEmptyBits();
    canAllocateBits() = liveBits() & ~markingRetiredBits();

    switch (m_attributes.destruction) {
    case NeedsDestruction: {
        // There are some blocks that we didn't allocate out of in the last cycle, but we swept them. This
        // will forget that we did that and we will end up sweeping them again and attempting to call their
        // destructors again. That's fine because of zapping. The only time when we cannot forget is when
        // we just allocate a block or when we move a block from one size class to another. That doesn't
        // happen here.
        destructibleBits() = liveBits();
        break;
    }

    case MayNeedDestruction: {
        // When this destruction mode is specified, each cell notifies whether this MarkedBlock needs destructor runs conservatively.
        // The bit will be set from the mutator and we use this bit to decide whether we run a destructor.
        // Until we clear the MarkedBlock completely, once this bit is set, this bit is stickily set to the MarkedBlock.
        break;
    }

    case DoesNotNeedDestruction:
        break;
    }

    if (BlockDirectoryInternal::verbose) {
        dataLogLn("Bits for ", m_cellSize, ", ", m_attributes, " after endMarking:");
        dumpBits(WTF::dataFile());
    }
}

void BlockDirectory::snapshotUnsweptForEdenCollection()
{
    assertWorldIsStopped();
    unsweptBits() |= edenBits();
}

void BlockDirectory::snapshotUnsweptForFullCollection()
{
    assertWorldIsStopped();
    unsweptBits() = liveBits();
}

MarkedBlock::Handle* BlockDirectory::findBlockToSweep(unsigned& unsweptCursor)
{
    Locker locker { m_bitvectorLock.mutate() };
    for (;; ++unsweptCursor) {
        unsweptCursor = (unsweptBitsView() & ~inUseBitsView()).findBit(unsweptCursor, true);
        if (unsweptCursor >= m_blocks.size())
            return nullptr;
        if (!claimInUse(unsweptCursor))
            continue;

        // Recheck under the claim; see claimInUse().
        if (isUnswept(unsweptCursor)) [[likely]] {
            dataLogLnIf(BlockDirectoryInternal::verbose, "Setting block ", unsweptCursor, " in use (findBlockToSweep) for ", *this);
            return m_blocks[unsweptCursor].first;
        }
        releaseInUse(unsweptCursor);
    }
}

void BlockDirectory::sweepAll()
{

    unsigned cursor = 0;
    while (MarkedBlock::Handle* block = findBlockToSweep(cursor)) {
        // findBlockToSweep() took this block's inUse bit, which is what keeps it ours across the sweep.
        block->sweep(nullptr);
        didFinishUsingBlock(block);
    }
}

void BlockDirectory::shrink()
{
    Locker locker { m_bitvectorLock.mutate() };
    for (size_t index = 0; index < m_blocks.size(); ++index) {
        index = (stealableBits() & ~inUseBitsView()).findBit(index, true);
        if (index >= m_blocks.size())
            break;

        if (!claimInUse(index))
            continue;
        assertInUse(index);
        // Recheck under the claim; see claimInUse().
        if (!isStealable(index)) [[unlikely]] {
            releaseInUse(index);
            continue;
        }
        dataLogLnIf(BlockDirectoryInternal::verbose, "Setting block ", index, " in use (shrink) for ", *this);
        MarkedBlock::Handle* block = m_blocks[index].first;

        DropLockForScope drop(locker);
        // removeBlock() clears every bit for the index on its way out, so there is no inUse to clear after.
        markedSpace().freeBlock(block);
    }
}

// FIXME: rdar://139998916
MarkedBlock::Handle* BlockDirectory::findMarkedBlockHandleDebug(MarkedBlock* block)
{
    for (size_t index = 0; index < m_blocks.size(); ++index) {
        MarkedBlock::Handle* handle = m_blocks[index].first;
        if (handle && &handle->block() == block)
            return handle;
    }
    return nullptr;
}

void BlockDirectory::assertNoUnswept()
{
    if (!ASSERT_ENABLED)
        return;

    Locker locker { m_bitvectorLock.mutate() };

    if (unsweptBitsView().isEmpty())
        return;
    
    dataLog("Assertion failed: unswept not empty in ", *this, ".\n");
    dumpBits();
    ASSERT_NOT_REACHED();
}

void BlockDirectory::didFinishUsingBlock(MarkedBlock::Handle* handle)
{
    Locker locker { m_bitvectorLock.mutate() };
    didFinishUsingBlock(locker, handle);
}

void BlockDirectory::didFinishUsingBlock(AbstractLocker&, MarkedBlock::Handle* handle)
{
    if (!isInUse(handle->index())) [[unlikely]] {
        dataLogLn("Finish using on a block that's not in use: ", handle->index());
        dumpBits();
        RELEASE_ASSERT_NOT_REACHED();
    }

    dataLogLnIf(BlockDirectoryInternal::verbose, "Setting block ", handle->index(), " not in use (didFinishUsingBlock) for ", *this);
    noteBlockMayBeStealable(handle->index());
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
            m_directory.assertIsMutatorOrMutatorIsStopped();
            m_index = m_directory.m_bits.markingNotEmpty().findBit(m_index, true);
            if (m_index >= m_directory.m_blocks.size()) {
                m_done = true;
                return nullptr;
            }
            return m_directory.m_blocks[m_index++].first;
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
        [&](auto vectorRef, const char* name) {
            UNUSED_PARAM(vectorRef);
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
            unsigned length = strlen(name);
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
            maxNameLength = std::max(maxNameLength, length);
        });
    
    forEachBitVectorWithName(
        [&](auto vectorRef, const char* name) {
            out.print("    ", name, ": ");
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
            for (unsigned i = maxNameLength - strlen(name); i--;)
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
                out.print(" ");
            out.print(vectorRef, "\n");
        });
}

MarkedSpace& BlockDirectory::markedSpace() const
{
    return m_subspace->space();
}

#if ASSERT_ENABLED
void BlockDirectory::assertIsMutatorOrMutatorIsStopped() const
{
    auto& heap = markedSpace().heap();
    if (!heap.worldIsStopped()) {
        if (auto owner = heap.vm().apiLock().ownerThread())
            ASSERT(owner->get() == &Thread::currentSingleton());
        else {
            // FIXME: It feels like heap access should be tied to holding the API lock.
            ASSERT(heap.hasAccess());
        }
    }
}

void BlockDirectory::assertWorldIsStopped() const WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    ASSERT(markedSpace().heap().worldIsStopped());
    ASSERT(inUseBitsView().isEmpty());
}
#endif
} // namespace JSC

