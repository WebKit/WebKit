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

    template <HeapType heapType> class CollectorHeapIterator {
    public:
        CollectorHeapIterator(CollectorBlock** block, CollectorBlock** endBlock);

        bool operator!=(const CollectorHeapIterator<heapType>& other) { return m_block != other.m_block || m_cell != other.m_cell; }
        CollectorHeapIterator<heapType>& operator++();
        JSCell* operator*() const;
    
    private:
        typedef typename HeapConstants<heapType>::Block Block;
        typedef typename HeapConstants<heapType>::Cell Cell;

        Block** m_block;
        Block** m_endBlock;
        Cell* m_cell;
        Cell* m_endCell;
    };

    template <HeapType heapType> 
    CollectorHeapIterator<heapType>::CollectorHeapIterator(CollectorBlock** block, CollectorBlock** endBlock)
        : m_block(reinterpret_cast<Block**>(block))
        , m_endBlock(reinterpret_cast<Block**>(endBlock))
        , m_cell(m_block == m_endBlock ? 0 : (*m_block)->cells)
        , m_endCell(m_block == m_endBlock ? 0 : (*m_block)->cells + HeapConstants<heapType>::cellsPerBlock)
    {
        if (m_cell && m_cell->u.freeCell.zeroIfFree == 0)
            ++*this;
    }

    template <HeapType heapType> 
    CollectorHeapIterator<heapType>& CollectorHeapIterator<heapType>::operator++()
    {
        do {
            for (++m_cell; m_cell != m_endCell; ++m_cell)
                if (m_cell->u.freeCell.zeroIfFree != 0) {
                    return *this;
                }

            if (++m_block != m_endBlock) {
                m_cell = (*m_block)->cells;
                m_endCell = (*m_block)->cells + HeapConstants<heapType>::cellsPerBlock;
            }
        } while(m_block != m_endBlock);

        m_cell = 0;
        return *this;
    }

    template <HeapType heapType> 
    JSCell* CollectorHeapIterator<heapType>::operator*() const
    {
        return reinterpret_cast<JSCell*>(m_cell);
    }

} // namespace JSC

#endif // CollectorHeapIterator_h
