/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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

#include "AllocationFailureMode.h"
#include "AllocatorForMode.h"
#include "Allocator.h"
#include "MarkedBlock.h"
#include "MarkedSpace.h"
#include <wtf/text/CString.h>

namespace JSC {

class AlignedMemoryAllocator;
class HeapCellType;

// The idea of subspaces is that you can provide some custom behavior for your objects if you
// allocate them from a custom Subspace in which you override some of the virtual methods. This
// class is the baseclass of Subspaces. Usually you will use either Subspace or FixedSizeSubspace.
class Subspace {
    WTF_MAKE_NONCOPYABLE(Subspace);
    WTF_MAKE_FAST_ALLOCATED;
public:
    JS_EXPORT_PRIVATE Subspace(CString name, Heap&);
    JS_EXPORT_PRIVATE virtual ~Subspace();

    const char* name() const { return m_name.data(); }
    MarkedSpace& space() const { return m_space; }
    
    const CellAttributes& attributes() const { return m_attributes; }
    HeapCellType* heapCellType() const { return m_heapCellType; }
    AlignedMemoryAllocator* alignedMemoryAllocator() const { return m_alignedMemoryAllocator; }
    
    void finishSweep(MarkedBlock::Handle&, FreeList*);
    void destroy(VM&, JSCell*);

    virtual Allocator allocatorFor(size_t, AllocatorForMode) = 0;
    virtual void* allocate(VM&, size_t, GCDeferralContext*, AllocationFailureMode) = 0;
    
    void prepareForAllocation();
    
    void didCreateFirstDirectory(BlockDirectory* directory) { m_directoryForEmptyAllocation = directory; }
    
    // Finds an empty block from any Subspace that agrees to trade blocks with us.
    MarkedBlock::Handle* findEmptyBlockToSteal();
    
    template<typename Func>
    void forEachDirectory(const Func&);
    
    Ref<SharedTask<BlockDirectory*()>> parallelDirectorySource();
    
    template<typename Func>
    void forEachMarkedBlock(const Func&);
    
    template<typename Func>
    void forEachNotEmptyMarkedBlock(const Func&);
    
    JS_EXPORT_PRIVATE Ref<SharedTask<MarkedBlock::Handle*()>> parallelNotEmptyMarkedBlockSource();
    
    template<typename Func>
    void forEachLargeAllocation(const Func&);
    
    template<typename Func>
    void forEachMarkedCell(const Func&);
    
    template<typename Func>
    Ref<SharedTask<void(SlotVisitor&)>> forEachMarkedCellInParallel(const Func&);

    template<typename Func>
    void forEachLiveCell(const Func&);
    
    void sweep();
    
    Subspace* nextSubspaceInAlignedMemoryAllocator() const { return m_nextSubspaceInAlignedMemoryAllocator; }
    void setNextSubspaceInAlignedMemoryAllocator(Subspace* subspace) { m_nextSubspaceInAlignedMemoryAllocator = subspace; }
    
    virtual void didResizeBits(size_t newSize);
    virtual void didRemoveBlock(size_t blockIndex);
    virtual void didBeginSweepingToFreeList(MarkedBlock::Handle*);

protected:
    void initialize(HeapCellType*, AlignedMemoryAllocator*);
    
    MarkedSpace& m_space;
    
    CString m_name;
    CellAttributes m_attributes;

    HeapCellType* m_heapCellType { nullptr };
    AlignedMemoryAllocator* m_alignedMemoryAllocator { nullptr };
    
    BlockDirectory* m_firstDirectory { nullptr };
    BlockDirectory* m_directoryForEmptyAllocation { nullptr }; // Uses the MarkedSpace linked list of blocks.
    SentinelLinkedList<LargeAllocation, BasicRawSentinelNode<LargeAllocation>> m_largeAllocations;
    Subspace* m_nextSubspaceInAlignedMemoryAllocator { nullptr };
};

} // namespace JSC

