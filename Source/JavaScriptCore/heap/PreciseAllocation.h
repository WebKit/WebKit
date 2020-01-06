/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#include "MarkedBlock.h"
#include "WeakSet.h"

namespace JSC {

class IsoSubspace;
class SlotVisitor;

// WebKit has a good malloc that already knows what to do for large allocations. The GC shouldn't
// have to think about such things. That's where PreciseAllocation comes in. We will allocate large
// objects directly using malloc, and put the PreciseAllocation header just before them. We can detect
// when a HeapCell* is a PreciseAllocation because it will have the MarkedBlock::atomSize / 2 bit set.

class PreciseAllocation : public PackedRawSentinelNode<PreciseAllocation> {
public:
    friend class LLIntOffsetsExtractor;
    friend class IsoSubspace;

    static PreciseAllocation* tryCreate(Heap&, size_t, Subspace*, unsigned indexInSpace);

    static PreciseAllocation* createForLowerTier(Heap&, size_t, Subspace*, uint8_t lowerTierIndex);
    PreciseAllocation* reuseForLowerTier();

    PreciseAllocation* tryReallocate(size_t, Subspace*);
    
    ~PreciseAllocation();
    
    static PreciseAllocation* fromCell(const void* cell)
    {
        return bitwise_cast<PreciseAllocation*>(bitwise_cast<char*>(cell) - headerSize());
    }
    
    HeapCell* cell() const
    {
        return bitwise_cast<HeapCell*>(bitwise_cast<char*>(this) + headerSize());
    }
    
    static bool isPreciseAllocation(HeapCell* cell)
    {
        return bitwise_cast<uintptr_t>(cell) & halfAlignment;
    }
    
    Subspace* subspace() const { return m_subspace; }
    
    void lastChanceToFinalize();
    
    Heap* heap() const { return m_weakSet.heap(); }
    VM& vm() const { return m_weakSet.vm(); }
    WeakSet& weakSet() { return m_weakSet; }

    unsigned indexInSpace() { return m_indexInSpace; }
    void setIndexInSpace(unsigned indexInSpace) { m_indexInSpace = indexInSpace; }
    
    void shrink();
    
    void visitWeakSet(SlotVisitor&);
    void reapWeakSet();
    
    void clearNewlyAllocated() { m_isNewlyAllocated = false; }
    void flip();
    
    bool isNewlyAllocated() const { return m_isNewlyAllocated; }
    ALWAYS_INLINE bool isMarked() { return m_isMarked.load(std::memory_order_relaxed); }
    ALWAYS_INLINE bool isMarked(HeapCell*) { return isMarked(); }
    ALWAYS_INLINE bool isMarked(HeapCell*, Dependency) { return isMarked(); }
    ALWAYS_INLINE bool isMarked(HeapVersion, HeapCell*) { return isMarked(); }
    bool isLive() { return isMarked() || isNewlyAllocated(); }
    
    bool hasValidCell() const { return m_hasValidCell; }
    
    bool isEmpty();
    
    size_t cellSize() const { return m_cellSize; }

    uint8_t lowerTierIndex() const { return m_lowerTierIndex; }
    
    bool aboveLowerBound(const void* rawPtr)
    {
        char* ptr = bitwise_cast<char*>(rawPtr);
        char* begin = bitwise_cast<char*>(cell());
        return ptr >= begin;
    }
    
    bool belowUpperBound(const void* rawPtr)
    {
        char* ptr = bitwise_cast<char*>(rawPtr);
        char* begin = bitwise_cast<char*>(cell());
        char* end = begin + cellSize();
        // We cannot #include IndexingHeader.h because reasons. The fact that IndexingHeader is 8
        // bytes is wired deep into our engine, so this isn't so bad.
        size_t sizeOfIndexingHeader = 8;
        return ptr <= end + sizeOfIndexingHeader;
    }
    
    bool contains(const void* rawPtr)
    {
        return aboveLowerBound(rawPtr) && belowUpperBound(rawPtr);
    }
    
    const CellAttributes& attributes() const { return m_attributes; }
    
    Dependency aboutToMark(HeapVersion) { return Dependency(); }
    
    ALWAYS_INLINE bool testAndSetMarked()
    {
        // This method is usually called when the object is already marked. This avoids us
        // having to CAS in that case. It's profitable to reduce the total amount of CAS
        // traffic.
        if (isMarked())
            return true;
        return m_isMarked.compareExchangeStrong(false, true);
    }
    ALWAYS_INLINE bool testAndSetMarked(HeapCell*, Dependency) { return testAndSetMarked(); }
    void clearMarked() { m_isMarked.store(false); }
    
    void noteMarked() { }
    
#if ASSERT_ENABLED
    void assertValidCell(VM&, HeapCell*) const;
#else
    void assertValidCell(VM&, HeapCell*) const { }
#endif
    
    void sweep();
    
    void destroy();
    
    void dump(PrintStream&) const;

    bool isLowerTier() const { return m_lowerTierIndex != UINT8_MAX; }
    
    static constexpr unsigned alignment = MarkedBlock::atomSize;
    static constexpr unsigned halfAlignment = alignment / 2;
    static constexpr unsigned headerSize() { return ((sizeof(PreciseAllocation) + halfAlignment - 1) & ~(halfAlignment - 1)) | halfAlignment; }

private:
    PreciseAllocation(Heap&, size_t, Subspace*, unsigned indexInSpace, bool adjustedAlignment);
    
    void* basePointer() const;
    
    unsigned m_indexInSpace { 0 };
    size_t m_cellSize;
    bool m_isNewlyAllocated : 1;
    bool m_hasValidCell : 1;
    bool m_adjustedAlignment : 1;
    Atomic<bool> m_isMarked;
    CellAttributes m_attributes;
    uint8_t m_lowerTierIndex { UINT8_MAX };
    Subspace* m_subspace;
    WeakSet m_weakSet;
};

inline void* PreciseAllocation::basePointer() const
{
    if (m_adjustedAlignment)
        return bitwise_cast<char*>(this) - halfAlignment;
    return bitwise_cast<void*>(this);
}

} // namespace JSC

