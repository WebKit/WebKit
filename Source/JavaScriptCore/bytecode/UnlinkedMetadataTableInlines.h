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

#include "MetadataTable.h"
#include "UnlinkedMetadataTable.h"
#include <wtf/FastMalloc.h>

namespace JSC {

ALWAYS_INLINE UnlinkedMetadataTable::UnlinkedMetadataTable()
{
    m_hasMetadata = false;
    m_isFinalized = false;
    m_isLinked = false;
    m_buffer = reinterpret_cast<Offset*>(fastCalloc(s_offsetTableEntries, sizeof(UnlinkedMetadataTable::Offset)));
}

ALWAYS_INLINE UnlinkedMetadataTable::~UnlinkedMetadataTable()
{
    ASSERT(!m_isLinked);
    if (m_hasMetadata || !m_isFinalized)
        fastFree(m_buffer);
}

ALWAYS_INLINE unsigned UnlinkedMetadataTable::addEntry(OpcodeID opcodeID)
{
    ASSERT(!m_isFinalized && opcodeID < s_offsetTableEntries - 1);
    m_hasMetadata = true;
    return m_buffer[opcodeID]++;
}

ALWAYS_INLINE size_t UnlinkedMetadataTable::sizeInBytes()
{
    if (m_isFinalized && !m_hasMetadata)
        return 0;

    return s_offsetTableSize;
}

ALWAYS_INLINE size_t UnlinkedMetadataTable::sizeInBytes(MetadataTable& metadataTable)
{
    ASSERT(m_isFinalized);

    if (!metadataTable.m_buffer)
        return 0;

    // In this case, we return the size of the table minus the offset table,
    // which was already accounted for in the UnlinkedCodeBlock.
    if (metadataTable.m_buffer == m_buffer) {
        ASSERT(m_isLinked);
        return m_buffer[s_offsetTableEntries - 1] - s_offsetTableSize;
    }

    return metadataTable.m_buffer[s_offsetTableEntries - 1];
}

ALWAYS_INLINE void UnlinkedMetadataTable::finalize()
{
    ASSERT(!m_isFinalized);
    m_isFinalized = true;
    if (!m_hasMetadata) {
        fastFree(m_buffer);
        m_buffer = nullptr;
        return;
    }

    unsigned offset = s_offsetTableSize;
    for (unsigned i = 0; i < s_offsetTableEntries - 1; i++) {
        unsigned numberOfEntries = m_buffer[i];

        if (numberOfEntries > 0) {
#if CPU(NEEDS_ALIGNED_ACCESS)
            offset = roundUpToMultipleOf(metadataAlignment(static_cast<OpcodeID>(i)), offset);
#endif
            m_buffer[i] = offset;
            offset += numberOfEntries * metadataSize(static_cast<OpcodeID>(i));
        } else
            m_buffer[i] = offset;
    }
    m_buffer[s_offsetTableEntries - 1] = offset;
}

ALWAYS_INLINE Ref<MetadataTable> UnlinkedMetadataTable::link()
{
    ASSERT(m_isFinalized);

    if (!m_hasMetadata)
        return adoptRef(*new MetadataTable(*this, nullptr));

    unsigned totalSize = m_buffer[s_offsetTableEntries - 1];
    Offset* buffer;
    if (!m_isLinked) {
        m_isLinked = true;
        buffer = m_buffer = reinterpret_cast<Offset*>(fastRealloc(m_buffer, totalSize));
    } else {
        buffer = reinterpret_cast<Offset*>(fastMalloc(totalSize));
        memcpy(buffer, m_buffer, s_offsetTableSize);
    }
    memset(reinterpret_cast<uint8_t*>(buffer) + s_offsetTableSize, 0, totalSize - s_offsetTableSize);
    return adoptRef(*new MetadataTable(*this, buffer));
}

ALWAYS_INLINE void UnlinkedMetadataTable::unlink(MetadataTable& metadataTable)
{
    ASSERT(m_isFinalized);
    if (!m_hasMetadata)
        return;

    if (metadataTable.m_buffer == m_buffer) {
        ASSERT(m_isLinked);
        m_isLinked = false;
        m_buffer = reinterpret_cast<Offset*>(fastRealloc(m_buffer, s_offsetTableSize));
    } else if (metadataTable.m_buffer)
        fastFree(metadataTable.m_buffer);
}

} // namespace JSC
