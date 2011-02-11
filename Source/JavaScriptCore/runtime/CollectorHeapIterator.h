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
#include "Heap.h"

#ifndef CollectorHeapIterator_h
#define CollectorHeapIterator_h

namespace JSC {

    class CollectorHeapIterator {
    public:
        bool operator!=(const CollectorHeapIterator& other);
        JSCell* operator*() const;
    
    protected:
        CollectorHeapIterator(CollectorHeap&, size_t startBlock, size_t startCell);
        void advance();
        bool isValid();
        bool isLive();

        CollectorHeap& m_heap;
        size_t m_block;
        size_t m_cell;
    };

    class LiveObjectIterator : public CollectorHeapIterator {
    public:
        LiveObjectIterator(CollectorHeap&, size_t startBlock, size_t startCell = 0);
        LiveObjectIterator& operator++();
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

    inline bool CollectorHeapIterator::isValid()
    {
        return m_block < m_heap.blocks.size();
    }

    inline bool CollectorHeapIterator::isLive()
    {
        return m_heap.collectorBlock(m_block)->marked.get(m_cell);
    }

    inline JSCell* CollectorHeapIterator::operator*() const
    {
        return reinterpret_cast<JSCell*>(&m_heap.collectorBlock(m_block)->cells[m_cell]);
    }
    
    // Iterators advance up to the next-to-last -- and not the last -- cell in a
    // block, since the last cell is a dummy sentinel.
    inline void CollectorHeapIterator::advance()
    {
        ++m_cell;
        if (m_cell == HeapConstants::cellsPerBlock - 1) {
            m_cell = 0;
            ++m_block;
        }
    }

    inline LiveObjectIterator::LiveObjectIterator(CollectorHeap& heap, size_t startBlock, size_t startCell)
        : CollectorHeapIterator(heap, startBlock, startCell)
    {
        if (isValid() && !isLive())
            ++(*this);
    }

    inline LiveObjectIterator& LiveObjectIterator::operator++()
    {
        do {
            advance();
        } while (isValid() && !isLive());
        return *this;
    }

} // namespace JSC

#endif // CollectorHeapIterator_h
