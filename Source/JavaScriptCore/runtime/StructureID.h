/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

#include "JSCConfig.h"
#include <wtf/HashTraits.h>
#include <wtf/StdIntExtras.h>

namespace JSC {

class Structure;

constexpr CPURegister structureIDMask = structureHeapAddressSize - 1;

class StructureID {
public:
    static constexpr uint32_t nukedStructureIDBit = 1;

    StructureID() = default;
    StructureID(StructureID const&) = default;
    StructureID& operator=(StructureID const&) = default;

    StructureID nuke() const { return StructureID(m_bits | nukedStructureIDBit); }
    bool isNuked() const { return m_bits & nukedStructureIDBit; }
    StructureID decontaminate() const { return StructureID(m_bits & ~nukedStructureIDBit); }

    inline Structure* decode() const;
    static StructureID encode(const Structure*);

    explicit operator bool() const { return !!m_bits; }
    bool operator==(StructureID const& other) const  { return m_bits == other.m_bits; }
    bool operator!=(StructureID const& other) const  { return m_bits != other.m_bits; }
    constexpr uint32_t bits() const { return m_bits; }

    StructureID(WTF::HashTableDeletedValueType) : m_bits(nukedStructureIDBit) { }
    bool isHashTableDeletedValue() const { return *this == StructureID(WTF::HashTableDeletedValue); }

private:
    explicit StructureID(uint32_t bits) : m_bits(bits) { }

    uint32_t m_bits { 0 };
};
static_assert(sizeof(StructureID) == sizeof(uint32_t));

#if CPU(ADDRESS64)

ALWAYS_INLINE Structure* StructureID::decode() const
{
    // Take care to only use the bits from m_bits in the structure's address reservation.
    ASSERT(decontaminate());
    return reinterpret_cast<Structure*>((static_cast<uintptr_t>(decontaminate().m_bits) & structureIDMask) + g_jscConfig.startOfStructureHeap);
}

ALWAYS_INLINE StructureID StructureID::encode(const Structure* structure)
{
    ASSERT(structure);
    ASSERT(g_jscConfig.startOfStructureHeap <= reinterpret_cast<uintptr_t>(structure) && reinterpret_cast<uintptr_t>(structure) < g_jscConfig.startOfStructureHeap + structureHeapAddressSize);
    auto result = StructureID(reinterpret_cast<uintptr_t>(structure) & structureIDMask);
    ASSERT(result.decode() == structure);
    return result;
}

#else // CPU(ADDRESS64)

ALWAYS_INLINE Structure* StructureID::decode() const
{
    ASSERT(decontaminate());
    return reinterpret_cast<Structure*>(m_bits);
}

ALWAYS_INLINE StructureID StructureID::encode(const Structure* structure)
{
    ASSERT(structure);
    return StructureID(reinterpret_cast<uint32_t>(structure));
}

#endif

struct StructureIDHash {
    static unsigned hash(const StructureID& key) { return key.bits(); }
    static bool equal(const StructureID& a, const StructureID& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

} // namespace JSC

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::StructureID> : JSC::StructureIDHash { };

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::StructureID> : SimpleClassHashTraits<JSC::StructureID> {
    static constexpr bool emptyValueIsZero = true;
};

}
