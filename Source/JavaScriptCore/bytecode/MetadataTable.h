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

#include "Instruction.h"
#include "Opcode.h"
#include "UnlinkedMetadataTable.h"
#include <wtf/RefCounted.h>

namespace JSC {

class CodeBlock;

class MetadataTable : public RefCounted<MetadataTable> {
    friend class LLIntOffsetsExtractor;
    friend class UnlinkedMetadataTable;

public:
    ~MetadataTable();

    ALWAYS_INLINE Instruction::Metadata* get(OpcodeID opcodeID)
    {
        ASSERT(m_buffer && opcodeID < NUMBER_OF_BYTECODE_WITH_METADATA);
        return reinterpret_cast<Instruction::Metadata*>(getImpl(opcodeID));
    }


    template<typename Op, typename Functor>
    ALWAYS_INLINE void forEach(const Functor& func)
    {
        if (!m_buffer)
            return;

        auto* metadata = reinterpret_cast<typename Op::Metadata*>(get(Op::opcodeID));
        auto* end = reinterpret_cast<typename Op::Metadata*>(getImpl(Op::opcodeID + 1));
        for (; metadata != end; ++metadata)
            func(*metadata);
    }

    size_t sizeInBytes();

private:
    MetadataTable(UnlinkedMetadataTable&, UnlinkedMetadataTable::Offset*);

    ALWAYS_INLINE uint8_t* getImpl(unsigned i)
    {
        return reinterpret_cast<uint8_t*>(m_buffer) + m_buffer[i];
    }

    UnlinkedMetadataTable::Offset* m_buffer;
    UnlinkedMetadataTable& m_unlinkedMetadata;
};

} // namespace JSC
