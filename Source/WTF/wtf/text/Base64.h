/*
 * Copyright (C) 2006 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 * Copyright (C) 2013-2024 Apple Inc. All rights reserved.
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
#include <wtf/text/StringView.h>

namespace WTF {

enum class Base64EncodeOption {
    URL = 1 << 0,
    OmitPadding = 1 << 1,
};

enum class Base64DecodeOption {
    URL = 1 << 0,
    ValidatePadding = 1 << 1,
    IgnoreWhitespace = 1 << 2,
};

struct Base64Specification {
    std::span<const std::byte> input;
    OptionSet<Base64EncodeOption> options;
};

// If the input string is pathologically large, just return nothing.
// Rather than being perfectly precise, this is a bit conservative.
static constexpr unsigned maximumBase64EncoderInputBufferSize = std::numeric_limits<unsigned>::max() / 77 * 76 / 4 * 3 - 2;

WTF_EXPORT_PRIVATE unsigned calculateBase64EncodedSize(unsigned inputLength, OptionSet<Base64EncodeOption>);

template<typename CharacterType> bool isBase64OrBase64URLCharacter(CharacterType);

WTF_EXPORT_PRIVATE void base64Encode(std::span<const std::byte>, std::span<UChar>, OptionSet<Base64EncodeOption> = { });
WTF_EXPORT_PRIVATE void base64Encode(std::span<const std::byte>, std::span<LChar>, OptionSet<Base64EncodeOption> = { });

WTF_EXPORT_PRIVATE Vector<uint8_t> base64EncodeToVector(std::span<const std::byte>, OptionSet<Base64EncodeOption> = { });
Vector<uint8_t> base64EncodeToVector(std::span<const uint8_t>, OptionSet<Base64EncodeOption> = { });

WTF_EXPORT_PRIVATE String base64EncodeToString(std::span<const std::byte>, OptionSet<Base64EncodeOption> = { });
String base64EncodeToString(std::span<const uint8_t>, OptionSet<Base64EncodeOption> = { });

WTF_EXPORT_PRIVATE String base64EncodeToStringReturnNullIfOverflow(std::span<const std::byte>, OptionSet<Base64EncodeOption> = { });
String base64EncodeToStringReturnNullIfOverflow(const CString&, OptionSet<Base64EncodeOption> = { });

WTF_EXPORT_PRIVATE std::optional<Vector<uint8_t>> base64Decode(std::span<const std::byte>, OptionSet<Base64DecodeOption> = { });
WTF_EXPORT_PRIVATE std::optional<Vector<uint8_t>> base64Decode(StringView, OptionSet<Base64DecodeOption> = { });
std::optional<Vector<uint8_t>> base64Decode(std::span<const uint8_t>, OptionSet<Base64DecodeOption> = { });

WTF_EXPORT_PRIVATE String base64DecodeToString(StringView, OptionSet<Base64DecodeOption> = { });

// All the same functions modified for base64url, as defined in RFC 4648.
// This format uses '-' and '_' instead of '+' and '/' respectively.

Vector<uint8_t> base64URLEncodeToVector(std::span<const std::byte>);
Vector<uint8_t> base64URLEncodeToVector(std::span<const uint8_t>);

String base64URLEncodeToString(std::span<const std::byte>);
String base64URLEncodeToString(std::span<const uint8_t>);

std::optional<Vector<uint8_t>> base64URLDecode(StringView);
inline std::optional<Vector<uint8_t>> base64URLDecode(ASCIILiteral literal) { return base64URLDecode(StringView { literal }); }
std::optional<Vector<uint8_t>> base64URLDecode(std::span<const std::byte>);
std::optional<Vector<uint8_t>> base64URLDecode(std::span<const uint8_t>);

// Versions for use with StringBuilder / makeString.

Base64Specification base64Encoded(std::span<const std::byte>, OptionSet<Base64EncodeOption> = { });
Base64Specification base64Encoded(std::span<const uint8_t>, OptionSet<Base64EncodeOption> = { });

Base64Specification base64URLEncoded(std::span<const std::byte>);
Base64Specification base64URLEncoded(std::span<const uint8_t>);

inline Vector<uint8_t> base64EncodeToVector(std::span<const uint8_t> input, OptionSet<Base64EncodeOption> options)
{
    return base64EncodeToVector(std::as_bytes(input), options);
}

inline String base64EncodeToString(std::span<const uint8_t> input, OptionSet<Base64EncodeOption> options)
{
    return base64EncodeToString(std::as_bytes(input), options);
}

inline String base64EncodeToStringReturnNullIfOverflow(std::span<const uint8_t> input, OptionSet<Base64EncodeOption> options)
{
    return base64EncodeToStringReturnNullIfOverflow(std::as_bytes(input), options);
}

inline String base64EncodeToStringReturnNullIfOverflow(const CString& input, OptionSet<Base64EncodeOption> options)
{
    return base64EncodeToStringReturnNullIfOverflow(input.span(), options);
}

inline std::optional<Vector<uint8_t>> base64Decode(std::span<const uint8_t> input, OptionSet<Base64DecodeOption> options)
{
    return base64Decode(std::as_bytes(input), options);
}

inline Vector<uint8_t> base64URLEncodeToVector(std::span<const std::byte> input)
{
    return base64EncodeToVector(input, { Base64EncodeOption::URL, Base64EncodeOption::OmitPadding });
}

inline Vector<uint8_t> base64URLEncodeToVector(std::span<const uint8_t> input)
{
    return base64EncodeToVector(input, { Base64EncodeOption::URL, Base64EncodeOption::OmitPadding });
}

inline String base64URLEncodeToString(std::span<const std::byte> input)
{
    return base64EncodeToString(input, { Base64EncodeOption::URL, Base64EncodeOption::OmitPadding });
}

inline String base64URLEncodeToString(std::span<const uint8_t> input)
{
    return base64EncodeToString(input, { Base64EncodeOption::URL, Base64EncodeOption::OmitPadding });
}

inline std::optional<Vector<uint8_t>> base64URLDecode(StringView input)
{
    return base64Decode(input, { Base64DecodeOption::URL });
}

inline std::optional<Vector<uint8_t>> base64URLDecode(std::span<const std::byte> input)
{
    return base64Decode(input, { Base64DecodeOption::URL });
}

inline std::optional<Vector<uint8_t>> base64URLDecode(std::span<const uint8_t> input)
{
    return base64Decode(input, { Base64DecodeOption::URL });
}

template<typename CharacterType> bool isBase64OrBase64URLCharacter(CharacterType c)
{
    return isASCIIAlphanumeric(c) || c == '+' || c == '/' || c == '-' || c == '_';
}

inline Base64Specification base64Encoded(std::span<const std::byte> input, OptionSet<Base64EncodeOption> options)
{
    return { input, options };
}

inline Base64Specification base64Encoded(std::span<const uint8_t> input, OptionSet<Base64EncodeOption> options)
{
    return base64Encoded(std::as_bytes(input), options);
}

inline Base64Specification base64URLEncoded(std::span<const std::byte> input)
{
    return base64Encoded(input, { Base64EncodeOption::URL, Base64EncodeOption::OmitPadding });
}

inline Base64Specification base64URLEncoded(std::span<const uint8_t> input)
{
    return base64Encoded(input, { Base64EncodeOption::URL, Base64EncodeOption::OmitPadding });
}

template<> class StringTypeAdapter<Base64Specification> {
public:
    StringTypeAdapter(const Base64Specification& base64)
        : m_base64 { base64 }
        , m_encodedLength { calculateBase64EncodedSize(base64.input.size(), base64.options) }
    {
    }

    unsigned length() const { return m_encodedLength; }
    bool is8Bit() const { return true; }

    template<typename CharacterType> void writeTo(std::span<CharacterType> destination) const
    {
        base64Encode(m_base64.input, destination.first(m_encodedLength), m_base64.options);
    }

private:
    Base64Specification m_base64;
    unsigned m_encodedLength;
};

enum class Alphabet : uint8_t { Base64, Base64URL };
enum class LastChunkHandling : uint8_t { Loose, Strict, StopBeforePartial };
enum class FromBase64ShouldThrowError: bool { No, Yes };
WTF_EXPORT_PRIVATE std::tuple<FromBase64ShouldThrowError, size_t, size_t> fromBase64(StringView, std::span<uint8_t>, Alphabet, LastChunkHandling);
WTF_EXPORT_PRIVATE size_t maxLengthFromBase64(StringView);

} // namespace WTF

using WTF::Base64EncodeOption;
using WTF::Base64DecodeOption;
using WTF::base64Decode;
using WTF::base64EncodeToString;
using WTF::base64EncodeToVector;
using WTF::base64Encoded;
using WTF::base64URLDecode;
using WTF::base64URLEncodeToString;
using WTF::base64URLEncodeToVector;
using WTF::base64URLEncoded;
using WTF::isBase64OrBase64URLCharacter;
using WTF::fromBase64;
using WTF::maxLengthFromBase64;
