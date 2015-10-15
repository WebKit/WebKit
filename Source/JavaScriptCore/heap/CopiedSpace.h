/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef CopiedSpace_h
#define CopiedSpace_h

#include "CopiedAllocator.h"
#include "HeapOperation.h"
#include "TinyBloomFilter.h"
#include <wtf/Assertions.h>
#include <wtf/CheckedBoolean.h>
#include <wtf/DoublyLinkedList.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>

namespace JSC {

class Heap;
class CopiedBlock;

class CopiedSpace {
    friend class CopyVisitor;
    friend class Heap;
    friend class SlotVisitor;
    friend class JIT;
public:
    CopiedSpace(Heap*);
    ~CopiedSpace();
    void init();

    CheckedBoolean tryAllocate(size_t, void**);
    CheckedBoolean tryReallocate(void**, size_t, size_t);
    
    CopiedAllocator& allocator() { return m_allocator; }

    void didStartFullCollection();

    template <HeapOperation collectionType>
    void startedCopying();
    void doneCopying();
    bool isInCopyPhase() const { return m_inCopyingPhase; }

    void pin(CopiedBlock*);
    bool isPinned(void*);

    bool contains(CopiedBlock*);
    bool contains(void*, CopiedBlock*&);
    
    void pinIfNecessary(void* pointer);

    size_t size();
    size_t capacity();

    bool isPagedOut(double deadline);
    bool shouldDoCopyPhase() const { return m_shouldDoCopyPhase; }

    static CopiedBlock* blockFor(void*);

    Heap* heap() const { return m_heap; }
    
    size_t takeBytesRemovedFromOldSpaceDueToReallocation()
    {
        size_t result = 0;
        std::swap(m_bytesRemovedFromOldSpaceDueToReallocation, result);
        return result;
    }

private:
    static bool isOversize(size_t);

    JS_EXPORT_PRIVATE CheckedBoolean tryAllocateSlowCase(size_t, void**);
    CheckedBoolean tryAllocateOversize(size_t, void**);
    CheckedBoolean tryReallocateOversize(void**, size_t, size_t);
    
    void allocateBlock();
    CopiedBlock* allocateBlockForCopyingPhase();

    void doneFillingBlock(CopiedBlock*, CopiedBlock**);
    void recycleEvacuatedBlock(CopiedBlock*, HeapOperation collectionType);
    void recycleBorrowedBlock(CopiedBlock*);

    Heap* m_heap;

    CopiedAllocator m_allocator;

    HashSet<CopiedBlock*> m_blockSet;

    Lock m_toSpaceLock;

    struct CopiedGeneration {
        CopiedGeneration()
            : toSpace(0)
            , fromSpace(0)
        {
        }

        DoublyLinkedList<CopiedBlock>* toSpace;
        DoublyLinkedList<CopiedBlock>* fromSpace;
        
        DoublyLinkedList<CopiedBlock> blocks1;
        DoublyLinkedList<CopiedBlock> blocks2;
        DoublyLinkedList<CopiedBlock> oversizeBlocks;

        TinyBloomFilter blockFilter;
    };

    CopiedGeneration m_oldGen;
    CopiedGeneration m_newGen;

    bool m_inCopyingPhase;
    bool m_shouldDoCopyPhase;

    Lock m_loanedBlocksLock;
    size_t m_numberOfLoanedBlocks;
    
    size_t m_bytesRemovedFromOldSpaceDueToReallocation;

    static const size_t s_maxAllocationSize = CopiedBlock::blockSize / 2;
    static const size_t s_blockMask = ~(CopiedBlock::blockSize - 1);
};

} // namespace JSC

#endif
