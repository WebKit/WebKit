/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2009, 2011, 2016 Apple Inc. All rights reserved.
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

#ifndef MarkedBlock_h
#define MarkedBlock_h

#include "AllocatorAttributes.h"
#include "DestructionMode.h"
#include "FreeList.h"
#include "HeapCell.h"
#include "HeapOperation.h"
#include "IterationStatus.h"
#include "WeakSet.h"
#include <wtf/Bitmap.h>
#include <wtf/DataLog.h>
#include <wtf/DoublyLinkedList.h>
#include <wtf/HashFunctions.h>
#include <wtf/StdLibExtras.h>

namespace JSC {
    
class Heap;
class JSCell;
class MarkedAllocator;

typedef uintptr_t Bits;

// Set to log state transitions of blocks.
#define HEAP_LOG_BLOCK_STATE_TRANSITIONS 0

#if HEAP_LOG_BLOCK_STATE_TRANSITIONS
#define HEAP_LOG_BLOCK_STATE_TRANSITION(handle) do {            \
        dataLogF(                                               \
            "%s:%d %s: block %s = %p, %d\n",                    \
            __FILE__, __LINE__, __FUNCTION__,                   \
            #handle, &(handle)->block(), (handle)->m_state);    \
    } while (false)
#else
#define HEAP_LOG_BLOCK_STATE_TRANSITION(handle) ((void)0)
#endif

// A marked block is a page-aligned container for heap-allocated objects.
// Objects are allocated within cells of the marked block. For a given
// marked block, all cells have the same size. Objects smaller than the
// cell size may be allocated in the marked block, in which case the
// allocation suffers from internal fragmentation: wasted space whose
// size is equal to the difference between the cell size and the object
// size.

class MarkedBlock {
    WTF_MAKE_NONCOPYABLE(MarkedBlock);
    friend class LLIntOffsetsExtractor;
    friend struct VerifyMarked;

public:
    class Handle;
private:
    friend class Handle;
public:
    enum BlockState : uint8_t { New, FreeListed, Allocated, Marked };
        
    static const size_t atomSize = 16; // bytes
    static const size_t blockSize = 16 * KB;
    static const size_t blockMask = ~(blockSize - 1); // blockSize must be a power of two.

    static const size_t atomsPerBlock = blockSize / atomSize;

    static_assert(!(MarkedBlock::atomSize & (MarkedBlock::atomSize - 1)), "MarkedBlock::atomSize must be a power of two.");
    static_assert(!(MarkedBlock::blockSize & (MarkedBlock::blockSize - 1)), "MarkedBlock::blockSize must be a power of two.");

    struct VoidFunctor {
        typedef void ReturnType;
        void returnValue() { }
    };

    class CountFunctor {
    public:
        typedef size_t ReturnType;

        CountFunctor() : m_count(0) { }
        void count(size_t count) const { m_count += count; }
        ReturnType returnValue() const { return m_count; }

    private:
        // FIXME: This is mutable because we're using a functor rather than C++ lambdas.
        // https://bugs.webkit.org/show_bug.cgi?id=159644
        mutable ReturnType m_count;
    };
        
    class Handle : public BasicRawSentinelNode<Handle> {
        WTF_MAKE_NONCOPYABLE(Handle);
        WTF_MAKE_FAST_ALLOCATED;
        friend class DoublyLinkedListNode<Handle>;
        friend class LLIntOffsetsExtractor;
        friend class MarkedBlock;
        friend struct VerifyMarked;
    public:
            
        ~Handle();
            
        MarkedBlock& block();
            
        void* cellAlign(void*);
            
        bool isEmpty();

        void lastChanceToFinalize();

        MarkedAllocator* allocator() const;
        Heap* heap() const;
        VM* vm() const;
        WeakSet& weakSet();
            
        enum SweepMode { SweepOnly, SweepToFreeList };
        FreeList sweep(SweepMode = SweepOnly);
        
        void unsweepWithNoNewlyAllocated();
        
        void zap(const FreeList&);
        
        void shrink();
            
        unsigned visitWeakSet(HeapRootVisitor&);
        void reapWeakSet();
            
        // While allocating from a free list, MarkedBlock temporarily has bogus
        // cell liveness data. To restore accurate cell liveness data, call one
        // of these functions:
        void didConsumeFreeList(); // Call this once you've allocated all the items in the free list.
        void stopAllocating(const FreeList&);
        FreeList resumeAllocating(); // Call this if you canonicalized a block for some non-collection related purpose.
            
        // Returns true if the "newly allocated" bitmap was non-null 
        // and was successfully cleared and false otherwise.
        bool clearNewlyAllocated();
            
        void flipForEdenCollection();
            
        size_t cellSize();
        const AllocatorAttributes& attributes() const;
        DestructionMode destruction() const;
        bool needsDestruction() const;
        HeapCell::Kind cellKind() const;
            
        size_t markCount();
        size_t size();
            
        bool isLive(const HeapCell*);
        bool isLiveCell(const void*);
        bool isMarkedOrNewlyAllocated(const HeapCell*);
            
        bool isNewlyAllocated(const void*);
        void setNewlyAllocated(const void*);
        void clearNewlyAllocated(const void*);
        
        bool hasAnyNewlyAllocated() const { return !!m_newlyAllocated; }
            
        bool isAllocated() const;
        bool isMarked() const;
        bool isFreeListed() const;
        bool needsSweeping() const;
        void willRemoveBlock();

        template <typename Functor> IterationStatus forEachCell(const Functor&);
        template <typename Functor> IterationStatus forEachLiveCell(const Functor&);
        template <typename Functor> IterationStatus forEachDeadCell(const Functor&);
            
        bool needsFlip();
            
        void flipIfNecessaryConcurrently(uint64_t heapVersion);
        void flipIfNecessary(uint64_t heapVersion);
        void flipIfNecessary();
            
        void assertFlipped();
            
        bool isOnBlocksToSweep() const { return m_isOnBlocksToSweep; }
        void setIsOnBlocksToSweep(bool value) { m_isOnBlocksToSweep = value; }
        
        BlockState state() const { return m_state; }
            
    private:
        Handle(Heap&, MarkedAllocator*, size_t cellSize, const AllocatorAttributes&, void*);
            
        template<DestructionMode>
        FreeList sweepHelperSelectScribbleMode(SweepMode = SweepOnly);
            
        enum ScribbleMode { DontScribble, Scribble };
            
        template<DestructionMode, ScribbleMode>
        FreeList sweepHelperSelectStateAndSweepMode(SweepMode = SweepOnly);
            
        enum NewlyAllocatedMode { HasNewlyAllocated, DoesNotHaveNewlyAllocated };
            
        template<BlockState, SweepMode, DestructionMode, ScribbleMode, NewlyAllocatedMode>
        FreeList specializedSweep();
            
        template<typename Func>
        void forEachFreeCell(const FreeList&, const Func&);
            
        MarkedBlock::Handle* m_prev;
        MarkedBlock::Handle* m_next;
            
        size_t m_atomsPerCell;
        size_t m_endAtom; // This is a fuzzy end. Always test for < m_endAtom.
            
        std::unique_ptr<WTF::Bitmap<atomsPerBlock>> m_newlyAllocated;
            
        AllocatorAttributes m_attributes;
        BlockState m_state;
        bool m_isOnBlocksToSweep { false };
            
        MarkedAllocator* m_allocator;
        WeakSet m_weakSet;
            
        MarkedBlock* m_block;
    };
        
    static MarkedBlock::Handle* tryCreate(Heap&, MarkedAllocator*, size_t cellSize, const AllocatorAttributes&);
        
    Handle& handle();
        
    VM* vm() const;

    static bool isAtomAligned(const void*);
    static MarkedBlock* blockFor(const void*);
    static size_t firstAtom();
    size_t atomNumber(const void*);
        
    size_t markCount();

    bool isMarked(const void*);
    bool testAndSetMarked(const void*);
        
    bool isMarkedOrNewlyAllocated(const HeapCell*);

    bool isAtom(const void*);
    void setMarked(const void*);
    void clearMarked(const void*);
        
    size_t cellSize();
    const AllocatorAttributes& attributes() const;

    bool hasAnyMarked() const;
    void noteMarked();
        
    WeakSet& weakSet();

    bool needsFlip();
        
    void flipIfNecessaryConcurrently(uint64_t heapVersion);
    void flipIfNecessary(uint64_t heapVersion);
    void flipIfNecessary();
        
    void assertFlipped();
        
    bool needsDestruction() const { return m_needsDestruction; }
        
private:
    static const size_t atomAlignmentMask = atomSize - 1;

    typedef char Atom[atomSize];

    MarkedBlock(VM&, Handle&);
    Atom* atoms();
        
    void flipIfNecessaryConcurrentlySlow();
    void flipIfNecessarySlow();
    void clearMarks();
    void clearHasAnyMarked();
    
    void noteMarkedSlow();
        
    WTF::Bitmap<atomsPerBlock, WTF::BitmapAtomic, uint8_t> m_marks;

    bool m_needsDestruction;
    Lock m_lock;
    
    // The actual mark count can be computed by doing: m_biasedMarkCount - m_markCountBias. Note
    // that this count is racy. It will accurately detect whether or not exactly zero things were
    // marked, but if N things got marked, then this may report anything in the range [1, N] (or
    // before unbiased, it would be [1 + m_markCountBias, N + m_markCountBias].)
    int16_t m_biasedMarkCount;
    
    // We bias the mark count so that if m_biasedMarkCount >= 0 then the block should be retired.
    // We go to all this trouble to make marking a bit faster: this way, marking knows when to
    // retire a block using a js/jns on m_biasedMarkCount.
    //
    // For example, if a block has room for 100 objects and retirement happens whenever 90% are
    // live, then m_markCountBias will be -90. This way, when marking begins, this will cause us to
    // set m_biasedMarkCount to -90 as well, since:
    //
    //     m_biasedMarkCount = actualMarkCount + m_markCountBias.
    //
    // Marking an object will increment m_biasedMarkCount. Once 90 objects get marked, we will have
    // m_biasedMarkCount = 0, which will trigger retirement. In other words, we want to set
    // m_markCountBias like so:
    //
    //     m_markCountBias = -(minMarkedBlockUtilization * cellsPerBlock)
    //
    // All of this also means that you can detect if any objects are marked by doing:
    //
    //     m_biasedMarkCount != m_markCountBias
    int16_t m_markCountBias;
    
    Handle& m_handle;
    VM* m_vm;
        
    uint64_t m_version;
};

inline MarkedBlock::Handle& MarkedBlock::handle()
{
    return m_handle;
}

inline MarkedBlock& MarkedBlock::Handle::block()
{
    return *m_block;
}

inline size_t MarkedBlock::firstAtom()
{
    return WTF::roundUpToMultipleOf<atomSize>(sizeof(MarkedBlock)) / atomSize;
}

inline MarkedBlock::Atom* MarkedBlock::atoms()
{
    return reinterpret_cast<Atom*>(this);
}

inline bool MarkedBlock::isAtomAligned(const void* p)
{
    return !(reinterpret_cast<Bits>(p) & atomAlignmentMask);
}

inline void* MarkedBlock::Handle::cellAlign(void* p)
{
    Bits base = reinterpret_cast<Bits>(block().atoms() + firstAtom());
    Bits bits = reinterpret_cast<Bits>(p);
    bits -= base;
    bits -= bits % cellSize();
    bits += base;
    return reinterpret_cast<void*>(bits);
}

inline MarkedBlock* MarkedBlock::blockFor(const void* p)
{
    return reinterpret_cast<MarkedBlock*>(reinterpret_cast<Bits>(p) & blockMask);
}

inline MarkedAllocator* MarkedBlock::Handle::allocator() const
{
    return m_allocator;
}

inline Heap* MarkedBlock::Handle::heap() const
{
    return m_weakSet.heap();
}

inline VM* MarkedBlock::Handle::vm() const
{
    return m_weakSet.vm();
}

inline VM* MarkedBlock::vm() const
{
    return m_vm;
}

inline WeakSet& MarkedBlock::Handle::weakSet()
{
    return m_weakSet;
}

inline WeakSet& MarkedBlock::weakSet()
{
    return m_handle.weakSet();
}

inline void MarkedBlock::Handle::shrink()
{
    m_weakSet.shrink();
}

inline unsigned MarkedBlock::Handle::visitWeakSet(HeapRootVisitor& heapRootVisitor)
{
    return m_weakSet.visit(heapRootVisitor);
}

inline void MarkedBlock::Handle::reapWeakSet()
{
    m_weakSet.reap();
}

inline size_t MarkedBlock::Handle::cellSize()
{
    return m_atomsPerCell * atomSize;
}

inline size_t MarkedBlock::cellSize()
{
    return m_handle.cellSize();
}

inline const AllocatorAttributes& MarkedBlock::Handle::attributes() const
{
    return m_attributes;
}

inline const AllocatorAttributes& MarkedBlock::attributes() const
{
    return m_handle.attributes();
}

inline bool MarkedBlock::Handle::needsDestruction() const
{
    return m_attributes.destruction == NeedsDestruction;
}

inline DestructionMode MarkedBlock::Handle::destruction() const
{
    return m_attributes.destruction;
}

inline HeapCell::Kind MarkedBlock::Handle::cellKind() const
{
    return m_attributes.cellKind;
}

inline size_t MarkedBlock::Handle::markCount()
{
    return m_block->markCount();
}

inline size_t MarkedBlock::Handle::size()
{
    return markCount() * cellSize();
}

inline size_t MarkedBlock::atomNumber(const void* p)
{
    return (reinterpret_cast<Bits>(p) - reinterpret_cast<Bits>(this)) / atomSize;
}

inline void MarkedBlock::flipIfNecessary(uint64_t heapVersion)
{
    if (UNLIKELY(heapVersion != m_version))
        flipIfNecessarySlow();
}

inline void MarkedBlock::flipIfNecessaryConcurrently(uint64_t heapVersion)
{
    if (UNLIKELY(heapVersion != m_version))
        flipIfNecessaryConcurrentlySlow();
    WTF::loadLoadFence();
}

inline void MarkedBlock::Handle::flipIfNecessary(uint64_t heapVersion)
{
    block().flipIfNecessary(heapVersion);
}

inline void MarkedBlock::Handle::flipIfNecessaryConcurrently(uint64_t heapVersion)
{
    block().flipIfNecessaryConcurrently(heapVersion);
}

inline void MarkedBlock::Handle::flipForEdenCollection()
{
    assertFlipped();
        
    HEAP_LOG_BLOCK_STATE_TRANSITION(this);
    
    ASSERT(m_state != New && m_state != FreeListed);
    
    m_state = Marked;
}

#if ASSERT_DISABLED
inline void MarkedBlock::assertFlipped()
{
}
#endif // ASSERT_DISABLED

inline void MarkedBlock::Handle::assertFlipped()
{
    block().assertFlipped();
}

inline bool MarkedBlock::isMarked(const void* p)
{
    assertFlipped();
    return m_marks.get(atomNumber(p));
}

inline bool MarkedBlock::testAndSetMarked(const void* p)
{
    assertFlipped();
    return m_marks.concurrentTestAndSet(atomNumber(p));
}

inline bool MarkedBlock::Handle::isNewlyAllocated(const void* p)
{
    return m_newlyAllocated->get(m_block->atomNumber(p));
}

inline void MarkedBlock::Handle::setNewlyAllocated(const void* p)
{
    m_newlyAllocated->set(m_block->atomNumber(p));
}

inline void MarkedBlock::Handle::clearNewlyAllocated(const void* p)
{
    m_newlyAllocated->clear(m_block->atomNumber(p));
}

inline bool MarkedBlock::Handle::clearNewlyAllocated()
{
    if (m_newlyAllocated) {
        m_newlyAllocated = nullptr;
        return true;
    }
    return false;
}

inline bool MarkedBlock::Handle::isMarkedOrNewlyAllocated(const HeapCell* cell)
{
    ASSERT(m_state == Marked);
    return m_block->isMarked(cell) || (m_newlyAllocated && isNewlyAllocated(cell));
}

inline bool MarkedBlock::isMarkedOrNewlyAllocated(const HeapCell* cell)
{
    ASSERT(m_handle.m_state == Marked);
    return isMarked(cell) || (m_handle.m_newlyAllocated && m_handle.isNewlyAllocated(cell));
}

inline bool MarkedBlock::Handle::isLive(const HeapCell* cell)
{
    assertFlipped();
    switch (m_state) {
    case Allocated:
        return true;

    case Marked:
        return isMarkedOrNewlyAllocated(cell);

    case New:
    case FreeListed:
        RELEASE_ASSERT_NOT_REACHED();
        return false;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

inline bool MarkedBlock::isAtom(const void* p)
{
    ASSERT(MarkedBlock::isAtomAligned(p));
    size_t atomNumber = this->atomNumber(p);
    size_t firstAtom = MarkedBlock::firstAtom();
    if (atomNumber < firstAtom) // Filters pointers into MarkedBlock metadata.
        return false;
    if ((atomNumber - firstAtom) % m_handle.m_atomsPerCell) // Filters pointers into cell middles.
        return false;
    if (atomNumber >= m_handle.m_endAtom) // Filters pointers into invalid cells out of the range.
        return false;
    return true;
}

inline bool MarkedBlock::Handle::isLiveCell(const void* p)
{
    if (!m_block->isAtom(p))
        return false;
    return isLive(static_cast<const HeapCell*>(p));
}

template <typename Functor>
inline IterationStatus MarkedBlock::Handle::forEachCell(const Functor& functor)
{
    HeapCell::Kind kind = m_attributes.cellKind;
    for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell) {
        HeapCell* cell = reinterpret_cast_ptr<HeapCell*>(&m_block->atoms()[i]);
        if (functor(cell, kind) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    return IterationStatus::Continue;
}

template <typename Functor>
inline IterationStatus MarkedBlock::Handle::forEachLiveCell(const Functor& functor)
{
    flipIfNecessary();
    HeapCell::Kind kind = m_attributes.cellKind;
    for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell) {
        HeapCell* cell = reinterpret_cast_ptr<HeapCell*>(&m_block->atoms()[i]);
        if (!isLive(cell))
            continue;

        if (functor(cell, kind) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    return IterationStatus::Continue;
}

template <typename Functor>
inline IterationStatus MarkedBlock::Handle::forEachDeadCell(const Functor& functor)
{
    flipIfNecessary();
    HeapCell::Kind kind = m_attributes.cellKind;
    for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell) {
        HeapCell* cell = reinterpret_cast_ptr<HeapCell*>(&m_block->atoms()[i]);
        if (isLive(cell))
            continue;

        if (functor(cell, kind) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    return IterationStatus::Continue;
}

inline bool MarkedBlock::Handle::needsSweeping() const
{
    const_cast<MarkedBlock::Handle*>(this)->flipIfNecessary();
    return m_state == Marked;
}

inline bool MarkedBlock::Handle::isAllocated() const
{
    const_cast<MarkedBlock::Handle*>(this)->flipIfNecessary();
    return m_state == Allocated;
}

inline bool MarkedBlock::Handle::isMarked() const
{
    const_cast<MarkedBlock::Handle*>(this)->flipIfNecessary();
    return m_state == Marked;
}

inline bool MarkedBlock::Handle::isFreeListed() const
{
    const_cast<MarkedBlock::Handle*>(this)->flipIfNecessary();
    return m_state == FreeListed;
}

inline bool MarkedBlock::hasAnyMarked() const
{
    return m_biasedMarkCount != m_markCountBias;
}

inline void MarkedBlock::noteMarked()
{
    // This is racy by design. We don't want to pay the price of an atomic increment!
    int16_t biasedMarkCount = m_biasedMarkCount;
    ++biasedMarkCount;
    m_biasedMarkCount = biasedMarkCount;
    if (UNLIKELY(!biasedMarkCount))
        noteMarkedSlow();
}

} // namespace JSC

namespace WTF {

struct MarkedBlockHash : PtrHash<JSC::MarkedBlock*> {
    static unsigned hash(JSC::MarkedBlock* const& key)
    {
        // Aligned VM regions tend to be monotonically increasing integers,
        // which is a great hash function, but we have to remove the low bits,
        // since they're always zero, which is a terrible hash function!
        return reinterpret_cast<JSC::Bits>(key) / JSC::MarkedBlock::blockSize;
    }
};

template<> struct DefaultHash<JSC::MarkedBlock*> {
    typedef MarkedBlockHash Hash;
};

void printInternal(PrintStream& out, JSC::MarkedBlock::BlockState);

} // namespace WTF

#endif // MarkedBlock_h
