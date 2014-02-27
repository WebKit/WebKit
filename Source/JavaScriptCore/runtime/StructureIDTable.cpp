/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include <wtf/DataLog.h>

namespace JSC {

StructureIDTable::StructureIDTable()
    : m_firstFreeOffset(0)
    , m_table(new StructureOrOffset[s_initialSize])
    , m_size(0)
    , m_capacity(s_initialSize)
{
    // We pre-allocate the first offset so that the null Structure
    // can still be represented as the StructureID '0'.
    allocateID(0);
}

StructureIDTable::~StructureIDTable()
{
    delete [] m_table;
}

void StructureIDTable::resize(size_t newCapacity)
{
    // Create the new table.
    StructureOrOffset* newTable = new StructureOrOffset[newCapacity];

    // Copy the contents of the old table to the new table.
    memcpy(newTable, m_table, m_capacity * sizeof(StructureOrOffset));

    // Store fence to make sure we've copied everything before doing the swap.
    WTF::storeStoreFence();

    // Swap the old and new tables.
    StructureOrOffset* oldTable = m_table;
    m_table = newTable;

    // Put the old table (now labeled as new) into the list of old tables.
    m_oldTables.append(oldTable);

    // Update the capacity.
    m_capacity = newCapacity;
}

void StructureIDTable::flushOldTables()
{
    for (unsigned i = 0; i < m_oldTables.size(); ++i)
        delete [] m_oldTables[i];
    m_oldTables.clear();
}

StructureID StructureIDTable::allocateID(Structure* structure)
{
#if USE(JSVALUE64)
    if (!m_firstFreeOffset) {
        RELEASE_ASSERT(m_capacity <= UINT_MAX);
        if (m_size == m_capacity)
            resize(m_capacity * 2);
        ASSERT(m_size < m_capacity);

        StructureOrOffset newEntry;
        newEntry.structure = structure;

        if (m_size == s_unusedID) {
            m_size++;
            return allocateID(structure);
        }

        StructureID result = m_size;
        m_table[result] = newEntry;
        m_size++;
        return result;
    }

    ASSERT(m_firstFreeOffset != s_unusedID);

    StructureID result = m_firstFreeOffset;
    m_firstFreeOffset = m_table[m_firstFreeOffset].offset;
    m_table[result].structure = structure;
    return result;
#else
    return structure;
#endif
}

void StructureIDTable::deallocateID(Structure* structure, StructureID structureID)
{
#if USE(JSVALUE64)
    ASSERT(structureID != s_unusedID);
    RELEASE_ASSERT(m_table[structureID].structure == structure);
    m_table[structureID].offset = m_firstFreeOffset;
    m_firstFreeOffset = structureID;
#else
    UNUSED_PARAM(structure);
    UNUSED_PARAM(structureID);
#endif
}

} // namespace JSC
