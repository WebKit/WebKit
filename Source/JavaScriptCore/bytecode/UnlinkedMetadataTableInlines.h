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
    : m_hasMetadata(false)
    , m_isFinalized(false)
    , m_isLinked(false)
    , m_is32Bit(false)
    , m_rawBuffer(static_cast<uint8_t*>(MetadataTableMalloc::zeroedMalloc(sizeof(LinkingData) + s_offset32TableSize)))
{
}

ALWAYS_INLINE UnlinkedMetadataTable::UnlinkedMetadataTable(bool is32Bit)
    : m_hasMetadata(false)
    , m_isFinalized(false)
    , m_isLinked(false)
    , m_is32Bit(is32Bit)
    , m_rawBuffer(static_cast<uint8_t*>(MetadataTableMalloc::zeroedMalloc(sizeof(LinkingData) + (is32Bit ? s_offset16TableSize + s_offset32TableSize : s_offset16TableSize))))
{
}

ALWAYS_INLINE UnlinkedMetadataTable::UnlinkedMetadataTable(EmptyTag)
    : m_hasMetadata(false)
    , m_isFinalized(true)
    , m_isLinked(false)
    , m_is32Bit(false)
    , m_rawBuffer(nullptr)
{
}

ALWAYS_INLINE UnlinkedMetadataTable::~UnlinkedMetadataTable()
{
    ASSERT(!m_isLinked);
    if (m_hasMetadata || !m_isFinalized)
        MetadataTableMalloc::free(m_rawBuffer);
}

ALWAYS_INLINE unsigned UnlinkedMetadataTable::addEntry(OpcodeID opcodeID)
{
    ASSERT(!m_isFinalized && opcodeID < s_offsetTableEntries - 1);
    m_hasMetadata = true;
    return preprocessBuffer()[opcodeID]++;
}

template <typename Bytecode>
ALWAYS_INLINE unsigned UnlinkedMetadataTable::numEntries()
{
    constexpr auto opcodeID = Bytecode::opcodeID;
    ASSERT(!m_isFinalized && opcodeID < s_offsetTableEntries - 1);
    return preprocessBuffer()[opcodeID];
}

ALWAYS_INLINE size_t UnlinkedMetadataTable::sizeInBytes()
{
    if (m_isFinalized && !m_hasMetadata)
        return 0;

    if (m_is32Bit)
        return s_offset16TableSize + s_offset32TableSize;
    return s_offset16TableSize;
}

ALWAYS_INLINE size_t UnlinkedMetadataTable::sizeInBytes(MetadataTable& metadataTable)
{
    ASSERT(m_isFinalized);

    // In this case, we return the size of the table minus the offset table,
    // which was already accounted for in the UnlinkedCodeBlock.

    // Be careful not to touch m_rawBuffer if this metadataTable is not owning it.
    // It is possible that, m_rawBuffer is realloced in the other thread while we are accessing here.
    size_t result = metadataTable.totalSize();
    if (metadataTable.buffer() == buffer()) {
        ASSERT(m_isLinked);
        if (m_is32Bit)
            return result - (s_offset16TableSize + s_offset32TableSize);
        return result - s_offset16TableSize;
    }
    return result;
}

ALWAYS_INLINE RefPtr<MetadataTable> UnlinkedMetadataTable::link()
{
    ASSERT(m_isFinalized);

    if (!m_hasMetadata)
        return nullptr;

    unsigned totalSize = this->totalSize();
    unsigned offsetTableSize = this->offsetTableSize();
    uint8_t* buffer;
    if (!m_isLinked) {
        m_isLinked = true;
        m_rawBuffer = buffer = reinterpret_cast<uint8_t*>(MetadataTableMalloc::realloc(m_rawBuffer, sizeof(LinkingData) + totalSize));
    } else {
#if ENABLE(METADATA_STATISTICS)
        MetadataStatistics::numberOfCopiesFromLinking++;
        MetadataStatistics::linkingCopyMemory += sizeof(LinkingData) + totalSize;
#endif
        buffer = reinterpret_cast<uint8_t*>(MetadataTableMalloc::malloc(sizeof(LinkingData) + totalSize));
        memcpy(buffer, m_rawBuffer, sizeof(LinkingData) + offsetTableSize);
    }
    memset(buffer + sizeof(LinkingData) + offsetTableSize, 0, totalSize - offsetTableSize);
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
        m_rawBuffer = static_cast<uint8_t*>(MetadataTableMalloc::realloc(m_rawBuffer, sizeof(LinkingData) + offsetTableSize()));
        return;
    }
    MetadataTableMalloc::free(&metadataTable.linkingData());
}

} // namespace JSC
