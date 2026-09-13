/*
* Copyright (C) 2010 Google Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
*     * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Hasher.h>
#include <wtf/HexNumber.h>
#include <wtf/Int128.h>
#include <wtf/SHA1.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringConcatenate.h>
#include <wtf/text/WTFString.h>

#ifdef __OBJC__
@class NSUUID;
#endif

namespace WTF {

class StringView;

class UUID {
WTF_DEPRECATED_MAKE_FAST_ALLOCATED(UUID);
public:
    static UUID createVersion4()
    {
        return UUID { };
    }

    static UUID createVersion4Weak()
    {
        return UUID { generateWeakRandomUUIDVersion4() };
    }

    WTF_EXPORT_PRIVATE static UUID NODELETE createVersion5(const SHA1::Digest&);
    WTF_EXPORT_PRIVATE static UUID createVersion5(UUID, std::span<const uint8_t>);

#ifdef __OBJC__
    WTF_EXPORT_PRIVATE RetainPtr<NSUUID> createNSUUID() const;
    WTF_EXPORT_PRIVATE static std::optional<UUID> fromNSUUID(NSUUID *);
#endif

    WTF_EXPORT_PRIVATE static std::optional<UUID> parse(StringView);
    WTF_EXPORT_PRIVATE static std::optional<UUID> parseVersion4(StringView);

    static std::optional<UUID> tryCreate(std::span<const uint8_t> span)
    {
        if (span.size() != 16)
            return std::nullopt;
        UUID uuid { span.first<16>() };
        if (!uuid.isValid())
            return std::nullopt;
        return uuid;
    }

    // Used by the generated IPC decoder, see WTFArgumentCoders.serialization.in.
    static std::optional<UUID> tryCreate(uint64_t high, uint64_t low)
    {
        auto data = (static_cast<UInt128>(high) << 64) | low;
        if (data == emptyValue || data == deletedValue)
            return std::nullopt;
        return UUID { data };
    }

    // For hardcoded UUID constants. consteval, so a reserved value is a build error rather than a
    // crash, and raw values still have no way in at runtime other than the fallible factories.
    static consteval UUID createConstant(uint64_t high, uint64_t low)
    {
        return UUID { (static_cast<UInt128>(high) << 64) | low };
    }

    std::span<const uint8_t, 16> span() const LIFETIME_BOUND
    {
        return asByteSpan<UInt128, 16>(m_data);
    }

    friend bool operator==(const UUID&, const UUID&) = default;

    // Public so that composite types can build their own hash traits. Unlike the empty value, the deleted
    // value is not a usable "no UUID" sentinel, since Markable and HashTraits both treat it as engaged.
    explicit constexpr UUID(HashTableDeletedValueType)
        : m_data(deletedValue)
    {
    }

    constexpr bool isHashTableDeletedValue() const { return m_data == deletedValue; }
    constexpr bool isHashTableEmptyValue() const { return m_data == emptyValue; }
    static constexpr bool safeToCompareToHashTableEmptyOrDeletedValue = true;
    WTF_EXPORT_PRIVATE String toString() const;

    constexpr operator bool() const { return !!m_data; }
    bool isValid() const { return m_data != emptyValue && m_data != deletedValue; }

    UInt128 data() const { return m_data; }

    uint64_t low() const { return static_cast<uint64_t>(m_data); }
    uint64_t high() const { return static_cast<uint64_t>(m_data >> 64);  }

private:
    friend struct HashTraits<UUID>;
    friend struct MarkableTraits<UUID>;
    friend void add(Hasher&, UUID);

    // The empty and deleted values are reserved for HashTraits and MarkableTraits. Code that needs to express
    // "no UUID" should use Markable<WTF::UUID> or std::optional<WTF::UUID> rather than naming these.
    static constexpr UInt128 emptyValue = 0;
    static constexpr UInt128 deletedValue = 1;

    // Private so that raw bytes can only enter through tryCreate(), which rejects the reserved values.
    explicit UUID(std::span<const uint8_t, 16> span)
    {
        memcpySpan(asMutableByteSpan(m_data), span);
    }

    explicit constexpr UUID(UInt128 data)
        : m_data(data)
    {
        RELEASE_ASSERT(data != emptyValue && data != deletedValue);
    }

    explicit constexpr UUID(HashTableEmptyValueType)
        : m_data(emptyValue)
    {
    }

    WTF_EXPORT_PRIVATE UUID();

    WTF_EXPORT_PRIVATE static UInt128 generateWeakRandomUUIDVersion4();

    UInt128 m_data;
};

template<>
struct MarkableTraits<UUID> {
    static bool isEmptyValue(const UUID& uuid) { return !uuid; }
    static UUID emptyValue() { return UUID { HashTableEmptyValue }; }
};

inline void add(Hasher& hasher, UUID uuid)
{
    add(hasher, uuid.m_data);
}

template<> struct HashTraits<UUID> : GenericHashTraits<UUID> {
    static UUID emptyValue() { return UUID { HashTableEmptyValue }; }
    static bool isEmptyValue(const UUID& value) { return value.isHashTableEmptyValue(); }
    static void constructDeletedValue(UUID& slot) { slot = UUID { HashTableDeletedValue }; }
    static bool isDeletedValue(const UUID& value) { return value.isHashTableDeletedValue(); }
};

// Creates a UUID that consists of 32 hexadecimal digits and returns its canonical form.
// The canonical form is displayed in 5 groups separated by hyphens, in the form 8-4-4-4-12 for a total of 36 characters.
// The hexadecimal values "a" through "f" are output as lower case characters.
//
// Note: for security reason, we should always generate version 4 UUID that use a scheme relying only on random numbers.
// This algorithm sets the version number as well as two reserved bits. All other bits are set using a random or pseudorandom
// data source. Version 4 UUIDs have the form xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx with hexadecimal digits for x and one of 8,
// 9, A, or B for y.

WTF_EXPORT_PRIVATE String createVersion4UUIDString();
WTF_EXPORT_PRIVATE String createVersion4UUIDStringWeak();

WTF_EXPORT_PRIVATE String bootSessionUUIDString();
WTF_EXPORT_PRIVATE bool isVersion4UUID(StringView);

// 128 bits rendered in the canonical UUID form, for bits that are UUID-shaped but cannot be held in a
// UUID because they may be a reserved value, such as a FIDO AAGUID: 16 arbitrary vendor bytes, and
// commonly all-zero. StringTypeAdapter<UUID> is this with the bits taken from a UUID.
struct UUIDCanonicalForm {
    uint64_t high { 0 };
    uint64_t low { 0 };
};

template<>
class StringTypeAdapter<UUIDCanonicalForm> {
public:
    StringTypeAdapter(UUIDCanonicalForm bits)
        : m_bits { bits }
    {
    }

    template<typename Func>
    auto handle(Func&& func) const -> decltype(auto)
    {
        return handleWithAdapters(std::forward<Func>(func),
            hex(m_bits.high >> 32, 8, Lowercase),
            '-',
            hex((m_bits.high >> 16) & 0xffff, 4, Lowercase),
            '-',
            hex(m_bits.high & 0xffff, 4, Lowercase),
            '-',
            hex(m_bits.low >> 48, 4, Lowercase),
            '-',
            hex(m_bits.low & 0xffffffffffff, 12, Lowercase));
    }

    unsigned length() const
    {
        return handle([](auto&&... adapters) -> unsigned {
            auto sum = checkedSum<int32_t>(adapters.length()...);
            if (sum.hasOverflowed())
                return UINT_MAX;
            return sum;
        });
    }

    bool is8Bit() const { return true; }

    template<typename CharacterType>
    void writeTo(std::span<CharacterType> destination) const
    {
        handle([&](auto&&... adapters) {
            stringTypeAdapterAccumulator(destination, std::forward<decltype(adapters)>(adapters)...);
        });
    }

private:
    UUIDCanonicalForm m_bits;
};

template<>
class StringTypeAdapter<UUID> : public StringTypeAdapter<UUIDCanonicalForm> {
public:
    StringTypeAdapter(UUID uuid)
        : StringTypeAdapter<UUIDCanonicalForm> { { uuid.high(), uuid.low() } }
    {
    }
};

}

using WTF::createVersion4UUIDString;
using WTF::createVersion4UUIDStringWeak;
using WTF::bootSessionUUIDString;
