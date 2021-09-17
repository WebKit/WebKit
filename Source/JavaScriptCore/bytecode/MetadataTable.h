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

// MetadataTable has a bit strange memory layout for LLInt optimization.
// [UnlinkedMetadataTable::LinkingData][MetadataTable]
//                                     ^
//                 The pointer of MetadataTable points at this address.
class MetadataTable {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(MetadataTable);
    friend class LLIntOffsetsExtractor;
    friend class UnlinkedMetadataTable;
public:
    ~MetadataTable();

    template<typename Metadata>
    ALWAYS_INLINE Metadata* get()
    {
        auto opcodeID = Metadata::opcodeID;
        ASSERT(opcodeID < NUMBER_OF_BYTECODE_WITH_METADATA);
        uintptr_t ptr = bitwise_cast<uintptr_t>(getWithoutAligning(opcodeID));
        ptr = roundUpToMultipleOf(alignof(Metadata), ptr);
        return bitwise_cast<Metadata*>(ptr);
    }

    template<typename Op, typename Functor>
    ALWAYS_INLINE void forEach(const Functor& func)
    {
        auto* metadata = get<typename Op::Metadata>();
        auto* end = bitwise_cast<typename Op::Metadata*>(getWithoutAligning(Op::opcodeID + 1));
        for (; metadata < end; ++metadata)
            func(*metadata);
    }

    size_t sizeInBytes();

    void ref()
    {
        ++linkingData().refCount;
    }

    void deref()
    {
        unsigned tempRefCount = linkingData().refCount - 1;
        if (!tempRefCount) {
            MetadataTable::destroy(this);
            return;
        }
        linkingData().refCount = tempRefCount;
    }

    unsigned refCount() const
    {
        return linkingData().refCount;
    }

    unsigned hasOneRef() const
    {
        return refCount() == 1;
    }

private:
    MetadataTable(UnlinkedMetadataTable&);

    UnlinkedMetadataTable::Offset16* offsetTable16() const { return bitwise_cast<UnlinkedMetadataTable::Offset16*>(this); }
    UnlinkedMetadataTable::Offset32* offsetTable32() const { return bitwise_cast<UnlinkedMetadataTable::Offset32*>(bitwise_cast<uint8_t*>(this) + UnlinkedMetadataTable::s_offset16TableSize); }

    size_t totalSize() const
    {
        return getOffset(UnlinkedMetadataTable::s_offsetTableEntries - 1);
    }

    UnlinkedMetadataTable::LinkingData& linkingData() const
    {
        return *bitwise_cast<UnlinkedMetadataTable::LinkingData*>((bitwise_cast<uint8_t*>(this) - sizeof(UnlinkedMetadataTable::LinkingData)));
    }

    void* buffer() { return this; }

    bool is32Bit() const { return !offsetTable16()[0]; }

    ALWAYS_INLINE unsigned getOffset(unsigned i) const
    {
        unsigned offset = offsetTable16()[i];
        if (offset)
            return offset;
        return offsetTable32()[i];
    }

    ALWAYS_INLINE uint8_t* getWithoutAligning(unsigned i)
    {
        return bitwise_cast<uint8_t*>(this) + getOffset(i);
    }

    static void destroy(MetadataTable*);
};

} // namespace JSC
