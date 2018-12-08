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
    m_rawBuffer = fastZeroedMalloc(sizeof(LinkingData) + s_offsetTableSize);
}

ALWAYS_INLINE UnlinkedMetadataTable::~UnlinkedMetadataTable()
{
    ASSERT(!m_isLinked);
    if (m_hasMetadata || !m_isFinalized)
        fastFree(m_rawBuffer);
}

ALWAYS_INLINE unsigned UnlinkedMetadataTable::addEntry(OpcodeID opcodeID)
{
    ASSERT(!m_isFinalized && opcodeID < s_offsetTableEntries - 1);
    m_hasMetadata = true;
    return buffer()[opcodeID]++;
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

    // In this case, we return the size of the table minus the offset table,
    // which was already accounted for in the UnlinkedCodeBlock.
    if (metadataTable.buffer() == buffer()) {
        ASSERT(m_isLinked);
        return buffer()[s_offsetTableEntries - 1] - s_offsetTableSize;
    }

    return metadataTable.buffer()[s_offsetTableEntries - 1];
}

ALWAYS_INLINE void UnlinkedMetadataTable::finalize()
{
    ASSERT(!m_isFinalized);
    m_isFinalized = true;
    if (!m_hasMetadata) {
        fastFree(m_rawBuffer);
        m_rawBuffer = nullptr;
        return;
    }

    unsigned offset = s_offsetTableSize;
    for (unsigned i = 0; i < s_offsetTableEntries - 1; i++) {
        unsigned numberOfEntries = buffer()[i];

        if (numberOfEntries > 0) {
            offset = roundUpToMultipleOf(metadataAlignment(static_cast<OpcodeID>(i)), offset);
            buffer()[i] = offset;
            offset += numberOfEntries * metadataSize(static_cast<OpcodeID>(i));
        } else
            buffer()[i] = offset;
    }
    buffer()[s_offsetTableEntries - 1] = offset;
}

ALWAYS_INLINE RefPtr<MetadataTable> UnlinkedMetadataTable::link()
{
    ASSERT(m_isFinalized);

    if (!m_hasMetadata)
        return nullptr;

    unsigned totalSize = buffer()[s_offsetTableEntries - 1];
    uint8_t* buffer;
    if (!m_isLinked) {
        m_isLinked = true;
        m_rawBuffer = buffer = reinterpret_cast<uint8_t*>(fastRealloc(m_rawBuffer, sizeof(LinkingData) + totalSize));
    } else {
        buffer = reinterpret_cast<uint8_t*>(fastMalloc(sizeof(LinkingData) + totalSize));
        memcpy(buffer, m_rawBuffer, sizeof(LinkingData) + s_offsetTableSize);
    }
    memset(buffer + sizeof(LinkingData) + s_offsetTableSize, 0, totalSize - s_offsetTableSize);
    return adoptRef(*new (buffer + sizeof(LinkingData)) MetadataTable(*this));
}

ALWAYS_INLINE void UnlinkedMetadataTable::unlink(MetadataTable& metadataTable)
{
    ASSERT(m_isFinalized);
    if (!m_hasMetadata)
        return;

    if (metadataTable.buffer() == buffer()) {
        ASSERT(m_isLinked);
        m_isLinked = false;
        m_rawBuffer = fastRealloc(m_rawBuffer, sizeof(LinkingData) + s_offsetTableSize);
        return;
    }
    fastFree(&metadataTable.linkingData());
}

} // namespace JSC
