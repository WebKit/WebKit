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

namespace JSC {

class MetadataTable;

class UnlinkedMetadataTable {
    friend class LLIntOffsetsExtractor;
    friend class MetadataTable;
    friend class CachedMetadataTable;

public:
    struct LinkingData {
        UnlinkedMetadataTable* unlinkedMetadata;
        unsigned refCount;
    };

    UnlinkedMetadataTable();
    ~UnlinkedMetadataTable();

    unsigned addEntry(OpcodeID);

    size_t sizeInBytes();

    void finalize();

    RefPtr<MetadataTable> link();

private:
    void unlink(MetadataTable&);

    size_t sizeInBytes(MetadataTable&);

    using Offset = unsigned;

    static constexpr unsigned s_offsetTableEntries = NUMBER_OF_BYTECODE_WITH_METADATA + 1; // one extra entry for the "end" offset;
    static constexpr unsigned s_offsetTableSize = s_offsetTableEntries * sizeof(UnlinkedMetadataTable::Offset);

    Offset* buffer() const { return bitwise_cast<Offset*>(bitwise_cast<uint8_t*>(m_rawBuffer) + sizeof(LinkingData)); }

    bool m_hasMetadata : 1;
    bool m_isFinalized : 1;
    bool m_isLinked : 1;
    void* m_rawBuffer;
};

} // namespace JSC
