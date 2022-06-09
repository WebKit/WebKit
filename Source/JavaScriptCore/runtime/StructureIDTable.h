/*
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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

#pragma once

#include "EnsureStillAliveHere.h"
#include "UnusedPointer.h"
#include <wtf/UniqueArray.h>
#include <wtf/Vector.h>
#include <wtf/WeakRandom.h>

namespace JSC {

class Structure;

#if USE(JSVALUE64)
using StructureID = uint32_t;

inline StructureID nukedStructureIDBit()
{
    return 0x80000000u;
}

inline StructureID nuke(StructureID id)
{
    return id | nukedStructureIDBit();
}

inline bool isNuked(StructureID id)
{
    return !!(id & nukedStructureIDBit());
}

inline StructureID decontaminate(StructureID id)
{
    return id & ~nukedStructureIDBit();
}
#else // not USE(JSVALUE64)
using StructureID = Structure*;

inline StructureID nukedStructureIDBit()
{
    return bitwise_cast<StructureID>(static_cast<uintptr_t>(1));
}

inline StructureID nuke(StructureID id)
{
    return bitwise_cast<StructureID>(bitwise_cast<uintptr_t>(id) | bitwise_cast<uintptr_t>(nukedStructureIDBit()));
}

inline bool isNuked(StructureID id)
{
    return !!(bitwise_cast<uintptr_t>(id) & bitwise_cast<uintptr_t>(nukedStructureIDBit()));
}

inline StructureID decontaminate(StructureID id)
{
    return bitwise_cast<StructureID>(bitwise_cast<uintptr_t>(id) & ~bitwise_cast<uintptr_t>(nukedStructureIDBit()));
}
#endif // not USE(JSVALUE64)

#if USE(JSVALUE64)

using EncodedStructureBits = uintptr_t;

class StructureIDTable {
    friend class LLIntOffsetsExtractor;
public:
    StructureIDTable();

    void** base() { return reinterpret_cast<void**>(&m_table); }

    ALWAYS_INLINE void validate(StructureID);

    // FIXME: rdar://69036888: remove this when no longer needed.
    // This is only used for a special case mitigation. It is not for general use.
    Structure* tryGet(StructureID);

    Structure* get(StructureID);
    void deallocateID(Structure*, StructureID);
    StructureID allocateID(Structure*);

    void flushOldTables();
    
    size_t size() const { return m_size; }

private:
    void resize(size_t newCapacity);
    void makeFreeListFromRange(uint32_t first, uint32_t last);

    union StructureOrOffset {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        EncodedStructureBits encodedStructureBits;
        uintptr_t offset;
    };

    StructureOrOffset* table() const { return m_table.get(); }
    static Structure* decode(EncodedStructureBits, StructureID);
    static EncodedStructureBits encode(Structure*, StructureID);

    static constexpr size_t s_initialSize = 512;

    Vector<UniqueArray<StructureOrOffset>> m_oldTables;

    uint32_t m_firstFreeOffset { 0 };
    uint32_t m_lastFreeOffset { 0 };
    UniqueArray<StructureOrOffset> m_table;

    size_t m_size { 0 };
    size_t m_capacity;

    WeakRandom m_weakRandom;

    static constexpr StructureID s_unusedID = 0;

public:
    // 1. StructureID is encoded as:
    //
    //    ----------------------------------------------------------------
    //    | 1 Nuke Bit | 26 StructureIDTable index bits | 5 entropy bits |
    //    ----------------------------------------------------------------
    //
    //    The entropy bits are chosen at random and assigned when a StructureID
    //    is allocated.
    //
    // 2. For each StructureID, the StructureIDTable stores encodedStructureBits
    //    which are encoded from the structure pointer as such:
    //
    //    ------------------------------------------------------------------
    //    | 11 low index bits | 5 entropy bits | 48 structure pointer bits |
    //    ------------------------------------------------------------------
    //
    //    The entropy bits here are the same 5 bits used in the encoding of the
    //    StructureID for this structure entry in the StructureIDTable.

    static constexpr uint32_t s_numberOfNukeBits = 1;
    static constexpr uint32_t s_numberOfEntropyBits = 5;
    static constexpr uint32_t s_entropyBitsShiftForStructurePointer = (sizeof(EncodedStructureBits) * 8) - 16;

    static constexpr uint32_t s_maximumNumberOfStructures = 1 << (32 - s_numberOfEntropyBits - s_numberOfNukeBits);
};

ALWAYS_INLINE Structure* StructureIDTable::decode(EncodedStructureBits bits, StructureID structureID)
{
    return bitwise_cast<Structure*>(bits ^ (static_cast<uintptr_t>(structureID) << s_entropyBitsShiftForStructurePointer));
}

ALWAYS_INLINE EncodedStructureBits StructureIDTable::encode(Structure* structure, StructureID structureID)
{
    return bitwise_cast<EncodedStructureBits>(structure) ^ (static_cast<EncodedStructureBits>(structureID) << s_entropyBitsShiftForStructurePointer);
}

inline Structure* StructureIDTable::get(StructureID structureID)
{
    ASSERT_WITH_SECURITY_IMPLICATION(structureID);
    ASSERT_WITH_SECURITY_IMPLICATION(!isNuked(structureID));
    uint32_t structureIndex = structureID >> s_numberOfEntropyBits;
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(structureIndex < m_capacity);
    return decode(table()[structureIndex].encodedStructureBits, structureID);
}

// FIXME: rdar://69036888: remove this function when no longer needed.
inline Structure* StructureIDTable::tryGet(StructureID structureID)
{
    uint32_t structureIndex = structureID >> s_numberOfEntropyBits;
    if (structureIndex >= m_capacity)
        return nullptr;
    return decode(table()[structureIndex].encodedStructureBits, structureID);
}

ALWAYS_INLINE void StructureIDTable::validate(StructureID structureID)
{
    uint32_t structureIndex = structureID >> s_numberOfEntropyBits;
    Structure* structure = decode(table()[structureIndex].encodedStructureBits, structureID);
    RELEASE_ASSERT(structureIndex < m_capacity);
    *bitwise_cast<volatile uint64_t*>(structure);
}

#else // not USE(JSVALUE64)

class StructureIDTable {
    friend class LLIntOffsetsExtractor;
public:
    StructureIDTable() = default;

    // FIXME: rdar://69036888: remove this function when no longer needed.
    Structure* tryGet(StructureID structureID) { return structureID; }
    Structure* get(StructureID structureID) { return structureID; }
    void deallocateID(Structure*, StructureID) { }
    StructureID allocateID(Structure* structure)
    {
        ASSERT(!isNuked(structure));
        return structure;
    };

    void flushOldTables() { }
    void validate(StructureID) { }
};

#endif // not USE(JSVALUE64)

} // namespace JSC
