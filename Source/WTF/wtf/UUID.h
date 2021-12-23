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

#include <wtf/Int128.h>
#include <wtf/text/WTFString.h>

namespace WTF {

class StringView;

class UUID {
WTF_MAKE_FAST_ALLOCATED;
public:
    static UUID create()
    {
        return UUID { };
    }

    explicit UUID(Span<const uint8_t, 16> span)
    {
        memcpy(&m_data, span.data(), 16);
    }

    explicit UUID(UInt128Impl&& data)
        : m_data(data)
    {
    }

    UUID(const UUID&) = default;

    Span<const uint8_t, 16> toSpan() const
    {
        return Span<const uint8_t, 16> { reinterpret_cast<const uint8_t*>(&m_data), 16 };
    }

    UUID& operator=(const UUID&) = default;
    bool operator==(const UUID& other) const { return m_data == other.m_data; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<UUID> decode(Decoder&);

    explicit UUID(HashTableDeletedValueType)
        : m_data(1)
    {
    }

    explicit UUID(HashTableEmptyValueType)
        : m_data(0)
    {
    }

    bool isHashTableDeletedValue() const { return m_data == 1; }
    WTF_EXPORT_PRIVATE unsigned hash() const;

private:
    WTF_EXPORT_PRIVATE UUID();

    UInt128Impl m_data;
};

struct UUIDHash {
    static unsigned hash(const UUID& key) { return key.hash(); }
    static bool equal(const UUID& a, const UUID& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

template<> struct HashTraits<UUID> : GenericHashTraits<UUID> {
    static UUID emptyValue() { return UUID { HashTableEmptyValue }; }
    static void constructDeletedValue(UUID& slot) { slot = UUID { HashTableDeletedValue }; }
    static bool isDeletedValue(const UUID& value) { return value.isHashTableDeletedValue(); }
};
template<> struct DefaultHash<UUID> : UUIDHash { };

template<class Encoder>
void UUID::encode(Encoder& encoder) const
{
    encoder << UInt128High64(m_data) << UInt128Low64(m_data);
}

template<class Decoder>
std::optional<UUID> UUID::decode(Decoder& decoder)
{
    std::optional<uint64_t> high;
    decoder >> high;
    if (!high)
        return std::nullopt;

    std::optional<uint64_t> low;
    decoder >> low;
    if (!low)
        return std::nullopt;

    return { UUID {
        MakeUInt128(*high, *low),
    } };
}

// Creates a UUID that consists of 32 hexadecimal digits and returns its canonical form.
// The canonical form is displayed in 5 groups separated by hyphens, in the form 8-4-4-4-12 for a total of 36 characters.
// The hexadecimal values "a" through "f" are output as lower case characters.
//
// Note: for security reason, we should always generate version 4 UUID that use a scheme relying only on random numbers.
// This algorithm sets the version number as well as two reserved bits. All other bits are set using a random or pseudorandom
// data source. Version 4 UUIDs have the form xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx with hexadecimal digits for x and one of 8,
// 9, A, or B for y.

WTF_EXPORT_PRIVATE String createCanonicalUUIDString();

WTF_EXPORT_PRIVATE String bootSessionUUIDString();
WTF_EXPORT_PRIVATE bool isVersion4UUID(StringView);

}

using WTF::UUID;
using WTF::createCanonicalUUIDString;
using WTF::bootSessionUUIDString;
