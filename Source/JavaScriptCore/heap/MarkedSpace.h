/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2011, 2016 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#pragma once

#include "IterationStatus.h"
#include "LargeAllocation.h"
#include "MarkedAllocator.h"
#include "MarkedBlock.h"
#include "MarkedBlockSet.h"
#include <array>
#include <wtf/Bag.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/RetainPtr.h>
#include <wtf/SentinelLinkedList.h>
#include <wtf/Vector.h>

namespace JSC {

class Heap;
class HeapIterationScope;
class LLIntOffsetsExtractor;
class WeakSet;

typedef uint32_t HeapVersion;

class MarkedSpace {
    WTF_MAKE_NONCOPYABLE(MarkedSpace);
public:
    // sizeStep is really a synonym for atomSize; it's no accident that they are the same.
    static const size_t sizeStep = MarkedBlock::atomSize;
    
    // Sizes up to this amount get a size class for each size step.
    static const size_t preciseCutoff = 80;
    
    // The amount of available payload in a block is the block's size minus the header. But the
    // header size might not be atom size aligned, so we round down the result accordingly.
    static const size_t blockPayload = (MarkedBlock::blockSize - sizeof(MarkedBlock)) & ~(MarkedBlock::atomSize - 1);
    
    // The largest cell we're willing to allocate in a MarkedBlock the "normal way" (i.e. using size
    // classes, rather than a large allocation) is half the size of the payload, rounded down. This
    // ensures that we only use the size class approach if it means being able to pack two things
    // into one block.
    static const size_t largeCutoff = (blockPayload / 2) & ~(sizeStep - 1);

    static const size_t numSizeClasses = largeCutoff / sizeStep;
    
    static const HeapVersion nullVersion = 0; // The version of freshly allocated blocks.
    static const HeapVersion initialVersion = 2; // The version that the heap starts out with. Set to make sure that nextVersion(nullVersion) != initialVersion.
    
    static HeapVersion nextVersion(HeapVersion version)
    {
        version++;
        if (version == nullVersion)
            version = initialVersion;
        return version;
    }
    
    static size_t sizeClassToIndex(size_t size)
    {
        ASSERT(size);
        return (size + sizeStep - 1) / sizeStep - 1;
    }
    
    static size_t indexToSizeClass(size_t index)
    {
        return (index + 1) * sizeStep;
    }
    
    // Each Subspace corresponds to all of the blocks for all of the sizes for some "class" of
    // objects. There are three classes: non-destructor JSCells, destructor JSCells, and auxiliary.
    // MarkedSpace is set up to make it relatively easy to add new Subspaces.
    struct Subspace {
        std::array<MarkedAllocator*, numSizeClasses> allocatorForSizeStep;
        
        // Each MarkedAllocator is a size class.
        Bag<MarkedAllocator> bagOfAllocators;
        
        AllocatorAttributes attributes;
    };
    
    MarkedSpace(Heap*);
    ~MarkedSpace();
    void lastChanceToFinalize();

    static size_t optimalSizeFor(size_t);
    
    static MarkedAllocator* allocatorFor(Subspace&, size_t);

    MarkedAllocator* allocatorFor(size_t);
    MarkedAllocator* destructorAllocatorFor(size_t);
    MarkedAllocator* auxiliaryAllocatorFor(size_t);

    JS_EXPORT_PRIVATE void* allocate(Subspace&, size_t);
    JS_EXPORT_PRIVATE void* allocate(Subspace&, GCDeferralContext*, size_t);
    JS_EXPORT_PRIVATE void* tryAllocate(Subspace&, size_t);
    JS_EXPORT_PRIVATE void* tryAllocate(Subspace&, GCDeferralContext*, size_t);
    
    void* allocateWithDestructor(size_t);
    void* allocateWithoutDestructor(size_t);
    void* allocateWithDestructor(GCDeferralContext*, size_t);
    void* allocateWithoutDestructor(GCDeferralContext*, size_t);
    void* allocateAuxiliary(size_t);
    void* tryAllocateAuxiliary(size_t);
    void* tryAllocateAuxiliary(GCDeferralContext*, size_t);
    
    Subspace& subspaceForObjectsWithDestructor() { return m_destructorSpace; }
    Subspace& subspaceForObjectsWithoutDestructor() { return m_normalSpace; }
    Subspace& subspaceForAuxiliaryData() { return m_auxiliarySpace; }
    
    void prepareForAllocation();

    void visitWeakSets(HeapRootVisitor&);
    void reapWeakSets();

    MarkedBlockSet& blocks() { return m_blocks; }

    void willStartIterating();
    bool isIterating() const { return m_isIterating; }
    void didFinishIterating();

    void stopAllocating();
    void resumeAllocating(); // If we just stopped allocation but we didn't do a collection, we need to resume allocation.
    
    void prepareForMarking();
    
    void prepareForConservativeScan();

    typedef HashSet<MarkedBlock*>::iterator BlockIterator;

    template<typename Functor> void forEachLiveCell(HeapIterationScope&, const Functor&);
    template<typename Functor> void forEachDeadCell(HeapIterationScope&, const Functor&);
    template<typename Functor> void forEachBlock(const Functor&);

    void shrink();
    void freeBlock(MarkedBlock::Handle*);
    void freeOrShrinkBlock(MarkedBlock::Handle*);

    void didAddBlock(MarkedBlock::Handle*);
    void didConsumeFreeList(MarkedBlock::Handle*);
    void didAllocateInBlock(MarkedBlock::Handle*);

    void beginMarking();
    void endMarking();
    void snapshotUnswept();
    void clearNewlyAllocated();
    void sweep();
    void sweepLargeAllocations();
    void assertNoUnswept();
    size_t objectCount();
    size_t size();
    size_t capacity();

    bool isPagedOut(double deadline);
    
    HeapVersion markingVersion() const { return m_markingVersion; }
    HeapVersion newlyAllocatedVersion() const { return m_newlyAllocatedVersion; }

    const Vector<LargeAllocation*>& largeAllocations() const { return m_largeAllocations; }
    unsigned largeAllocationsNurseryOffset() const { return m_largeAllocationsNurseryOffset; }
    unsigned largeAllocationsOffsetForThisCollection() const { return m_largeAllocationsOffsetForThisCollection; }
    
    // These are cached pointers and offsets for quickly searching the large allocations that are
    // relevant to this collection.
    LargeAllocation** largeAllocationsForThisCollectionBegin() const { return m_largeAllocationsForThisCollectionBegin; }
    LargeAllocation** largeAllocationsForThisCollectionEnd() const { return m_largeAllocationsForThisCollectionEnd; }
    unsigned largeAllocationsForThisCollectionSize() const { return m_largeAllocationsForThisCollectionSize; }
    
    MarkedAllocator* firstAllocator() const { return m_firstAllocator; }
    MarkedAllocator* allocatorForEmptyAllocation() const { return m_allocatorForEmptyAllocation; }
    
    MarkedBlock::Handle* findEmptyBlockToSteal();
    
    // When this is true it means that we have flipped but the mark bits haven't converged yet.
    bool isMarking() const { return m_isMarking; }
    
    void dumpBits(PrintStream& = WTF::dataFile());
    
private:
    friend class LLIntOffsetsExtractor;
    friend class JIT;
    friend class WeakSet;
    
    JS_EXPORT_PRIVATE static std::array<size_t, numSizeClasses> s_sizeClassForSizeStep;
    
    void* allocateLarge(Subspace&, GCDeferralContext*, size_t);
    void* tryAllocateLarge(Subspace&, GCDeferralContext*, size_t);

    static void initializeSizeClassForStepSize();
    
    void initializeSubspace(Subspace&);

    template<typename Functor> inline void forEachAllocator(const Functor&);
    template<typename Functor> inline void forEachSubspace(const Functor&);
    
    void addActiveWeakSet(WeakSet*);

    Subspace m_destructorSpace;
    Subspace m_normalSpace;
    Subspace m_auxiliarySpace;

    Heap* m_heap;
    HeapVersion m_markingVersion { initialVersion };
    HeapVersion m_newlyAllocatedVersion { initialVersion };
    size_t m_capacity;
    bool m_isIterating;
    bool m_isMarking { false };
    MarkedBlockSet m_blocks;
    
    Vector<LargeAllocation*> m_largeAllocations;
    unsigned m_largeAllocationsNurseryOffset { 0 };
    unsigned m_largeAllocationsOffsetForThisCollection { 0 };
    unsigned m_largeAllocationsNurseryOffsetForSweep { 0 };
    LargeAllocation** m_largeAllocationsForThisCollectionBegin { nullptr };
    LargeAllocation** m_largeAllocationsForThisCollectionEnd { nullptr };
    unsigned m_largeAllocationsForThisCollectionSize { 0 };
    
    SentinelLinkedList<WeakSet, BasicRawSentinelNode<WeakSet>> m_activeWeakSets;
    SentinelLinkedList<WeakSet, BasicRawSentinelNode<WeakSet>> m_newActiveWeakSets;
    
    MarkedAllocator* m_firstAllocator { nullptr };
    MarkedAllocator* m_allocatorForEmptyAllocation { nullptr };
};

inline MarkedAllocator* MarkedSpace::allocatorFor(Subspace& space, size_t bytes)
{
    ASSERT(bytes);
    if (bytes <= largeCutoff)
        return space.allocatorForSizeStep[sizeClassToIndex(bytes)];
    return nullptr;
}

inline MarkedAllocator* MarkedSpace::allocatorFor(size_t bytes)
{
    return allocatorFor(m_normalSpace, bytes);
}

inline MarkedAllocator* MarkedSpace::destructorAllocatorFor(size_t bytes)
{
    return allocatorFor(m_destructorSpace, bytes);
}

inline MarkedAllocator* MarkedSpace::auxiliaryAllocatorFor(size_t bytes)
{
    return allocatorFor(m_auxiliarySpace, bytes);
}

inline void* MarkedSpace::allocateWithoutDestructor(size_t bytes)
{
    return allocate(m_normalSpace, bytes);
}

inline void* MarkedSpace::allocateWithDestructor(size_t bytes)
{
    return allocate(m_destructorSpace, bytes);
}

inline void* MarkedSpace::allocateWithoutDestructor(GCDeferralContext* deferralContext, size_t bytes)
{
    return allocate(m_normalSpace, deferralContext, bytes);
}

inline void* MarkedSpace::allocateWithDestructor(GCDeferralContext* deferralContext, size_t bytes)
{
    return allocate(m_destructorSpace, deferralContext, bytes);
}

inline void* MarkedSpace::allocateAuxiliary(size_t bytes)
{
    return allocate(m_auxiliarySpace, bytes);
}

inline void* MarkedSpace::tryAllocateAuxiliary(size_t bytes)
{
    return tryAllocate(m_auxiliarySpace, bytes);
}

inline void* MarkedSpace::tryAllocateAuxiliary(GCDeferralContext* deferralContext, size_t bytes)
{
    return tryAllocate(m_auxiliarySpace, deferralContext, bytes);
}

template <typename Functor> inline void MarkedSpace::forEachBlock(const Functor& functor)
{
    forEachAllocator(
        [&] (MarkedAllocator& allocator) -> IterationStatus {
            allocator.forEachBlock(functor);
            return IterationStatus::Continue;
        });
}

template <typename Functor>
void MarkedSpace::forEachAllocator(const Functor& functor)
{
    for (MarkedAllocator* allocator = m_firstAllocator; allocator; allocator = allocator->nextAllocator()) {
        if (functor(*allocator) == IterationStatus::Done)
            return;
    }
}

template<typename Functor>
inline void MarkedSpace::forEachSubspace(const Functor& func)
{
    AllocatorAttributes attributes;
    
    attributes.destruction = NeedsDestruction;
    attributes.cellKind = HeapCell::JSCell;
    if (func(m_destructorSpace, attributes) == IterationStatus::Done)
        return;
    
    attributes.destruction = DoesNotNeedDestruction;
    attributes.cellKind = HeapCell::JSCell;
    if (func(m_normalSpace, attributes) == IterationStatus::Done)
        return;

    attributes.destruction = DoesNotNeedDestruction;
    attributes.cellKind = HeapCell::Auxiliary;
    func(m_auxiliarySpace, attributes);
}

ALWAYS_INLINE size_t MarkedSpace::optimalSizeFor(size_t bytes)
{
    ASSERT(bytes);
    if (bytes <= preciseCutoff)
        return WTF::roundUpToMultipleOf<sizeStep>(bytes);
    if (bytes <= largeCutoff)
        return s_sizeClassForSizeStep[sizeClassToIndex(bytes)];
    return bytes;
}

} // namespace JSC
