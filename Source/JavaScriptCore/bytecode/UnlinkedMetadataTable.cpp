/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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
#include "UnlinkedMetadataTable.h"

#include "BytecodeStructs.h"

namespace JSC {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(MetadataTable);
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(UnlinkedMetadataTable);

#if CPU(ADDRESS64)
static_assert((UnlinkedMetadataTable::s_maxMetadataAlignment >=
#define JSC_ALIGNMENT_CHECK(size) size) && (size >=
FOR_EACH_BYTECODE_METADATA_ALIGNMENT(JSC_ALIGNMENT_CHECK)
#undef JSC_ALIGNMENT_CHECK
1));
#else
#define JSC_ALIGNMENT_CHECK(size) static_assert(size <= UnlinkedMetadataTable::s_maxMetadataAlignment);
FOR_EACH_BYTECODE_METADATA_ALIGNMENT(JSC_ALIGNMENT_CHECK)
#undef JSC_ALIGNMENT_CHECK
#endif

#if ENABLE(METADATA_STATISTICS)
size_t MetadataStatistics::unlinkedMetadataCount = 0;
size_t MetadataStatistics::size32MetadataCount = 0;
size_t MetadataStatistics::totalMemory = 0;
size_t MetadataStatistics::perOpcodeCount[NUMBER_OF_BYTECODE_WITH_METADATA] { 0 };
size_t MetadataStatistics::numberOfCopiesFromLinking = 0;
size_t MetadataStatistics::linkingCopyMemory = 0;

void MetadataStatistics::reportMetadataStatistics()
{
    static constexpr bool verbose = true;

    dataLogLn("\nMetadata statistics\n");

    totalMemory += unlinkedMetadataCount * sizeof(UnlinkedMetadataTable::LinkingData);
    totalMemory += size32MetadataCount * (UnlinkedMetadataTable::s_offset32TableSize);
    totalMemory += linkingCopyMemory;
    dataLogLn("total memory: ", totalMemory);
    if (verbose)
        dataLogLn("\t of which due to multiple linked copies: ", linkingCopyMemory);

    dataLogLn("# of unlinked metadata tables created: ", unlinkedMetadataCount);
    dataLogLn("# of which were 32bit: ", size32MetadataCount);
    dataLogLn("# of copies from linking: ", numberOfCopiesFromLinking);
    dataLogLn();

    if (!verbose)
        return;

    dataLogLn("Per opcode statistics:");
    std::array<unsigned, NUMBER_OF_BYTECODE_WITH_METADATA> opcodeIds;
    std::array<size_t, NUMBER_OF_BYTECODE_WITH_METADATA> memoryUsagePerOpcode;
    for (unsigned i = 0; i < NUMBER_OF_BYTECODE_WITH_METADATA; ++i) {
        opcodeIds[i] = i;
        memoryUsagePerOpcode[i] = perOpcodeCount[i] * metadataSize(static_cast<OpcodeID>(i));
    }
    std::sort(opcodeIds.begin(), opcodeIds.end(), [&](auto a, auto b) {
        return memoryUsagePerOpcode[a] > memoryUsagePerOpcode[b];
    });
    for (unsigned i = 0; i < NUMBER_OF_BYTECODE_WITH_METADATA; ++i) {
        auto id = opcodeIds[i];
        auto numberOfEntries = perOpcodeCount[id];
        if (!numberOfEntries)
            continue;
        dataLogLn(opcodeNames[id], ":");
        dataLogLn("\tnumber of entries: ", numberOfEntries);
        dataLogLn("\tmemory usage: ", memoryUsagePerOpcode[id]);
    }
}
#endif

void UnlinkedMetadataTable::finalize()
{
    ASSERT(!m_isFinalized);
    m_isFinalized = true;
    if (!m_hasMetadata) {
        MetadataTableMalloc::free(m_rawBuffer);
        m_rawBuffer = nullptr;
        return;
    }

    unsigned offset = s_offset16TableSize;
    {
        Offset32* buffer = preprocessBuffer();
        for (unsigned i = 0; i < s_offsetTableEntries - 1; i++) {
            unsigned numberOfEntries = buffer[i];
            if (!numberOfEntries) {
                buffer[i] = offset;
                continue;
            }
            buffer[i] = offset; // We align when we access this.
            unsigned alignment = metadataAlignment(static_cast<OpcodeID>(i));
            ASSERT(alignment <= s_maxMetadataAlignment);

#if CPU(ADDRESS64)
            // This is only necessary for the first metadata entry, if the buffer
            // is 4-byte aligned and the entry has an alignment requirement of 8
            ASSERT(offset == roundUpToMultipleOf(alignment, offset) || offset == s_offset16TableSize);
#endif
            offset = roundUpToMultipleOf(alignment, offset);

            offset += numberOfEntries * metadataSize(static_cast<OpcodeID>(i));
#if ENABLE(METADATA_STATISTICS)
            MetadataStatistics::perOpcodeCount[i] += numberOfEntries;
#endif
        }
        buffer[s_offsetTableEntries - 1] = offset;
        m_is32Bit = offset > UINT16_MAX;
    }

#if ENABLE(METADATA_STATISTICS)
    MetadataStatistics::unlinkedMetadataCount++;
    if (m_is32Bit)
        MetadataStatistics::size32MetadataCount++;
    MetadataStatistics::totalMemory += offset;
    static std::once_flag once;
    std::call_once(once, [] {
        std::atexit(MetadataStatistics::reportMetadataStatistics);
    });
#endif

    unsigned valueProfileSize = m_numValueProfiles * sizeof(ValueProfile);
    if (m_is32Bit) {
        // offset already accounts for s_offset16TableSize
        uint8_t* newBuffer = reinterpret_cast_ptr<uint8_t*>(MetadataTableMalloc::malloc(valueProfileSize + sizeof(LinkingData) + s_offset32TableSize + offset));
        memset(newBuffer, 0, valueProfileSize + sizeof(LinkingData) + s_offset16TableSize);
        memset(newBuffer + valueProfileSize + sizeof(LinkingData) + s_offset16TableSize + s_offset32TableSize, 0, offset - s_offset16TableSize);
        Offset32* buffer = bitwise_cast<Offset32*>(newBuffer + valueProfileSize + sizeof(LinkingData) + s_offset16TableSize);
        for (unsigned i = 0; i < s_offsetTableEntries; ++i)
            buffer[i] = preprocessBuffer()[i] + s_offset32TableSize;
        MetadataTableMalloc::free(m_rawBuffer);
        m_rawBuffer = newBuffer;
    } else {
        // offset already accounts for s_offset16TableSize
        uint8_t* newBuffer = reinterpret_cast_ptr<uint8_t*>(MetadataTableMalloc::malloc(valueProfileSize + sizeof(LinkingData) + offset));
        memset(newBuffer, 0, valueProfileSize + sizeof(LinkingData));
        memset(newBuffer + valueProfileSize + sizeof(LinkingData) + s_offset16TableSize, 0, offset - s_offset16TableSize);
        Offset16* buffer = bitwise_cast<Offset16*>(newBuffer + valueProfileSize + sizeof(LinkingData));
        for (unsigned i = 0; i < s_offsetTableEntries; ++i)
            buffer[i] = preprocessBuffer()[i];
        MetadataTableMalloc::free(m_rawBuffer);
        m_rawBuffer = newBuffer;
    }
}

} // namespace JSC
