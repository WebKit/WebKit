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

#include "Opcode.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace JSC {

class VM;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(MetadataTable);

class MetadataTable;

class UnlinkedMetadataTable : public RefCounted<UnlinkedMetadataTable> {
    friend class LLIntOffsetsExtractor;
    friend class MetadataTable;
    friend class CachedMetadataTable;
public:
    static constexpr unsigned s_maxMetadataAlignment = 8;

    struct LinkingData {
        Ref<UnlinkedMetadataTable> unlinkedMetadata;
        unsigned refCount;
    };

    ~UnlinkedMetadataTable();

    unsigned addEntry(OpcodeID);

    size_t sizeInBytes();

    void finalize();

    RefPtr<MetadataTable> link();

    static Ref<UnlinkedMetadataTable> create()
    {
        return adoptRef(*new UnlinkedMetadataTable);
    }

    template <typename Opcode>
    uintptr_t offsetInMetadataTable(const Opcode& opcode)
    {
        ASSERT(m_isFinalized);
        uintptr_t baseTypeOffset = m_is32Bit ? offsetTable32()[Opcode::opcodeID] : offsetTable16()[Opcode::opcodeID];
        baseTypeOffset = roundUpToMultipleOf(alignof(typename Opcode::Metadata), baseTypeOffset);
        return baseTypeOffset + sizeof(typename Opcode::Metadata) * opcode.m_metadataID;
    }

    template <typename Bytecode>
    unsigned numEntries();

    bool isFinalized() { return m_isFinalized; }
    bool hasMetadata() { return m_hasMetadata; }

private:
    enum EmptyTag { Empty };

    UnlinkedMetadataTable();
    UnlinkedMetadataTable(bool is32Bit);
    UnlinkedMetadataTable(EmptyTag);

    static Ref<UnlinkedMetadataTable> create(bool is32Bit)
    {
        return adoptRef(*new UnlinkedMetadataTable(is32Bit));
    }

    static Ref<UnlinkedMetadataTable> empty()
    {
        return adoptRef(*new UnlinkedMetadataTable(Empty));
    }

    void unlink(MetadataTable&);

    size_t sizeInBytes(MetadataTable&);

    unsigned totalSize() const
    {
        ASSERT(m_isFinalized);
        if (m_is32Bit)
            return offsetTable32()[s_offsetTableEntries - 1];
        return offsetTable16()[s_offsetTableEntries - 1];
    }

    unsigned offsetTableSize() const
    {
        ASSERT(m_isFinalized);
        if (m_is32Bit)
            return s_offset16TableSize + s_offset32TableSize;
        return s_offset16TableSize;
    }


    using Offset32 = uint32_t;
    using Offset16 = uint16_t;

    static constexpr unsigned s_offsetTableEntries = NUMBER_OF_BYTECODE_WITH_METADATA + 1; // one extra entry for the "end" offset;

    // Not to break alignment of 32bit offset table, we round up size with sizeof(Offset32).
    static constexpr unsigned s_offset16TableSize = roundUpToMultipleOf<sizeof(Offset32)>(s_offsetTableEntries * sizeof(Offset16));
    // Not to break alignment of the metadata calculated based on the alignment of s_offset16TableSize, s_offset32TableSize must be rounded by 8.
    // Then, s_offset16TableSize and s_offset16TableSize + s_offset32TableSize offer the same alignment characteristics for subsequent Metadata.
    static constexpr unsigned s_offset32TableSize = roundUpToMultipleOf<s_maxMetadataAlignment>(s_offsetTableEntries * sizeof(Offset32));

    Offset32* preprocessBuffer() const { return bitwise_cast<Offset32*>(m_rawBuffer + sizeof(LinkingData)); }
    void* buffer() const { return m_rawBuffer + sizeof(LinkingData); }

    Offset16* offsetTable16() const
    {
        ASSERT(!m_is32Bit);
        return bitwise_cast<Offset16*>(m_rawBuffer + sizeof(LinkingData));
    }
    Offset32* offsetTable32() const
    {
        ASSERT(m_is32Bit);
        return bitwise_cast<Offset32*>(m_rawBuffer + sizeof(LinkingData) + s_offset16TableSize);
    }

    bool m_hasMetadata : 1;
    bool m_isFinalized : 1;
    bool m_isLinked : 1;
    bool m_is32Bit : 1;
    uint8_t* m_rawBuffer;
};

} // namespace JSC
