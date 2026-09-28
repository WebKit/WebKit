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

#pragma once

#include <JavaScriptCore/BlockDirectoryBits.h>
#include <JavaScriptCore/CellAttributes.h>
#include <JavaScriptCore/FreeList.h>
#include <JavaScriptCore/JSExportMacros.h>
#include <JavaScriptCore/LocalAllocator.h>
#include <JavaScriptCore/MarkedBlock.h>
#include <wtf/Atomics.h>
#include <wtf/DataLog.h>
#include <wtf/DebugHeap.h>
#include <wtf/Lock.h>
#include <wtf/ReadWriteLock.h>
#include <wtf/SharedTask.h>
#include <wtf/Vector.h>

namespace JSC {

class GCDeferralContext;
class Heap;
class IsoCellSet;
class MarkedSpace;
class LLIntOffsetsExtractor;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(BlockDirectory);

// This just renames the fields of ReadWriteLock so they're less confusing for the way the lock is
// used in the BlockDirectory. The bits are readable / concurrently accessible while the mutate
// lock is held. When the grow lock is held the buffer can be resized or modified without concurrent
// access.
class WTF_CAPABILITY_LOCK MutateGrowLock : private ReadWriteLock {
public:
    void mutateLock() WTF_ACQUIRES_SHARED_LOCK() { readLock(); }
    void mutateUnlock() WTF_RELEASES_SHARED_LOCK() { readUnlock(); }
    void growLock() WTF_ACQUIRES_LOCK() { writeLock(); }
    void growUnlock() WTF_RELEASES_LOCK() { writeUnlock(); }

    using MutateLockView = WTF::ReadLockView;
    using GrowLockView = WTF::WriteLockView;
    MutateLockView& mutate() WTF_RETURNS_LOCK(*this) { return read(); }
    GrowLockView& grow() WTF_RETURNS_LOCK(*this) { return write(); }
};

class BlockDirectory {
    WTF_MAKE_NONCOPYABLE(BlockDirectory);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(BlockDirectory, BlockDirectory);
    
    friend class LLIntOffsetsExtractor;

public:
    BlockDirectory(size_t cellSize);
    ~BlockDirectory();
    void NODELETE setSubspace(Subspace*);
    void lastChanceToFinalize();
    void prepareForAllocation();
    void stopAllocating();
    void stopAllocatingForGood();
    void resumeAllocating();
    void NODELETE beginMarkingForFullCollection();
    void endMarking();
    void NODELETE snapshotUnsweptForEdenCollection();
    void NODELETE snapshotUnsweptForFullCollection();
    void sweepAll();
    void shrink();
    void assertNoUnswept();
    size_t cellSize() const { return m_cellSize; }
    CellAttributes attributes() const { return m_attributes; }
    DestructionMode destruction() const { return m_attributes.destruction; }
    HeapCell::Kind cellKind() const { return m_attributes.cellKind; }

    inline void forEachBlock(const std::invocable<MarkedBlock::Handle*> auto&);
    inline void forEachNotEmptyBlock(const std::invocable<MarkedBlock::Handle*> auto&);

    // Intended for diagnostics only (rdar://157153895)
    bool isFreeListedCell(const void*);

    RefPtr<SharedTask<MarkedBlock::Handle*()>> parallelNotEmptyBlockSource();
    
    void addBlock(MarkedBlock::Handle*);
    enum class WillDeleteBlock : bool { No, Yes };
    // If WillDeleteBlock::Yes is passed then the block will be left in an invalid state. We do this, however, to avoid potentially paging in / decompressing old blocks to update their handle just before freeing them.
    void removeBlock(MarkedBlock::Handle*, WillDeleteBlock = WillDeleteBlock::No);

#if ASSERT_ENABLED
    JS_EXPORT_PRIVATE void assertIsMutatorOrMutatorIsStopped() const WTF_ASSERTS_ACQUIRED_SHARED_LOCK(m_bitvectorLock);
#else
    ALWAYS_INLINE void assertIsMutatorOrMutatorIsStopped() const WTF_ASSERTS_ACQUIRED_SHARED_LOCK(m_bitvectorLock) { }
#endif
    MutateGrowLock& bitvectorLock() LIFETIME_BOUND WTF_RETURNS_LOCK(m_bitvectorLock) { return m_bitvectorLock; }

    void assertInUse(size_t index) const WTF_REQUIRES_SHARED_LOCK(m_bitvectorLock) WTF_ASSERTS_ACQUIRED_CAPABILITY(m_blockOwnership)
    {
        ASSERT_UNUSED(index, isInUse(index));
    }

#if ASSERT_ENABLED
    JS_EXPORT_PRIVATE void assertWorldIsStopped() const WTF_ASSERTS_ACQUIRED_LOCK(m_bitvectorLock) WTF_ASSERTS_ACQUIRED_CAPABILITY(m_blockOwnership);
#else
    ALWAYS_INLINE void assertWorldIsStopped() const WTF_ASSERTS_ACQUIRED_LOCK(m_bitvectorLock) WTF_ASSERTS_ACQUIRED_CAPABILITY(m_blockOwnership) { }
#endif

    // The only place we allow touching the BlockOwned destructible bit is from
    // HeapCell::notifyNeedsDestruction(). That's safe because sweeping only runs on the mutator but
    // if we ever move sweeping off the mutator we'd have to give up on this optimization.
    void assertMayNotifyNeedsDestruction() const WTF_ASSERTS_ACQUIRED_CAPABILITY(m_blockOwnership) { }

    // The named, lock-annotated accessors. Use these rather than going through m_bits directly.
    //
    // Writing any block's BlockOwned bit also requires m_blockOwnership. Bulk reading does not,
    // since a reader that races with the owner will fail to either claim the block or in
    // post-claim validation independent of what it read.
#define BLOCK_DIRECTORY_BIT_OWNERSHIP_BlockOwned WTF_REQUIRES_LOCK(m_blockOwnership)
#define BLOCK_DIRECTORY_BIT_OWNERSHIP_Unowned
#define BLOCK_DIRECTORY_BIT_ACCESSORS(lowerBitName, capitalBitName, ownership)     \
    bool is ## capitalBitName(size_t index) const WTF_REQUIRES_SHARED_LOCK(m_bitvectorLock) { return m_bits.bit<BlockDirectoryBits::Kind::capitalBitName>(index).concurrentGet(std::memory_order_relaxed); } \
    bool is ## capitalBitName(MarkedBlock::Handle* block) const WTF_REQUIRES_SHARED_LOCK(m_bitvectorLock) { return is ## capitalBitName(block->index()); } \
    BlockDirectoryBits::BlockDirectoryBitVectorView<BlockDirectoryBits::Kind::capitalBitName> lowerBitName ## BitsView() const WTF_REQUIRES_SHARED_LOCK(m_bitvectorLock) { return m_bits.lowerBitName(); } \
    \
    void setIs ## capitalBitName(size_t index, bool value) WTF_REQUIRES_SHARED_LOCK(m_bitvectorLock) BLOCK_DIRECTORY_BIT_OWNERSHIP_ ## ownership { m_bits.bit<BlockDirectoryBits::Kind::capitalBitName>(index).concurrentSet(value, std::memory_order_relaxed); } \
    void setIs ## capitalBitName(MarkedBlock::Handle* block, bool value) WTF_REQUIRES_SHARED_LOCK(m_bitvectorLock) BLOCK_DIRECTORY_BIT_OWNERSHIP_ ## ownership { setIs ## capitalBitName(block->index(), value); } \
    bool testAndClearIs ## capitalBitName(size_t index) WTF_REQUIRES_SHARED_LOCK(m_bitvectorLock) BLOCK_DIRECTORY_BIT_OWNERSHIP_ ## ownership { return m_bits.bit<BlockDirectoryBits::Kind::capitalBitName>(index).concurrentTestAndClear(std::memory_order_relaxed); } \
    BlockDirectoryBits::BlockDirectoryBitVectorRef<BlockDirectoryBits::Kind::capitalBitName> lowerBitName ## Bits() WTF_REQUIRES_LOCK(m_bitvectorLock) { return m_bits.lowerBitName(); }

    FOR_EACH_BLOCK_DIRECTORY_BIT(BLOCK_DIRECTORY_BIT_ACCESSORS)
#undef BLOCK_DIRECTORY_BIT_ACCESSORS
#undef BLOCK_DIRECTORY_BIT_OWNERSHIP_Unowned
#undef BLOCK_DIRECTORY_BIT_OWNERSHIP_BlockOwned

    // The inUse bit acts as a per-block lock, holding it allows access to the BlockOwned bits for that block.
    // NOTE: The most common idiom of searching the bits for some combination of bits then taking the inUse
    // for that block has to handle time-of-check, time-of-use problem by re-validating the bits in question.
    // after this function returns true.
    [[nodiscard]] bool claimInUse(size_t index) WTF_REQUIRES_SHARED_LOCK(m_bitvectorLock)
    {
        return !m_bits.bit<BlockDirectoryBits::Kind::InUse>(index).concurrentTestAndSet(std::memory_order_acquire);
    }

    void releaseInUse(size_t index) WTF_REQUIRES_SHARED_LOCK(m_bitvectorLock)
    {
        m_bits.bit<BlockDirectoryBits::Kind::InUse>(index).concurrentSet(false, std::memory_order_release);
    }

    // A destructible block still owes its old owner a destructor pass over every one of its cells,
    // and whoever took it would have to pay that inline, so it stays with the sweeper until the bit
    // says the destructors have run.
    auto stealableBits() const WTF_REQUIRES_SHARED_LOCK(m_bitvectorLock) { return emptyBitsView() & ~destructibleBitsView(); }
    bool isStealable(size_t index) const WTF_REQUIRES_SHARED_LOCK(m_bitvectorLock) { return stealableBits()[index]; }

    template<typename Func>
    void forEachBitVector(const Func& func) WTF_REQUIRES_LOCK(m_bitvectorLock)
    {
#define BLOCK_DIRECTORY_BIT_CALLBACK(lowerBitName, capitalBitName, ownership) \
        func(lowerBitName ## Bits());
        FOR_EACH_BLOCK_DIRECTORY_BIT(BLOCK_DIRECTORY_BIT_CALLBACK);
#undef BLOCK_DIRECTORY_BIT_CALLBACK
    }

    void clearBitsForRemovedBlock(size_t index) WTF_REQUIRES_LOCK(m_bitvectorLock) WTF_REQUIRES_LOCK(m_blockOwnership)
    {
        // We don't have to clear inUse last here because we're holding the exclusive growth lock.
#define BLOCK_DIRECTORY_BIT_CLEAR(lowerBitName, capitalBitName, ownership) \
        setIs##capitalBitName(index, false);
        FOR_EACH_BLOCK_DIRECTORY_BIT(BLOCK_DIRECTORY_BIT_CLEAR)
#undef BLOCK_DIRECTORY_BIT_CLEAR
    }

    template<typename Func>
    void forEachBitVectorWithName(const Func& func) const WTF_REQUIRES_SHARED_LOCK(m_bitvectorLock)
    {
#define BLOCK_DIRECTORY_BIT_CALLBACK(lowerBitName, capitalBitName, ownership) \
        func(lowerBitName ## BitsView(), #capitalBitName);
        FOR_EACH_BLOCK_DIRECTORY_BIT(BLOCK_DIRECTORY_BIT_CALLBACK);
#undef BLOCK_DIRECTORY_BIT_CALLBACK
    }
    
    BlockDirectory* nextDirectory() const { return m_nextDirectory; }
    BlockDirectory* nextDirectoryInSubspace() const { return m_nextDirectoryInSubspace; }

    void setNextDirectory(BlockDirectory* directory) { m_nextDirectory = directory; }
    void setNextDirectoryInSubspace(BlockDirectory* directory) { m_nextDirectoryInSubspace = directory; }

    MarkedBlock::Handle* findEmptyBlockToSteal();

    // Call while holding the block's inUse bit; this releases it.
    void noteBlockMayBeStealable(unsigned index) WTF_REQUIRES_SHARED_LOCK(m_bitvectorLock);

    inline MarkedBlock::Handle* findBlockToSweep();
    MarkedBlock::Handle* findBlockToSweep(unsigned& unsweptCursor);

    // FIXME: rdar://139998916
    MarkedBlock::Handle* NODELETE findMarkedBlockHandleDebug(MarkedBlock*);

    void didFinishUsingBlock(MarkedBlock::Handle*);
    void didFinishUsingBlock(AbstractLocker&, MarkedBlock::Handle*) WTF_REQUIRES_SHARED_LOCK(m_bitvectorLock);

    Subspace* subspace() const { return m_subspace; }
    MarkedSpace& NODELETE markedSpace() const;
    
    void dump(PrintStream&) const;
    void dumpBits(PrintStream& = WTF::dataFile()) WTF_REQUIRES_SHARED_LOCK(m_bitvectorLock);

private:
    friend class AlignedMemoryAllocator;
    friend class IsoCellSet;
    friend class LocalAllocator;
    friend class LocalSideAllocator;
    friend class MarkedBlock;
    
    MarkedBlock::Handle* findBlockForAllocation(LocalAllocator&);
    
    MarkedBlock::Handle* tryAllocateBlock(Heap&);
    
    // The MarkedBlock is stored next to its Handle so that we can prefetch its header without chasing through
    // the Handle first. MarkedBlocks are often cold when first accessed so this can accelerate sweeping.
    Vector<std::pair<MarkedBlock::Handle*, MarkedBlock*>> m_blocks;
    Vector<unsigned> m_freeBlockIndices;

    BlockDirectoryBits m_bits WTF_GUARDED_BY_LOCK(m_bitvectorLock); // Don't access this directly use one of the accessors above.
    MutateGrowLock m_bitvectorLock;
    [[no_unique_address]] BlockOwnership m_blockOwnership;
    Lock m_localAllocatorsLock;
    CellAttributes m_attributes;

    unsigned m_cellSize;
    
    // After you do something to a block based on one of these cursors, you clear the bit in the
    // corresponding bitvector and leave the cursor where it was. We can use unsigned instead of size_t since
    // this number is bound by capacity of Vector m_blocks, which must be within unsigned.
    Atomic<unsigned> m_emptyCursor { 0 };
    Atomic<unsigned> m_unsweptCursor { 0 }; // Points to the next block that is a candidate for incremental sweeping.

    // FIXME: All of these should probably be references.
    // https://bugs.webkit.org/show_bug.cgi?id=166988
    Subspace* m_subspace { nullptr };
    BlockDirectory* m_nextDirectory { nullptr };
    BlockDirectory* m_nextDirectoryInSubspace { nullptr };
    BlockDirectory* m_nextDirectoryWithEmptyBlocks { nullptr };
    Atomic<bool> m_isOnEmptyBlocksList { false };
    
    SentinelLinkedList<LocalAllocator, BasicRawSentinelNode<LocalAllocator>> m_localAllocators;
};

} // namespace JSC
