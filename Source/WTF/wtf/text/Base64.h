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
#include <wtf/text/WTFString.h>

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

static constexpr unsigned maximumBase64LineLengthWhenInsertingLFs = 76;

// If the input string is pathologically large, just return nothing.
// Rather than being perfectly precise, this is a bit conservative.
static constexpr unsigned maximumBase64EncoderInputBufferSize = std::numeric_limits<unsigned>::max() / 77 * 76 / 4 * 3 - 2;

WTF_EXPORT_PRIVATE void base64Encode(const void*, unsigned, UChar*, unsigned, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);
WTF_EXPORT_PRIVATE void base64Encode(const void*, unsigned, LChar*, unsigned, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);

WTF_EXPORT_PRIVATE Vector<uint8_t> base64EncodeToVector(const void*, unsigned, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);
Vector<uint8_t> base64EncodeToVector(const Vector<uint8_t>&, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);
Vector<uint8_t> base64EncodeToVector(const Vector<char>&, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);
Vector<uint8_t> base64EncodeToVector(const CString&, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);

WTF_EXPORT_PRIVATE String base64EncodeToString(const void*, unsigned, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);
String base64EncodeToString(const Vector<uint8_t>&, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);
String base64EncodeToString(const Vector<char>&, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);
String base64EncodeToString(const CString&, Base64EncodePolicy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap = Base64EncodeMap::Default);

WTF_EXPORT_PRIVATE std::optional<Vector<uint8_t>> base64Decode(const String&, OptionSet<Base64DecodeOptions> = { }, Base64DecodeMap = Base64DecodeMap::Default);
WTF_EXPORT_PRIVATE std::optional<Vector<uint8_t>> base64Decode(StringView, OptionSet<Base64DecodeOptions> = { }, Base64DecodeMap = Base64DecodeMap::Default);
WTF_EXPORT_PRIVATE std::optional<Vector<uint8_t>> base64Decode(const Vector<uint8_t>&, OptionSet<Base64DecodeOptions> = { }, Base64DecodeMap = Base64DecodeMap::Default);
WTF_EXPORT_PRIVATE std::optional<Vector<uint8_t>> base64Decode(const Vector<char>&, OptionSet<Base64DecodeOptions> = { }, Base64DecodeMap = Base64DecodeMap::Default);
WTF_EXPORT_PRIVATE std::optional<Vector<uint8_t>> base64Decode(const uint8_t*, unsigned, OptionSet<Base64DecodeOptions> = { }, Base64DecodeMap = Base64DecodeMap::Default);
WTF_EXPORT_PRIVATE std::optional<Vector<uint8_t>> base64Decode(const char*, unsigned, OptionSet<Base64DecodeOptions> = { }, Base64DecodeMap = Base64DecodeMap::Default);

inline Vector<uint8_t> base64EncodeToVector(const Vector<uint8_t>& input, Base64EncodePolicy policy, Base64EncodeMap map)
{
    return base64EncodeToVector(input.data(), input.size(), policy, map);
}

inline Vector<uint8_t> base64EncodeToVector(const Vector<char>& input, Base64EncodePolicy policy, Base64EncodeMap map)
{
    return base64EncodeToVector(input.data(), input.size(), policy, map);
}

inline Vector<uint8_t> base64EncodeToVector(const CString& input, Base64EncodePolicy policy, Base64EncodeMap map)
{
    return base64EncodeToVector(input.data(), input.length(), policy, map);
}

inline String base64EncodeToString(const Vector<uint8_t>& input, Base64EncodePolicy policy, Base64EncodeMap map)
{
    return base64EncodeToString(input.data(), input.size(), policy, map);
}

inline String base64EncodeToString(const Vector<char>& input, Base64EncodePolicy policy, Base64EncodeMap map)
{
    return base64EncodeToString(input.data(), input.size(), policy, map);
}

inline String base64EncodeToString(const CString& input, Base64EncodePolicy policy, Base64EncodeMap map)
{
    return base64EncodeToString(input.data(), input.length(), policy, map);
}


// ======================================================================================
// All the same functions modified for base64url, as defined in RFC 4648.
// This format uses '-' and '_' instead of '+' and '/' respectively.
// ======================================================================================

Vector<uint8_t> base64URLEncodeToVector(const void*, unsigned);
Vector<uint8_t> base64URLEncodeToVector(const Vector<uint8_t>&);
Vector<uint8_t> base64URLEncodeToVector(const Vector<char>&);
Vector<uint8_t> base64URLEncodeToVector(const CString&);

String base64URLEncodeToString(const void*, unsigned);
String base64URLEncodeToString(const Vector<uint8_t>&);
String base64URLEncodeToString(const Vector<char>&);
String base64URLEncodeToString(const CString&);

WTF_EXPORT_PRIVATE std::optional<Vector<uint8_t>> base64URLDecode(const String&);
WTF_EXPORT_PRIVATE std::optional<Vector<uint8_t>> base64URLDecode(StringView);
WTF_EXPORT_PRIVATE std::optional<Vector<uint8_t>> base64URLDecode(const Vector<uint8_t>&);
WTF_EXPORT_PRIVATE std::optional<Vector<uint8_t>> base64URLDecode(const Vector<char>&);
WTF_EXPORT_PRIVATE std::optional<Vector<uint8_t>> base64URLDecode(const uint8_t*, unsigned);
WTF_EXPORT_PRIVATE std::optional<Vector<uint8_t>> base64URLDecode(const char*, unsigned);

inline Vector<uint8_t> base64URLEncodeToVector(const void* input, unsigned length)
{
    return base64EncodeToVector(input, length, Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

inline Vector<uint8_t> base64URLEncodeToVector(const Vector<uint8_t>& input)
{
    return base64EncodeToVector(input.data(), input.size(), Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

inline Vector<uint8_t> base64URLEncodeToVector(const Vector<char>& input)
{
    return base64EncodeToVector(input.data(), input.size(), Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

inline Vector<uint8_t> base64URLEncodeToVector(const CString& input)
{
    return base64EncodeToVector(input.data(), input.length(), Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

inline String base64URLEncodeToString(const void* input, unsigned length)
{
    return base64EncodeToString(input, length, Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

inline String base64URLEncodeToString(const Vector<uint8_t>& input)
{
    return base64EncodeToString(input.data(), input.size(), Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

inline String base64URLEncodeToString(const Vector<char>& input)
{
    return base64EncodeToString(input.data(), input.size(), Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

inline String base64URLEncodeToString(const CString& input)
{
    return base64EncodeToString(input.data(), input.length(), Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

template<typename CharacterType> static inline bool isBase64OrBase64URLCharacter(CharacterType c)
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

struct Base64Specification {
    const void* inputData;
    unsigned inputLength;
    Base64EncodePolicy policy;
    Base64EncodeMap map;
};

inline Base64Specification base64Encoded(const void* inputData, unsigned inputLength, Base64EncodePolicy policy = Base64EncodePolicy::DoNotInsertLFs, Base64EncodeMap map = Base64EncodeMap::Default)
{
    return { inputData, inputLength, policy, map };
}

inline Base64Specification base64Encoded(const Vector<uint8_t>& input, Base64EncodePolicy policy = Base64EncodePolicy::DoNotInsertLFs)
{
    return base64Encoded(input.data(), input.size(), policy);
}

inline Base64Specification base64Encoded(const Vector<char>& input, Base64EncodePolicy policy = Base64EncodePolicy::DoNotInsertLFs)
{
    return base64Encoded(input.data(), input.size(), policy);
}

inline Base64Specification base64Encoded(const CString& input, Base64EncodePolicy policy = Base64EncodePolicy::DoNotInsertLFs)
{
    return base64Encoded(input.data(), input.length(), policy);
}

inline Base64Specification base64URLEncoded(const void* inputData, unsigned inputLength)
{
    return base64Encoded(inputData, inputLength, Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

inline Base64Specification base64URLEncoded(const Vector<uint8_t>& input)
{
    return base64Encoded(input.data(), input.size(), Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

inline Base64Specification base64URLEncoded(const Vector<char>& input)
{
    return base64Encoded(input.data(), input.size(), Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

inline Base64Specification base64URLEncoded(const CString& input)
{
    return base64Encoded(input.data(), input.length(), Base64EncodePolicy::URL, Base64EncodeMap::URL);
}

template<> class StringTypeAdapter<Base64Specification> {
public:
    StringTypeAdapter(const Base64Specification& base64)
        : m_base64 { base64 }
        , m_encodedLength { calculateBase64EncodedSize(base64.inputLength, base64.policy) }
    {
    }

    unsigned length() const { return m_encodedLength; }
    bool is8Bit() const { return true; }

    template<typename CharacterType> void writeTo(CharacterType* destination) const
    {
        base64Encode(m_base64.inputData, m_base64.inputLength, destination, m_encodedLength, m_base64.policy, m_base64.map);
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
