/*
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/JSCConfig.h>
#include <JavaScriptCore/MarkedBlock.h>
#include <compare>
#include <wtf/HashTraits.h>
#include <wtf/StdIntExtras.h>

namespace JSC {

class Structure;

#if CPU(ADDRESS64)

#if defined(STRUCTURE_HEAP_ADDRESS_SIZE_IN_MB) && STRUCTURE_HEAP_ADDRESS_SIZE_IN_MB > 0
constexpr uintptr_t structureHeapAddressSize = STRUCTURE_HEAP_ADDRESS_SIZE_IN_MB * MB;
#elif PLATFORM(PLAYSTATION) || OS(QNX)
constexpr uintptr_t structureHeapAddressSize = 128 * MB;
#elif (PLATFORM(IOS_FAMILY) && !CPU(ARM64E)) || PLATFORM(WATCHOS) || PLATFORM(APPLETV)
constexpr uintptr_t structureHeapAddressSize = 512 * MB;
#elif PLATFORM(IOS_FAMILY)
constexpr uintptr_t structureHeapAddressSize = 2 * GB;
#else
constexpr uintptr_t structureHeapAddressSize = 4 * GB;
#endif

#endif // CPU(ADDRESS64)

class StructureID {
public:
    static constexpr uint32_t nukedStructureIDBit = 1;

#if CPU(ADDRESS64)
    static constexpr uintptr_t structureIDMask = static_cast<uintptr_t>(UINT_MAX);
    static_assert(structureHeapAddressSize - 1 <= structureIDMask, "StructureID relies on only the lower 32 bits of Structure addresses varying");
#endif

    constexpr StructureID() = default;
    constexpr StructureID(StructureID const&) = default;
    constexpr StructureID& operator=(StructureID const&) = default;

    StructureID nuke() const { return StructureID(m_bits | nukedStructureIDBit); }
    bool isNuked() const { return m_bits & nukedStructureIDBit; }
    StructureID decontaminate() const { return StructureID(m_bits & ~nukedStructureIDBit); }

    inline Structure* decode() const;
    inline Structure* tryDecode() const;
    static StructureID encode(const Structure*);

    explicit operator bool() const { return !!m_bits; }
    friend auto operator<=>(const StructureID&, const StructureID&) = default;
    constexpr uint32_t bits() const { return m_bits; }

    constexpr StructureID(WTF::HashTableDeletedValueType) : m_bits(nukedStructureIDBit) { }
    bool isHashTableDeletedValue() const { return *this == StructureID(WTF::HashTableDeletedValue); }

private:
    explicit constexpr StructureID(uint32_t bits) : m_bits(bits) { }

    uint32_t m_bits { 0 };
};
static_assert(sizeof(StructureID) == sizeof(uint32_t));

#if CPU(ADDRESS64)

ALWAYS_INLINE Structure* StructureID::decode() const
{
    // Take care to only use the bits from m_bits in the structure's address reservation.
    ASSERT(decontaminate());
    return reinterpret_cast<Structure*>(static_cast<uintptr_t>(decontaminate().m_bits) + structureIDBase());
}

ALWAYS_INLINE Structure* StructureID::tryDecode() const
{
    // Take care to only use the bits from m_bits in the structure's address reservation.
    SUPPRESS_UNRETAINED_LOCAL Structure* structure = decode();
    uintptr_t offset = std::bit_cast<uintptr_t>(structure) - startOfStructureHeap();
    if (offset < MarkedBlock::blockSize || offset >= g_jscConfig.sizeOfStructureHeap)
        return nullptr;
    return structure;
}

ALWAYS_INLINE StructureID StructureID::encode(const Structure* structure)
{
    ASSERT(structure);
    ASSERT(startOfStructureHeap() <= reinterpret_cast<uintptr_t>(structure) && reinterpret_cast<uintptr_t>(structure) < startOfStructureHeap() + structureHeapAddressSize);
    auto result = StructureID(reinterpret_cast<uintptr_t>(structure));
    ASSERT(result.decode() == structure);
    return result;
}

#else // CPU(ADDRESS64)

ALWAYS_INLINE Structure* StructureID::decode() const
{
    ASSERT(decontaminate());
    return reinterpret_cast<Structure*>(decontaminate().m_bits);
}

ALWAYS_INLINE Structure* StructureID::tryDecode() const
{
    return reinterpret_cast<Structure*>(decontaminate().m_bits);
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
