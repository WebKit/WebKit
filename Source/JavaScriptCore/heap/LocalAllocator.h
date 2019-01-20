/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "FreeList.h"
#include "MarkedBlock.h"
#include <wtf/Noncopyable.h>

namespace JSC {

class BlockDirectory;
class GCDeferralContext;

class LocalAllocator : public BasicRawSentinelNode<LocalAllocator> {
    WTF_MAKE_NONCOPYABLE(LocalAllocator);
    
public:
    LocalAllocator(BlockDirectory*);
    ~LocalAllocator();
    
    void* allocate(GCDeferralContext*, AllocationFailureMode);
    
    unsigned cellSize() const { return m_freeList.cellSize(); }

    void stopAllocating();
    void prepareForAllocation();
    void resumeAllocating();
    void stopAllocatingForGood();
    
    static ptrdiff_t offsetOfFreeList();
    static ptrdiff_t offsetOfCellSize();
    
    bool isFreeListedCell(const void*) const;
    
private:
    friend class BlockDirectory;
    
    void reset();
    JS_EXPORT_PRIVATE void* allocateSlowCase(GCDeferralContext*, AllocationFailureMode failureMode);
    void didConsumeFreeList();
    void* tryAllocateWithoutCollecting();
    void* tryAllocateIn(MarkedBlock::Handle*);
    void* allocateIn(MarkedBlock::Handle*);
    ALWAYS_INLINE void doTestCollectionsIfNeeded(GCDeferralContext*);

    BlockDirectory* m_directory;
    FreeList m_freeList;

    MarkedBlock::Handle* m_currentBlock { nullptr };
    MarkedBlock::Handle* m_lastActiveBlock { nullptr };
    
    // After you do something to a block based on one of these cursors, you clear the bit in the
    // corresponding bitvector and leave the cursor where it was.
    size_t m_allocationCursor { 0 }; // Points to the next block that is a candidate for allocation.
};

inline ptrdiff_t LocalAllocator::offsetOfFreeList()
{
    return OBJECT_OFFSETOF(LocalAllocator, m_freeList);
}

inline ptrdiff_t LocalAllocator::offsetOfCellSize()
{
    return OBJECT_OFFSETOF(LocalAllocator, m_freeList) + FreeList::offsetOfCellSize();
}

} // namespace JSC

