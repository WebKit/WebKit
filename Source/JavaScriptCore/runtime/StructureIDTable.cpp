/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StructureIDTable.h"

#include <limits.h>
#include <wtf/Atomics.h>

namespace JSC {

#if USE(JSVALUE64)

StructureIDTable::StructureIDTable()
    : m_table(makeUniqueArray<StructureOrOffset>(s_initialSize))
    , m_capacity(s_initialSize)
{
    // We pre-allocate the first offset so that the null Structure
    // can still be represented as the StructureID '0'.
    table()[0].structure = nullptr;

    makeFreeListFromRange(1, m_capacity - 1);
    ASSERT(m_size == m_capacity);
}

void StructureIDTable::makeFreeListFromRange(uint32_t first, uint32_t last)
{
    ASSERT(!m_firstFreeOffset);
    ASSERT(!m_lastFreeOffset);

    // Put all the new IDs on the free list sequentially.
    uint32_t head = first;
    uint32_t tail = last;
    for (uint32_t i = first; i < last; ++i)
        table()[i].offset = i + 1;
    table()[last].offset = 0;

    // Randomize the free list.
    uint32_t size = last - first + 1;
    uint32_t maxIterations = (size * 2) / 3;
    for (uint32_t count = 0; count < maxIterations; ++count) {
        // Move a random pick either to the head or the tail of the free list.
        uint32_t random = m_weakRandom.getUint32();
        uint32_t nodeBefore = first + (random % size);
        uint32_t pick = table()[nodeBefore].offset;
        if (pick) {
            uint32_t nodeAfter = table()[pick].offset;
            table()[nodeBefore].offset = nodeAfter;
            if ((random & 1) || !nodeAfter) {
                // Move to the head.
                table()[pick].offset = head;
                head = pick;
                if (!nodeAfter)
                    tail = nodeBefore;
            } else {
                // Move to the tail.
                table()[pick].offset = 0;
                table()[tail].offset = pick;
                tail = pick;
            }
        }
    }

    // Cut list in half and swap halves.
    uint32_t cut = first + (m_weakRandom.getUint32() % size);
    uint32_t afterCut = table()[cut].offset;
    if (afterCut) {
        table()[tail].offset = head;
        tail = cut;
        head = afterCut;
        table()[cut].offset = 0;
    }

    m_firstFreeOffset = head;
    m_lastFreeOffset = tail;
    m_size = m_capacity;
}

void StructureIDTable::resize(size_t newCapacity)
{
    // Create the new table.
    auto newTable = makeUniqueArray<StructureOrOffset>(newCapacity);

    // Copy the contents of the old table to the new table.
    memcpy(newTable.get(), table(), m_capacity * sizeof(StructureOrOffset));

    // Store fence to make sure we've copied everything before doing the swap.
    WTF::storeStoreFence();

    // Swap the old and new tables.
    swap(m_table, newTable);

    // Put the old table (now labeled as new) into the list of old tables.
    m_oldTables.append(WTFMove(newTable));

    // Update the capacity.
    m_capacity = newCapacity;

    makeFreeListFromRange(m_size, m_capacity - 1);
}

void StructureIDTable::flushOldTables()
{
    m_oldTables.clear();
}

StructureID StructureIDTable::allocateID(Structure* structure)
{
    if (!m_firstFreeOffset) {
        RELEASE_ASSERT(m_capacity <= UINT_MAX);
        if (m_size == m_capacity)
            resize(m_capacity * 2);
        ASSERT(m_size == m_capacity);
        ASSERT(m_firstFreeOffset);
    }

    ASSERT(m_firstFreeOffset != s_unusedID);

    StructureID result = m_firstFreeOffset;
    m_firstFreeOffset = table()[m_firstFreeOffset].offset;
    if (!m_firstFreeOffset)
        m_lastFreeOffset = 0;

    table()[result].structure = structure;
    ASSERT(!isNuked(result));
    return result;
}

void StructureIDTable::deallocateID(Structure* structure, StructureID structureID)
{
    ASSERT(structureID != s_unusedID);
    RELEASE_ASSERT(table()[structureID].structure == structure);

    if (!m_firstFreeOffset) {
        table()[structureID].offset = 0;
        m_firstFreeOffset = structureID;
        m_lastFreeOffset = structureID;
        return;
    }

    bool insertAtHead = m_weakRandom.getUint32() & 1;
    if (insertAtHead) {
        table()[structureID].offset = m_firstFreeOffset;
        m_firstFreeOffset = structureID;
    } else {
        table()[structureID].offset = 0;
        table()[m_lastFreeOffset].offset = structureID;
        m_lastFreeOffset = structureID;
    }
}

#endif // USE(JSVALUE64)

} // namespace JSC
