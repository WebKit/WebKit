/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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
#include "Collector.h"

#ifndef CollectorHeapIterator_h
#define CollectorHeapIterator_h

namespace JSC {

    class CollectorHeapIterator {
    public:
        bool operator!=(const CollectorHeapIterator& other);
        JSCell* operator*() const;
    
    protected:
        CollectorHeapIterator(CollectorHeap&, size_t startBlock, size_t startCell);
        void advance(size_t max);

        CollectorHeap& m_heap;
        size_t m_block;
        size_t m_cell;
    };

    class LiveObjectIterator : public CollectorHeapIterator {
    public:
        LiveObjectIterator(CollectorHeap&, size_t startBlock, size_t startCell = 0);
        LiveObjectIterator& operator++();
    };

    class DeadObjectIterator : public CollectorHeapIterator {
    public:
        DeadObjectIterator(CollectorHeap&, size_t startBlock, size_t startCell = 0);
        DeadObjectIterator& operator++();
    };

    class ObjectIterator : public CollectorHeapIterator {
    public:
        ObjectIterator(CollectorHeap&, size_t startBlock, size_t startCell = 0);
        ObjectIterator& operator++();
    };

    inline CollectorHeapIterator::CollectorHeapIterator(CollectorHeap& heap, size_t startBlock, size_t startCell)
        : m_heap(heap)
        , m_block(startBlock)
        , m_cell(startCell)
    {
    }

    inline bool CollectorHeapIterator::operator!=(const CollectorHeapIterator& other)
    {
        return m_block != other.m_block || m_cell != other.m_cell;
    }

    inline JSCell* CollectorHeapIterator::operator*() const
    {
        return reinterpret_cast<JSCell*>(&m_heap.collectorBlock(m_block)->cells[m_cell]);
    }
    
    // Iterators advance up to the next-to-last -- and not the last -- cell in a
    // block, since the last cell is a dummy sentinel.
    inline void CollectorHeapIterator::advance(size_t max)
    {
        ++m_cell;
        if (m_cell == max) {
            m_cell = 0;
            ++m_block;
        }
    }

    inline LiveObjectIterator::LiveObjectIterator(CollectorHeap& heap, size_t startBlock, size_t startCell)
        : CollectorHeapIterator(heap, startBlock, startCell - 1)
    {
        ++(*this);
    }

    inline LiveObjectIterator& LiveObjectIterator::operator++()
    {
        advance(HeapConstants::cellsPerBlock - 1);
        if (m_block < m_heap.nextBlock || (m_block == m_heap.nextBlock && m_cell < m_heap.nextCell))
            return *this;

        while (m_block < m_heap.usedBlocks && !m_heap.collectorBlock(m_block)->marked.get(m_cell))
            advance(HeapConstants::cellsPerBlock - 1);
        return *this;
    }

    inline DeadObjectIterator::DeadObjectIterator(CollectorHeap& heap, size_t startBlock, size_t startCell)
        : CollectorHeapIterator(heap, startBlock, startCell - 1)
    {
        ++(*this);
    }

    inline DeadObjectIterator& DeadObjectIterator::operator++()
    {
        do {
            advance(HeapConstants::cellsPerBlock - 1);
            ASSERT(m_block > m_heap.nextBlock || (m_block == m_heap.nextBlock && m_cell >= m_heap.nextCell));
        } while (m_block < m_heap.usedBlocks && m_heap.collectorBlock(m_block)->marked.get(m_cell));
        return *this;
    }

    inline ObjectIterator::ObjectIterator(CollectorHeap& heap, size_t startBlock, size_t startCell)
        : CollectorHeapIterator(heap, startBlock, startCell - 1)
    {
        ++(*this);
    }

    inline ObjectIterator& ObjectIterator::operator++()
    {
        advance(HeapConstants::cellsPerBlock - 1);
        return *this;
    }

} // namespace JSC

#endif // CollectorHeapIterator_h
