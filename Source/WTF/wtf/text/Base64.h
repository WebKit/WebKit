/*
 * Copyright (C) 2006 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 * Copyright (C) 2013, 2016, 2023 Apple Inc. All rights reserved.
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

enum class Base64EncodeMode : bool { Default, URL };

// - ::DefaultIgnorePadding and ::URL ignore padding-related requirements.
//   Note that no mode enforces "Canonical Encoding" as defined in RFC 4648.
// - ::DefaultValidatePadding and ::DefaultValidatePaddingAndIgnoreWhitespace
//   enforce a correct number of trailing equal signs in the input.
// - ::DefaultValidatePaddingAndIgnoreWhitespace ignores ASCII whitespace in
//   the input. It matches <https://infra.spec.whatwg.org/#forgiving-base64>.
// - ::DefaultIgnoreWhitespaceForQuirk, URL ignores ASCII whitespace in the
//   input but doesn't validate padding. It is currently only used for quirks.
enum class Base64DecodeMode { DefaultIgnorePadding, DefaultValidatePadding, DefaultValidatePaddingAndIgnoreWhitespace, DefaultIgnoreWhitespaceForQuirk, URL };

struct Base64Specification {
    std::span<const std::byte> input;
    Base64EncodeMode mode;
};

// If the input string is pathologically large, just return nothing.
// Rather than being perfectly precise, this is a bit conservative.
static constexpr unsigned maximumBase64EncoderInputBufferSize = std::numeric_limits<unsigned>::max() / 77 * 76 / 4 * 3 - 2;

unsigned calculateBase64EncodedSize(unsigned inputLength, Base64EncodeMode);

template<typename CharacterType> bool isBase64OrBase64URLCharacter(CharacterType);

WTF_EXPORT_PRIVATE void base64Encode(std::span<const std::byte>, std::span<UChar>, Base64EncodeMode = Base64EncodeMode::Default);
WTF_EXPORT_PRIVATE void base64Encode(std::span<const std::byte>, std::span<LChar>, Base64EncodeMode = Base64EncodeMode::Default);

WTF_EXPORT_PRIVATE Vector<uint8_t> base64EncodeToVector(std::span<const std::byte>, Base64EncodeMode = Base64EncodeMode::Default);
Vector<uint8_t> base64EncodeToVector(std::span<const uint8_t>, Base64EncodeMode = Base64EncodeMode::Default);
Vector<uint8_t> base64EncodeToVector(std::span<const char>, Base64EncodeMode = Base64EncodeMode::Default);
Vector<uint8_t> base64EncodeToVector(const CString&, Base64EncodeMode = Base64EncodeMode::Default);
Vector<uint8_t> base64EncodeToVector(const void*, unsigned, Base64EncodeMode = Base64EncodeMode::Default);

WTF_EXPORT_PRIVATE String base64EncodeToString(std::span<const std::byte>, Base64EncodeMode = Base64EncodeMode::Default);
String base64EncodeToString(std::span<const uint8_t>, Base64EncodeMode = Base64EncodeMode::Default);
String base64EncodeToString(std::span<const char>, Base64EncodeMode = Base64EncodeMode::Default);
String base64EncodeToString(const CString&, Base64EncodeMode = Base64EncodeMode::Default);
String base64EncodeToString(const void*, unsigned, Base64EncodeMode = Base64EncodeMode::Default);

WTF_EXPORT_PRIVATE String base64EncodeToStringReturnNullIfOverflow(std::span<const std::byte>, Base64EncodeMode = Base64EncodeMode::Default);
String base64EncodeToStringReturnNullIfOverflow(const CString&, Base64EncodeMode = Base64EncodeMode::Default);

WTF_EXPORT_PRIVATE std::optional<Vector<uint8_t>> base64Decode(std::span<const std::byte>, Base64DecodeMode = Base64DecodeMode::DefaultIgnorePadding);
WTF_EXPORT_PRIVATE std::optional<Vector<uint8_t>> base64Decode(StringView, Base64DecodeMode = Base64DecodeMode::DefaultIgnorePadding);
std::optional<Vector<uint8_t>> base64Decode(std::span<const uint8_t>, Base64DecodeMode = Base64DecodeMode::DefaultIgnorePadding);
std::optional<Vector<uint8_t>> base64Decode(std::span<const char>, Base64DecodeMode = Base64DecodeMode::DefaultIgnorePadding);
std::optional<Vector<uint8_t>> base64Decode(const void*, unsigned, Base64DecodeMode = Base64DecodeMode::DefaultIgnorePadding);

WTF_EXPORT_PRIVATE String base64DecodeToString(StringView, Base64DecodeMode = Base64DecodeMode::DefaultIgnorePadding);

// All the same functions modified for base64url, as defined in RFC 4648.
// This format uses '-' and '_' instead of '+' and '/' respectively.

Vector<uint8_t> base64URLEncodeToVector(std::span<const std::byte>);
Vector<uint8_t> base64URLEncodeToVector(std::span<const uint8_t>);
Vector<uint8_t> base64URLEncodeToVector(std::span<const char>);
Vector<uint8_t> base64URLEncodeToVector(const CString&);
Vector<uint8_t> base64URLEncodeToVector(const void*, unsigned);

String base64URLEncodeToString(std::span<const std::byte>);
String base64URLEncodeToString(std::span<const uint8_t>);
String base64URLEncodeToString(std::span<const char>);
String base64URLEncodeToString(const CString&);
String base64URLEncodeToString(const void*, unsigned);

std::optional<Vector<uint8_t>> base64URLDecode(StringView);
inline std::optional<Vector<uint8_t>> base64URLDecode(ASCIILiteral literal) { return base64URLDecode(StringView { literal }); }
std::optional<Vector<uint8_t>> base64URLDecode(std::span<const std::byte>);
std::optional<Vector<uint8_t>> base64URLDecode(std::span<const uint8_t>);
std::optional<Vector<uint8_t>> base64URLDecode(std::span<const char>);
std::optional<Vector<uint8_t>> base64URLDecode(const void*, unsigned);

// Versions for use with StringBuilder / makeString.

Base64Specification base64Encoded(std::span<const std::byte>, Base64EncodeMode = Base64EncodeMode::Default);
Base64Specification base64Encoded(std::span<const uint8_t>, Base64EncodeMode = Base64EncodeMode::Default);
Base64Specification base64Encoded(const CString&, Base64EncodeMode = Base64EncodeMode::Default);
Base64Specification base64Encoded(const void*, unsigned, Base64EncodeMode = Base64EncodeMode::Default);

Base64Specification base64URLEncoded(std::span<const std::byte>);
Base64Specification base64URLEncoded(std::span<const uint8_t>);
Base64Specification base64URLEncoded(const CString&);
Base64Specification base64URLEncoded(const void*, unsigned);


inline Vector<uint8_t> base64EncodeToVector(std::span<const uint8_t> input, Base64EncodeMode mode)
{
    return base64EncodeToVector(std::as_bytes(input), mode);
}

inline Vector<uint8_t> base64EncodeToVector(std::span<const char> input, Base64EncodeMode mode)
{
    return base64EncodeToVector(std::as_bytes(input), mode);
}

inline Vector<uint8_t> base64EncodeToVector(const CString& input, Base64EncodeMode mode)
{
    return base64EncodeToVector(input.bytes(), mode);
}

inline Vector<uint8_t> base64EncodeToVector(const void* input, unsigned length, Base64EncodeMode mode)
{
    return base64EncodeToVector({ static_cast<const std::byte*>(input), length }, mode);
}

inline String base64EncodeToString(std::span<const uint8_t> input, Base64EncodeMode mode)
{
    return base64EncodeToString(std::as_bytes(input), mode);
}

inline String base64EncodeToStringReturnNullIfOverflow(std::span<const uint8_t> input, Base64EncodeMode mode)
{
    return base64EncodeToStringReturnNullIfOverflow(std::as_bytes(input), mode);
}

inline String base64EncodeToString(std::span<const char> input, Base64EncodeMode mode)
{
    return base64EncodeToString(std::as_bytes(input), mode);
}

inline String base64EncodeToString(const CString& input, Base64EncodeMode mode)
{
    return base64EncodeToString(input.bytes(), mode);
}

inline String base64EncodeToStringReturnNullIfOverflow(const CString& input, Base64EncodeMode mode)
{
    return base64EncodeToStringReturnNullIfOverflow(input.bytes(), mode);
}

inline String base64EncodeToString(const void* input, unsigned length, Base64EncodeMode mode)
{
    return base64EncodeToString({ static_cast<const std::byte*>(input), length }, mode);
}

inline std::optional<Vector<uint8_t>> base64Decode(std::span<const uint8_t> input, Base64DecodeMode mode)
{
    return base64Decode(std::as_bytes(input), mode);
}

inline std::optional<Vector<uint8_t>> base64Decode(std::span<const char> input, Base64DecodeMode mode)
{
    return base64Decode(std::as_bytes(input), mode);
}

inline std::optional<Vector<uint8_t>> base64Decode(const void* input, unsigned length, Base64DecodeMode mode)
{
    return base64Decode({ static_cast<const std::byte*>(input), length }, mode);
}

inline Vector<uint8_t> base64URLEncodeToVector(std::span<const std::byte> input)
{
    return base64EncodeToVector(input, Base64EncodeMode::URL);
}

inline Vector<uint8_t> base64URLEncodeToVector(std::span<const uint8_t> input)
{
    return base64EncodeToVector(input, Base64EncodeMode::URL);
}

inline Vector<uint8_t> base64URLEncodeToVector(std::span<const char> input)
{
    return base64EncodeToVector(input, Base64EncodeMode::URL);
}

inline Vector<uint8_t> base64URLEncodeToVector(const CString& input)
{
    return base64EncodeToVector(input, Base64EncodeMode::URL);
}

inline Vector<uint8_t> base64URLEncodeToVector(const void* input, unsigned length)
{
    return base64EncodeToVector(input, length, Base64EncodeMode::URL);
}

inline String base64URLEncodeToString(std::span<const std::byte> input)
{
    return base64EncodeToString(input, Base64EncodeMode::URL);
}

inline String base64URLEncodeToString(std::span<const uint8_t> input)
{
    return base64EncodeToString(input, Base64EncodeMode::URL);
}

inline String base64URLEncodeToString(std::span<const char> input)
{
    return base64EncodeToString(input, Base64EncodeMode::URL);
}

inline String base64URLEncodeToString(const CString& input)
{
    return base64EncodeToString(input, Base64EncodeMode::URL);
}

inline String base64URLEncodeToString(const void* input, unsigned length)
{
    return base64EncodeToString(input, length, Base64EncodeMode::URL);
}

inline std::optional<Vector<uint8_t>> base64URLDecode(StringView input)
{
    return base64Decode(input, Base64DecodeMode::URL);
}

inline std::optional<Vector<uint8_t>> base64URLDecode(std::span<const std::byte> input)
{
    return base64Decode(input, Base64DecodeMode::URL);
}

inline std::optional<Vector<uint8_t>> base64URLDecode(std::span<const uint8_t> input)
{
    return base64Decode(input, Base64DecodeMode::URL);
}

inline std::optional<Vector<uint8_t>> base64URLDecode(std::span<const char> input)
{
    return base64Decode(input, Base64DecodeMode::URL);
}

inline std::optional<Vector<uint8_t>> base64URLDecode(const void* input, unsigned length)
{
    return base64Decode(input, length, Base64DecodeMode::URL);
}

template<typename CharacterType> bool isBase64OrBase64URLCharacter(CharacterType c)
{
    return isASCIIAlphanumeric(c) || c == '+' || c == '/' || c == '-' || c == '_';
}

inline unsigned calculateBase64EncodedSize(unsigned inputLength, Base64EncodeMode mode)
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

    switch (mode) {
    case Base64EncodeMode::Default:
        return basePaddedEncodedSize();

    case Base64EncodeMode::URL:
        return baseUnpaddedEncodedSize();
    }

    ASSERT_NOT_REACHED();
    return 0;
}

inline Base64Specification base64Encoded(std::span<const std::byte> input, Base64EncodeMode mode)
{
    return { input, mode };
}

inline Base64Specification base64Encoded(std::span<const uint8_t> input, Base64EncodeMode mode)
{
    return base64Encoded(std::as_bytes(input), mode);
}

inline Base64Specification base64Encoded(const CString& input, Base64EncodeMode mode)
{
    return base64Encoded(input.bytes(), mode);
}

inline Base64Specification base64Encoded(const void* input, unsigned length, Base64EncodeMode mode)
{
    return base64Encoded({ static_cast<const std::byte*>(input), length }, mode);
}

inline Base64Specification base64URLEncoded(std::span<const std::byte> input)
{
    return base64Encoded(input, Base64EncodeMode::URL);
}

inline Base64Specification base64URLEncoded(std::span<const uint8_t> input)
{
    return base64Encoded(input, Base64EncodeMode::URL);
}

inline Base64Specification base64URLEncoded(const CString& input)
{
    return base64Encoded(input, Base64EncodeMode::URL);
}

inline Base64Specification base64URLEncoded(const void* input, unsigned length)
{
    return base64Encoded(input, length, Base64EncodeMode::URL);
}

template<> class StringTypeAdapter<Base64Specification> {
public:
    StringTypeAdapter(const Base64Specification& base64)
        : m_base64 { base64 }
        , m_encodedLength { calculateBase64EncodedSize(base64.input.size(), base64.mode) }
    {
    }

    unsigned length() const { return m_encodedLength; }
    bool is8Bit() const { return true; }

    template<typename CharacterType> void writeTo(CharacterType* destination) const
    {
        base64Encode(m_base64.input, std::span(destination, m_encodedLength), m_base64.mode);
    }

private:
    Base64Specification m_base64;
    unsigned m_encodedLength;
};

} // namespace WTF

using WTF::Base64DecodeMode;
using WTF::base64Decode;
using WTF::base64EncodeToString;
using WTF::base64EncodeToVector;
using WTF::base64Encoded;
using WTF::base64URLDecode;
using WTF::base64URLEncodeToString;
using WTF::base64URLEncodeToVector;
using WTF::base64URLEncoded;
using WTF::isBase64OrBase64URLCharacter;
