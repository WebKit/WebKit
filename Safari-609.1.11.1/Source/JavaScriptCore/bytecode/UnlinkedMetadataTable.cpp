/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "CodeBlock.h"
#include "JSCInlines.h"
#include "OpcodeInlines.h"
#include "UnlinkedMetadataTableInlines.h"
#include <wtf/FastMalloc.h>

namespace JSC {

#define JSC_ALIGNMENT_CHECK(size) static_assert(size <= UnlinkedMetadataTable::s_maxMetadataAlignment);
FOR_EACH_BYTECODE_METADATA_ALIGNMENT(JSC_ALIGNMENT_CHECK)
#undef JSC_ALIGNMENT_CHECK

void UnlinkedMetadataTable::finalize()
{
    ASSERT(!m_isFinalized);
    m_isFinalized = true;
    if (!m_hasMetadata) {
        fastFree(m_rawBuffer);
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
            unsigned alignment = metadataAlignment(static_cast<OpcodeID>(i));
            offset = roundUpToMultipleOf(alignment, offset);
            ASSERT(alignment <= s_maxMetadataAlignment);
            buffer[i] = offset;
            offset += numberOfEntries * metadataSize(static_cast<OpcodeID>(i));
        }
        buffer[s_offsetTableEntries - 1] = offset;
        m_is32Bit = offset > UINT16_MAX;
    }

    if (m_is32Bit) {
        m_rawBuffer = reinterpret_cast<uint8_t*>(fastRealloc(m_rawBuffer, s_offset16TableSize + s_offset32TableSize + sizeof(LinkingData)));
        memmove(m_rawBuffer + sizeof(LinkingData) + s_offset16TableSize, m_rawBuffer + sizeof(LinkingData), s_offset32TableSize);
        memset(m_rawBuffer + sizeof(LinkingData), 0, s_offset16TableSize);
        Offset32* buffer = bitwise_cast<Offset32*>(m_rawBuffer + sizeof(LinkingData) + s_offset16TableSize);
        // This adjustment does not break the alignment calculated for metadata in the above loop so long as s_offset32TableSize is rounded with 8.
        for (unsigned i = 0; i < s_offsetTableEntries; i++)
            buffer[i] += s_offset32TableSize;
    } else {
        Offset32* oldBuffer = bitwise_cast<Offset32*>(m_rawBuffer + sizeof(LinkingData));
        Offset16* buffer = bitwise_cast<Offset16*>(m_rawBuffer + sizeof(LinkingData));
        for (unsigned i = 0; i < s_offsetTableEntries; i++)
            buffer[i] = oldBuffer[i];
        m_rawBuffer = static_cast<uint8_t*>(fastRealloc(m_rawBuffer, s_offset16TableSize + sizeof(LinkingData)));
    }
}

} // namespace JSC
