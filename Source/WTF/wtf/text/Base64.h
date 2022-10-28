/*
 * Copyright (C) 2006 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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

#include <wtf/Forward.h>
#include <wtf/OptionSet.h>
#include <wtf/text/StringConcatenate.h>
#include <wtf/text/StringView.h>

namespace WTF {

enum class Base64EncodePolicy {
    DoNotInsertLFs,
    InsertLFs,
    URL // No padding, no LFs.
};

enum class Base64EncodeMap { Default, URL };

enum class Base64DecodeOptions : uint8_t {
    ValidatePadding         = 1 << 0,
    IgnoreSpacesAndNewLines = 1 << 1,
    DiscardVerticalTab      = 1 << 2,
};

enum class Base64DecodeMap { Default, URL };

struct Base64Specification {
    Span<const std::byte> input;
    Base64EncodePolicy policy;
    Base64EncodeMap map;
};

static constexpr unsigned maximumBase64LineLengthWhenInsertingLFs = 76;

// If the input string is pathologically large, just return nothing.
// Rather than being perfectly precise, this is a bit conservative.
static constexpr unsigned maximumBase64EncoderInputBufferSize = std::numeric_limits<unsigned>::max() / 77 * 76 / 4 * 3 - 2;

unsigned calculateBase64EncodedSize(unsigned inputLength, Base64EncodePolicy);

template<typename CharacterType> bool isBase64OrBase64URLCharacter(CharacterType);

WTF_EXPORT_PRIVATE void base64Encode(Span<const std::byte>, Span<UChar>, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);
WTF_EXPORT_PRIVATE void base64Encode(Span<const std::byte>, Span<LChar>, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);

WTF_EXPORT_PRIVATE Vector<uint8_t> base64EncodeToVector(Span<const std::byte>, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);
Vector<uint8_t> base64EncodeToVector(Span<const uint8_t>, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);
Vector<uint8_t> base64EncodeToVector(Span<const char>, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);
Vector<uint8_t> base64EncodeToVector(const CString&, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);
Vector<uint8_t> base64EncodeToVector(const void*, unsigned, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);

WTF_EXPORT_PRIVATE String base64EncodeToString(Span<const std::byte>, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);
String base64EncodeToString(Span<const uint8_t>, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);
String base64EncodeToString(Span<const char>, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);
String base64EncodeToString(const CString&, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);
String base64EncodeToString(const void*, unsigned, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);

WTF_EXPORT_PRIVATE std::optional<Vector<uint8_t>> base64Decode(Span<const std::byte>, OptionSet<Base64DecodeOptions> = { }, Base64DecodeMap = Base64DecodeMap::Default);
WTF_EXPORT_PRIVATE std::optional<Vector<uint8_t>> base64Decode(StringView, OptionSet<Base64DecodeOptions> = { }, Base64DecodeMap = Base64DecodeMap::Default);
std::optional<Vector<uint8_t>> base64Decode(Span<const uint8_t>, OptionSet<Base64DecodeOptions> = { }, Base64DecodeMap = Base64DecodeMap::Default);
std::optional<Vector<uint8_t>> base64Decode(Span<const char>, OptionSet<Base64DecodeOptions> = { }, Base64DecodeMap = Base64DecodeMap::Default);
std::optional<Vector<uint8_t>> base64Decode(const void*, unsigned, OptionSet<Base64DecodeOptions> = { }, Base64DecodeMap = Base64DecodeMap::Default);

// All the same functions modified for base64url, as defined in RFC 4648.
// This format uses '-' and '_' instead of '+' and '/' respectively.

Vector<uint8_t> base64URLEncodeToVector(Span<const std::byte>);
Vector<uint8_t> base64URLEncodeToVector(Span<const uint8_t>);
Vector<uint8_t> base64URLEncodeToVector(Span<const char>);
Vector<uint8_t> base64URLEncodeToVector(const CString&);
Vector<uint8_t> base64URLEncodeToVector(const void*, unsigned);

String base64URLEncodeToString(Span<const std::byte>);
String base64URLEncodeToString(Span<const uint8_t>);
String base64URLEncodeToString(Span<const char>);
String base64URLEncodeToString(const CString&);
String base64URLEncodeToString(const void*, unsigned);

std::optional<Vector<uint8_t>> base64URLDecode(StringView);
inline std::optional<Vector<uint8_t>> base64URLDecode(ASCIILiteral literal) { return base64URLDecode(StringView { literal }); }
std::optional<Vector<uint8_t>> base64URLDecode(Span<const std::byte>);
std::optional<Vector<uint8_t>> base64URLDecode(Span<const uint8_t>);
std::optional<Vector<uint8_t>> base64URLDecode(Span<const char>);
std::optional<Vector<uint8_t>> base64URLDecode(const void*, unsigned);

// Versions for use with StringBuilder / makeString.

Base64Specification base64Encoded(Span<const std::byte>, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);
Base64Specification base64Encoded(Span<const uint8_t>, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);
Base64Specification base64Encoded(const CString&, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);
Base64Specification base64Encoded(const void*, unsigned, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);

Base64Specification base64URLEncoded(Span<const std::byte>);
Base64Specification base64URLEncoded(Span<const uint8_t>);
Base64Specification base64URLEncoded(const CString&);
Base64Specification base64URLEncoded(const void*, unsigned);


inline Vector<uint8_t> base64EncodeToVector(Span<const uint8_t> input, Base64EncodePolicy policy, Base64EncodeMap map)
{
    return base64EncodeToVector(asBytes(input), policy, map);
}

inline Vector<uint8_t> base64EncodeToVector(Span<const char> input, Base64EncodePolicy policy, Base64EncodeMap map)
{
    return base64EncodeToVector(asBytes(input), policy, map);
}

inline Vector<uint8_t> base64EncodeToVector(const CString& input, Base64EncodePolicy policy, Base64EncodeMap map)
{
    return base64EncodeToVector(input.bytes(), policy, map);
}

inline Vector<uint8_t> base64EncodeToVector(const void* input, unsigned length, Base64EncodePolicy policy, Base64EncodeMap map)
{
    return base64EncodeToVector({ static_cast<const std::byte*>(input), length }, policy, map);
}

inline String base64EncodeToString(Span<const uint8_t> input, Base64EncodePolicy policy, Base64EncodeMap map)
{
    return base64EncodeToString(asBytes(input), policy, map);
}

inline String base64EncodeToString(Span<const char> input, Base64EncodePolicy policy, Base64EncodeMap map)
{
    return base64EncodeToString(asBytes(input), policy, map);
}

inline String base64EncodeToString(const CString& input, Base64EncodePolicy policy, Base64EncodeMap map)
{
    return base64EncodeToString(input.bytes(), policy, map);
}

inline String base64EncodeToString(const void* input, unsigned length, Base64EncodePolicy policy, Base64EncodeMap map)
{
    return base64EncodeToString({ static_cast<const std::byte*>(input), length }, policy, map);
}

inline std::optional<Vector<uint8_t>> base64Decode(Span<const uint8_t> input, OptionSet<Base64DecodeOptions> options, Base64DecodeMap map)
{
    return base64Decode(asBytes(input), options, map);
}

inline std::optional<Vector<uint8_t>> base64Decode(Span<const char> input, OptionSet<Base64DecodeOptions> options, Base64DecodeMap map)
{
    return base64Decode(asBytes(input), options, map);
}

inline std::optional<Vector<uint8_t>> base64Decode(const void* input, unsigned length, OptionSet<Base64DecodeOptions> options, Base64DecodeMap map)
{
    return base64Decode({ static_cast<const std::byte*>(input), length }, options, map);
}

inline Vector<uint8_t> base64URLEncodeToVector(Span<const std::byte> input)
{
    return base64EncodeToVector(input, Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

inline Vector<uint8_t> base64URLEncodeToVector(Span<const uint8_t> input)
{
    return base64EncodeToVector(input, Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

inline Vector<uint8_t> base64URLEncodeToVector(Span<const char> input)
{
    return base64EncodeToVector(input, Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

inline Vector<uint8_t> base64URLEncodeToVector(const CString& input)
{
    return base64EncodeToVector(input, Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

inline Vector<uint8_t> base64URLEncodeToVector(const void* input, unsigned length)
{
    return base64EncodeToVector(input, length, Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

inline String base64URLEncodeToString(Span<const std::byte> input)
{
    return base64EncodeToString(input, Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

inline String base64URLEncodeToString(Span<const uint8_t> input)
{
    return base64EncodeToString(input, Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

inline String base64URLEncodeToString(Span<const char> input)
{
    return base64EncodeToString(input, Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

inline String base64URLEncodeToString(const CString& input)
{
    return base64EncodeToString(input, Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

inline String base64URLEncodeToString(const void* input, unsigned length)
{
    return base64EncodeToString(input, length, Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

inline std::optional<Vector<uint8_t>> base64URLDecode(StringView input)
{
    return base64Decode(input, { }, Base64DecodeMap::URL);
}

inline std::optional<Vector<uint8_t>> base64URLDecode(Span<const std::byte> input)
{
    return base64Decode(input, { }, Base64DecodeMap::URL);
}

inline std::optional<Vector<uint8_t>> base64URLDecode(Span<const uint8_t> input)
{
    return base64Decode(input, { }, Base64DecodeMap::URL);
}

inline std::optional<Vector<uint8_t>> base64URLDecode(Span<const char> input)
{
    return base64Decode(input, { }, Base64DecodeMap::URL);
}

inline std::optional<Vector<uint8_t>> base64URLDecode(const void* input, unsigned length)
{
    return base64Decode(input, length, { }, Base64DecodeMap::URL);
}

template<typename CharacterType> bool isBase64OrBase64URLCharacter(CharacterType c)
{
    return isASCIIAlphanumeric(c) || c == '+' || c == '/' || c == '-' || c == '_';
}

inline unsigned calculateBase64EncodedSize(unsigned inputLength, Base64EncodePolicy policy)
{
    if (!inputLength)
        return 0;

    if (inputLength > maximumBase64EncoderInputBufferSize)
        return 0;

    auto basePaddedEncodedSize = [&] {
        return ((inputLength + 2) / 3) * 4;
    };

    auto baseUnpaddedEncodedSize = [&] {
        return ((inputLength * 4) + 2) / 3;
    };

    switch (policy) {
    case Base64EncodePolicy::DoNotInsertLFs:
        return basePaddedEncodedSize();

    case Base64EncodePolicy::InsertLFs: {
        auto result = basePaddedEncodedSize();
        return result + ((result - 1) / maximumBase64LineLengthWhenInsertingLFs);
    }

    case Base64EncodePolicy::URL:
        return baseUnpaddedEncodedSize();
    }

    ASSERT_NOT_REACHED();
    return 0;
}

inline Base64Specification base64Encoded(Span<const std::byte> input, Base64EncodePolicy policy, Base64EncodeMap map)
{
    return { input, policy, map };
}

inline Base64Specification base64Encoded(Span<const uint8_t> input, Base64EncodePolicy policy, Base64EncodeMap map)
{
    return base64Encoded(asBytes(input), policy, map);
}

inline Base64Specification base64Encoded(const CString& input, Base64EncodePolicy policy, Base64EncodeMap map)
{
    return base64Encoded(input.bytes(), policy, map);
}

inline Base64Specification base64Encoded(const void* input, unsigned length, Base64EncodePolicy policy, Base64EncodeMap map)
{
    return base64Encoded({ static_cast<const std::byte*>(input), length }, policy, map);
}

inline Base64Specification base64URLEncoded(Span<const std::byte> input)
{
    return base64Encoded(input, Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

inline Base64Specification base64URLEncoded(Span<const uint8_t> input)
{
    return base64Encoded(input, Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

inline Base64Specification base64URLEncoded(const CString& input)
{
    return base64Encoded(input, Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

inline Base64Specification base64URLEncoded(const void* input, unsigned length)
{
    return base64Encoded(input, length, Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

template<> class StringTypeAdapter<Base64Specification> {
public:
    StringTypeAdapter(const Base64Specification& base64)
        : m_base64 { base64 }
        , m_encodedLength { calculateBase64EncodedSize(base64.input.size(), base64.policy) }
    {
    }

    unsigned length() const { return m_encodedLength; }
    bool is8Bit() const { return true; }

    template<typename CharacterType> void writeTo(CharacterType* destination) const
    {
        base64Encode(m_base64.input, Span { destination, m_encodedLength }, m_base64.policy, m_base64.map);
    }

private:
    Base64Specification m_base64;
    unsigned m_encodedLength;
};

} // namespace WTF

using WTF::Base64DecodeOptions;
using WTF::Base64EncodePolicy;
using WTF::base64Decode;
using WTF::base64EncodeToString;
using WTF::base64EncodeToVector;
using WTF::base64Encoded;
using WTF::base64URLDecode;
using WTF::base64URLDecode;
using WTF::base64URLEncodeToString;
using WTF::base64URLEncodeToVector;
using WTF::base64URLEncoded;
using WTF::isBase64OrBase64URLCharacter;
