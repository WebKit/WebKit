/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2021 Apple Inc. All rights reserved.
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

#include "CellAttributes.h"
#include "DestructionMode.h"
#include "HeapCell.h"
#include "IterationStatus.h"
#include "WeakSet.h"
#include <algorithm>
#include <wtf/Atomics.h>
#include <wtf/Bitmap.h>
#include <wtf/CountingLock.h>
#include <wtf/HashFunctions.h>
#include <wtf/PageBlock.h>
#include <wtf/StdLibExtras.h>

namespace JSC {

class AlignedMemoryAllocator;    
class FreeList;
class Heap;
class JSCell;
class BlockDirectory;
class MarkedSpace;
class SlotVisitor;
class Subspace;

typedef uint32_t HeapVersion;

// A marked block is a page-aligned container for heap-allocated objects.
// Objects are allocated within cells of the marked block. For a given
// marked block, all cells have the same size. Objects smaller than the
// cell size may be allocated in the marked block, in which case the
// allocation suffers from internal fragmentation: wasted space whose
// size is equal to the difference between the cell size and the object
// size.
DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(MarkedBlock);
DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(MarkedBlockHandle);
class MarkedBlock {
    WTF_MAKE_NONCOPYABLE(MarkedBlock);
    WTF_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(MarkedBlock);
    friend class LLIntOffsetsExtractor;
    friend struct VerifyMarked;

public:
    class Footer;
    class Handle;
private:
    friend class Footer;
    friend class Handle;
public:
    static constexpr size_t atomSize = 16; // bytes

    // Block size must be at least as large as the system page size.
    static constexpr size_t blockSize = std::max(16 * KB, CeilingOnPageSize);

    static constexpr size_t blockMask = ~(blockSize - 1); // blockSize must be a power of two.

    static constexpr size_t atomsPerBlock = blockSize / atomSize;

    static constexpr size_t maxNumberOfLowerTierCells = 8;
    static_assert(maxNumberOfLowerTierCells <= 256);
    
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
        
    class Handle {
        WTF_MAKE_NONCOPYABLE(Handle);
        WTF_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(MarkedBlockHandle);
        friend class LLIntOffsetsExtractor;
        friend class MarkedBlock;
        friend struct VerifyMarked;
    public:
            
        ~Handle();
            
        MarkedBlock& block();
        MarkedBlock::Footer& blockFooter();
            
        void* cellAlign(void*);
            
        bool isEmpty();

        void lastChanceToFinalize();

        BlockDirectory* directory() const;
        Subspace* subspace() const;
        AlignedMemoryAllocator* alignedMemoryAllocator() const;
        Heap* heap() const;
        inline MarkedSpace* space() const;
        VM& vm() const;
        WeakSet& weakSet();
            
        enum SweepMode { SweepOnly, SweepToFreeList };

        // Sweeping ensures that destructors get called and removes the block from the unswept
        // set. Sweeping to free list also removes the block from the empty set, if it was in that
        // set. Sweeping with SweepOnly may add this block to the empty set, if the block is found
        // to be empty. The free-list being null implies SweepOnly.
        //
        // Note that you need to make sure that the empty bit reflects reality. If it's not set
        // and the block is freshly created, then we'll make the mistake of running destructors in
        // the block. If it's not set and the block has nothing marked, then we'll make the
        // mistake of making a pop freelist rather than a bump freelist.
        void sweep(FreeList*);
        
        // This is to be called by Subspace.
        template<typename DestroyFunc>
        void finishSweepKnowingHeapCellType(FreeList*, const DestroyFunc&);
        
        void unsweepWithNoNewlyAllocated();
        
        void shrink();
            
        // While allocating from a free list, MarkedBlock temporarily has bogus
        // cell liveness data. To restore accurate cell liveness data, call one
        // of these functions:
        void didConsumeFreeList(); // Call this once you've allocated all the items in the free list.
        void stopAllocating(const FreeList&);
        void resumeAllocating(FreeList&); // Call this if you canonicalized a block for some non-collection related purpose.
            
        size_t cellSize();
        inline unsigned cellsPerBlock();
        
        const CellAttributes& attributes() const;
        DestructionMode destruction() const;
        bool needsDestruction() const;
        HeapCell::Kind cellKind() const;
            
        size_t markCount();
        size_t size();
        
        bool isAllocated();
        
        bool isLive(HeapVersion markingVersion, HeapVersion newlyAllocatedVersion, bool isMarking, const HeapCell*);
        inline bool isLiveCell(HeapVersion markingVersion, HeapVersion newlyAllocatedVersion, bool isMarking, const void*);

        bool isLive(const HeapCell*);
        bool isLiveCell(const void*);

        bool isFreeListedCell(const void* target) const;

        template <typename Functor> IterationStatus forEachCell(const Functor&);
        template <typename Functor> inline IterationStatus forEachLiveCell(const Functor&);
        template <typename Functor> inline IterationStatus forEachDeadCell(const Functor&);
        template <typename Functor> inline IterationStatus forEachMarkedCell(const Functor&);
            
        JS_EXPORT_PRIVATE bool areMarksStale();
        bool areMarksStaleForSweep();
        
        void assertMarksNotStale();
            
        bool isFreeListed() const { return m_isFreeListed; }
        
        unsigned index() const { return m_index; }
        
        void removeFromDirectory();
        
        void didAddToDirectory(BlockDirectory*, unsigned index);
        void didRemoveFromDirectory();
        
        void* start() const { return &m_block->atoms()[0]; }
        void* end() const { return &m_block->atoms()[m_endAtom]; }
        void* atomAt(size_t i) const { return &m_block->atoms()[i]; }
        bool contains(void* p) const { return start() <= p && p < end(); }

        void dumpState(PrintStream&);
        
    private:
        Handle(Heap&, AlignedMemoryAllocator*, void*);
        
        enum SweepDestructionMode { BlockHasNoDestructors, BlockHasDestructors, BlockHasDestructorsAndCollectorIsRunning };
        enum ScribbleMode { DontScribble, Scribble };
        enum EmptyMode { IsEmpty, NotEmpty };
        enum NewlyAllocatedMode { HasNewlyAllocated, DoesNotHaveNewlyAllocated };
        enum MarksMode { MarksStale, MarksNotStale };
        
        SweepDestructionMode sweepDestructionMode();
        EmptyMode emptyMode();
        ScribbleMode scribbleMode();
        NewlyAllocatedMode newlyAllocatedMode();
        MarksMode marksMode();
        
        template<bool, EmptyMode, SweepMode, SweepDestructionMode, ScribbleMode, NewlyAllocatedMode, MarksMode, typename DestroyFunc>
        void specializedSweep(FreeList*, EmptyMode, SweepMode, SweepDestructionMode, ScribbleMode, NewlyAllocatedMode, MarksMode, const DestroyFunc&);
        
        void setIsFreeListed();
        
        unsigned m_atomsPerCell { std::numeric_limits<unsigned>::max() };
        unsigned m_endAtom { std::numeric_limits<unsigned>::max() }; // This is a fuzzy end. Always test for < m_endAtom.
            
        CellAttributes m_attributes;
        bool m_isFreeListed { false };
        unsigned m_index { std::numeric_limits<unsigned>::max() };

        AlignedMemoryAllocator* m_alignedMemoryAllocator { nullptr };
        BlockDirectory* m_directory { nullptr };
        WeakSet m_weakSet;
        
        MarkedBlock* m_block { nullptr };
    };

private:    
    static constexpr size_t atomAlignmentMask = atomSize - 1;

    typedef char Atom[atomSize];

public:
    class Footer {
    public:
        Footer(VM&, Handle&);
        ~Footer();

        static ptrdiff_t offsetOfVM() { return OBJECT_OFFSETOF(Footer, m_vm); }
        
    private:
        friend class LLIntOffsetsExtractor;
        friend class MarkedBlock;
        
        Handle& m_handle;
        // m_vm must remain a pointer (instead of a reference) because JSCLLIntOffsetsExtractor
        // will fail otherwise.
        VM* m_vm;
        Subspace* m_subspace;

        CountingLock m_lock;
    
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

        HeapVersion m_markingVersion;
        HeapVersion m_newlyAllocatedVersion;

        Bitmap<atomsPerBlock> m_marks;
        Bitmap<atomsPerBlock> m_newlyAllocated;
        void* m_verifierMemo { nullptr };
    };
    
private:
    Footer& footer();
    const Footer& footer() const;

public:
    static constexpr size_t endAtom = (blockSize - sizeof(Footer)) / atomSize;
    static constexpr size_t payloadSize = endAtom * atomSize;
    static constexpr size_t footerSize = blockSize - payloadSize;

    static_assert(payloadSize == ((blockSize - sizeof(MarkedBlock::Footer)) & ~(atomSize - 1)), "Payload size computed the alternate way should give the same result");
    
    static MarkedBlock::Handle* tryCreate(Heap&, AlignedMemoryAllocator*);
        
    Handle& handle();
    const Handle& handle() const;
        
    VM& vm() const;
    inline Heap* heap() const;
    inline MarkedSpace* space() const;

    static bool isAtomAligned(const void*);
    static MarkedBlock* blockFor(const void*);
    unsigned atomNumber(const void*);
    size_t candidateAtomNumber(const void*);
        
    size_t markCount();

    bool isMarked(const void*);
    bool isMarked(HeapVersion markingVersion, const void*);
    bool isMarked(const void*, Dependency);
    bool testAndSetMarked(const void*, Dependency);
        
    bool isAtom(const void*);
    void clearMarked(const void*);
    
    bool isNewlyAllocated(const void*);
    void setNewlyAllocated(const void*);
    void clearNewlyAllocated(const void*);
    const Bitmap<atomsPerBlock>& newlyAllocated() const;
    
    HeapVersion newlyAllocatedVersion() const { return footer().m_newlyAllocatedVersion; }
    
    inline bool isNewlyAllocatedStale() const;
    
    inline bool hasAnyNewlyAllocated();
    void resetAllocated();
        
    size_t cellSize();
    const CellAttributes& attributes() const;
    
    bool hasAnyMarked() const;
    void noteMarked();
#if ASSERT_ENABLED
    void assertValidCell(VM&, HeapCell*) const;
#else
    void assertValidCell(VM&, HeapCell*) const { }
#endif
        
    WeakSet& weakSet();

    JS_EXPORT_PRIVATE bool areMarksStale();
    bool areMarksStale(HeapVersion markingVersion);
    
    Dependency aboutToMark(HeapVersion markingVersion);
        
#if ASSERT_ENABLED
    JS_EXPORT_PRIVATE void assertMarksNotStale();
#else
    void assertMarksNotStale() { }
#endif
        
    void resetMarks();
    
    bool isMarkedRaw(const void* p);
    HeapVersion markingVersion() const { return footer().m_markingVersion; }
    
    const Bitmap<atomsPerBlock>& marks() const;
    
    CountingLock& lock() { return footer().m_lock; }
    
    Subspace* subspace() const { return footer().m_subspace; }

    void populatePage() const
    {
        *bitwise_cast<volatile uint8_t*>(&footer());
    }
    
    void setVerifierMemo(void*);
    template<typename T> T verifierMemo() const;

    static constexpr size_t offsetOfFooter = endAtom * atomSize;
    static_assert(offsetOfFooter + sizeof(Footer) <= blockSize);

private:
    MarkedBlock(VM&, Handle&);
    ~MarkedBlock();
    Atom* atoms();
        
    JS_EXPORT_PRIVATE void aboutToMarkSlow(HeapVersion markingVersion);
    void clearHasAnyMarked();
    
    void noteMarkedSlow();
    
    inline bool marksConveyLivenessDuringMarking(HeapVersion markingVersion);
    inline bool marksConveyLivenessDuringMarking(HeapVersion myMarkingVersion, HeapVersion markingVersion);
};

inline MarkedBlock::Footer& MarkedBlock::footer()
{
    return *bitwise_cast<MarkedBlock::Footer*>(atoms() + endAtom);
}

inline const MarkedBlock::Footer& MarkedBlock::footer() const
{
    return const_cast<MarkedBlock*>(this)->footer();
}

inline MarkedBlock::Handle& MarkedBlock::handle()
{
    return footer().m_handle;
}

inline const MarkedBlock::Handle& MarkedBlock::handle() const
{
    return const_cast<MarkedBlock*>(this)->handle();
}

inline MarkedBlock& MarkedBlock::Handle::block()
{
    return *m_block;
}

inline MarkedBlock::Footer& MarkedBlock::Handle::blockFooter()
{
    return block().footer();
}

inline MarkedBlock::Atom* MarkedBlock::atoms()
{
    return reinterpret_cast<Atom*>(this);
}

inline bool MarkedBlock::isAtomAligned(const void* p)
{
    return !(reinterpret_cast<uintptr_t>(p) & atomAlignmentMask);
}

inline void* MarkedBlock::Handle::cellAlign(void* p)
{
    uintptr_t base = reinterpret_cast<uintptr_t>(block().atoms());
    uintptr_t bits = reinterpret_cast<uintptr_t>(p);
    bits -= base;
    bits -= bits % cellSize();
    bits += base;
    return reinterpret_cast<void*>(bits);
}

inline MarkedBlock* MarkedBlock::blockFor(const void* p)
{
    return reinterpret_cast<MarkedBlock*>(reinterpret_cast<uintptr_t>(p) & blockMask);
}

inline BlockDirectory* MarkedBlock::Handle::directory() const
{
    return m_directory;
}

inline AlignedMemoryAllocator* MarkedBlock::Handle::alignedMemoryAllocator() const
{
    return m_alignedMemoryAllocator;
}

inline Heap* MarkedBlock::Handle::heap() const
{
    return m_weakSet.heap();
}

inline VM& MarkedBlock::Handle::vm() const
{
    return m_weakSet.vm();
}

inline VM& MarkedBlock::vm() const
{
    return *footer().m_vm;
}

inline WeakSet& MarkedBlock::Handle::weakSet()
{
    return m_weakSet;
}

inline WeakSet& MarkedBlock::weakSet()
{
    return handle().weakSet();
}

inline void MarkedBlock::Handle::shrink()
{
    m_weakSet.shrink();
}

inline size_t MarkedBlock::Handle::cellSize()
{
    return m_atomsPerCell * atomSize;
}

inline size_t MarkedBlock::cellSize()
{
    return handle().cellSize();
}

inline const CellAttributes& MarkedBlock::Handle::attributes() const
{
    return m_attributes;
}

inline const CellAttributes& MarkedBlock::attributes() const
{
    return handle().attributes();
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

inline size_t MarkedBlock::candidateAtomNumber(const void* p)
{
    // This function must return size_t instead of unsigned since pointer |p| is not guaranteed that this is within MarkedBlock.
    // See MarkedBlock::isAtom which can accept out-of-bound pointers.
    return (reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(this)) / atomSize;
}

inline unsigned MarkedBlock::atomNumber(const void* p)
{
    size_t atomNumber = candidateAtomNumber(p);
    ASSERT(atomNumber < handle().m_endAtom);
    return atomNumber;
}

inline bool MarkedBlock::areMarksStale(HeapVersion markingVersion)
{
    return markingVersion != footer().m_markingVersion;
}

inline Dependency MarkedBlock::aboutToMark(HeapVersion markingVersion)
{
    HeapVersion version;
    Dependency dependency = Dependency::loadAndFence(&footer().m_markingVersion, version);
    if (UNLIKELY(version != markingVersion))
        aboutToMarkSlow(markingVersion);
    return dependency;
}

inline void MarkedBlock::Handle::assertMarksNotStale()
{
    block().assertMarksNotStale();
}

inline bool MarkedBlock::isMarkedRaw(const void* p)
{
    return footer().m_marks.get(atomNumber(p));
}

inline bool MarkedBlock::isMarked(HeapVersion markingVersion, const void* p)
{
    HeapVersion version;
    Dependency dependency = Dependency::loadAndFence(&footer().m_markingVersion, version);
    if (UNLIKELY(version != markingVersion))
        return false;
    return footer().m_marks.get(atomNumber(p), dependency);
}

inline bool MarkedBlock::isMarked(const void* p, Dependency dependency)
{
    assertMarksNotStale();
    return footer().m_marks.get(atomNumber(p), dependency);
}

inline bool MarkedBlock::testAndSetMarked(const void* p, Dependency dependency)
{
    assertMarksNotStale();
    return footer().m_marks.concurrentTestAndSet(atomNumber(p), dependency);
}

inline const Bitmap<MarkedBlock::atomsPerBlock>& MarkedBlock::marks() const
{
    return footer().m_marks;
}

inline bool MarkedBlock::isNewlyAllocated(const void* p)
{
    return footer().m_newlyAllocated.get(atomNumber(p));
}

inline void MarkedBlock::setNewlyAllocated(const void* p)
{
    footer().m_newlyAllocated.set(atomNumber(p));
}

inline void MarkedBlock::clearNewlyAllocated(const void* p)
{
    footer().m_newlyAllocated.clear(atomNumber(p));
}

inline const Bitmap<MarkedBlock::atomsPerBlock>& MarkedBlock::newlyAllocated() const
{
    return footer().m_newlyAllocated;
}

inline bool MarkedBlock::isAtom(const void* p)
{
    ASSERT(MarkedBlock::isAtomAligned(p));
    size_t atomNumber = candidateAtomNumber(p);
    if (atomNumber % handle().m_atomsPerCell) // Filters pointers into cell middles.
        return false;
    if (atomNumber >= handle().m_endAtom) // Filters pointers into invalid cells out of the range.
        return false;
    return true;
}

template <typename Functor>
inline IterationStatus MarkedBlock::Handle::forEachCell(const Functor& functor)
{
    HeapCell::Kind kind = m_attributes.cellKind;
    for (size_t i = 0; i < m_endAtom; i += m_atomsPerCell) {
        HeapCell* cell = reinterpret_cast_ptr<HeapCell*>(&m_block->atoms()[i]);
        if (functor(i, cell, kind) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    return IterationStatus::Continue;
}

inline bool MarkedBlock::hasAnyMarked() const
{
    return footer().m_biasedMarkCount != footer().m_markCountBias;
}

inline void MarkedBlock::noteMarked()
{
    // This is racy by design. We don't want to pay the price of an atomic increment!
    int16_t biasedMarkCount = footer().m_biasedMarkCount;
    ++biasedMarkCount;
    footer().m_biasedMarkCount = biasedMarkCount;
    if (UNLIKELY(!biasedMarkCount))
        noteMarkedSlow();
}

inline void MarkedBlock::setVerifierMemo(void* p)
{
    footer().m_verifierMemo = p;
}

template<typename T>
T MarkedBlock::verifierMemo() const
{
    return bitwise_cast<T>(footer().m_verifierMemo);
}

} // namespace JSC

namespace WTF {

struct MarkedBlockHash : PtrHash<JSC::MarkedBlock*> {
    static unsigned hash(JSC::MarkedBlock* const& key)
    {
        // Aligned VM regions tend to be monotonically increasing integers,
        // which is a great hash function, but we have to remove the low bits,
        // since they're always zero, which is a terrible hash function!
        return reinterpret_cast<uintptr_t>(key) / JSC::MarkedBlock::blockSize;
    }
};

template<> struct DefaultHash<JSC::MarkedBlock*> : MarkedBlockHash { };

void printInternal(PrintStream& out, JSC::MarkedBlock::Handle::SweepMode);

} // namespace WTF
