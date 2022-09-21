/*
 * Copyright (C) 2011, 2014-2015 Apple Inc. All rights reserved.
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

#include "config.h"
#include <wtf/persistence/PersistentCoders.h>

#include <wtf/URL.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WTF::Persistence {

void Coder<AtomString>::encode(Encoder& encoder, const AtomString& atomString)
{
    encoder << atomString.string();
}

// FIXME: Constructing a String and then looking it up in the AtomStringTable is inefficient.
// Ideally, we wouldn't need to allocate a String when it is already in the AtomStringTable.
std::optional<AtomString> Coder<AtomString>::decode(Decoder& decoder)
{
    std::optional<String> string;
    decoder >> string;
    if (!string)
        return std::nullopt;

    return { AtomString { WTFMove(*string) } };
}

void Coder<CString>::encode(Encoder& encoder, const CString& string)
{
    // Special case the null string.
    if (string.isNull()) {
        encoder << std::numeric_limits<uint32_t>::max();
        return;
    }

    uint32_t length = string.length();
    encoder << length;
    encoder.encodeFixedLengthData({ string.dataAsUInt8Ptr(), length });
}

std::optional<CString> Coder<CString>::decode(Decoder& decoder)
{
    std::optional<uint32_t> length;
    decoder >> length;
    if (!length)
        return std::nullopt;

    if (length == std::numeric_limits<uint32_t>::max()) {
        // This is the null string.
        return CString();
    }

    // Before allocating the string, make sure that the decoder buffer is big enough.
    if (!decoder.bufferIsLargeEnoughToContain<char>(*length))
        return std::nullopt;

    char* buffer;
    CString string = CString::newUninitialized(*length, buffer);
    if (!decoder.decodeFixedLengthData({ reinterpret_cast<uint8_t*>(buffer), *length }))
        return std::nullopt;

    return string;
}

void Coder<String>::encode(Encoder& encoder, const String& string)
{
    // Special case the null string.
    if (string.isNull()) {
        encoder << std::numeric_limits<uint32_t>::max();
        return;
    }

    uint32_t length = string.length();
    bool is8Bit = string.is8Bit();

    encoder << length << is8Bit;

    if (is8Bit)
        encoder.encodeFixedLengthData({ string.characters8(), length });
    else
        encoder.encodeFixedLengthData({ reinterpret_cast<const uint8_t*>(string.characters16()), length * sizeof(UChar) });
}

template <typename CharacterType>
static inline std::optional<String> decodeStringText(Decoder& decoder, uint32_t length)
{
    // Before allocating the string, make sure that the decoder buffer is big enough.
    if (!decoder.bufferIsLargeEnoughToContain<CharacterType>(length))
        return std::nullopt;

    CharacterType* buffer;
    String string = String::createUninitialized(length, buffer);
    if (!decoder.decodeFixedLengthData({ reinterpret_cast<uint8_t*>(buffer), length * sizeof(CharacterType) }))
        return std::nullopt;
    
    return string;
}

std::optional<String> Coder<String>::decode(Decoder& decoder)
{
    std::optional<uint32_t> length;
    decoder >> length;
    if (!length)
        return std::nullopt;

    if (*length == std::numeric_limits<uint32_t>::max()) {
        // This is the null string.
        return String();
    }

    std::optional<bool> is8Bit;
    decoder >> is8Bit;
    if (!is8Bit)
        return std::nullopt;

    if (*is8Bit)
        return decodeStringText<LChar>(decoder, *length);
    return decodeStringText<UChar>(decoder, *length);
}

void Coder<URL>::encode(Encoder& encoder, const URL& url)
{
    encoder << url.string();
}

std::optional<URL> Coder<URL>::decode(Decoder& decoder)
{
    std::optional<String> string;
    decoder >> string;
    if (!string)
        return std::nullopt;
    return URL(WTFMove(*string));
}

void Coder<SHA1::Digest>::encode(Encoder& encoder, const SHA1::Digest& digest)
{
    encoder.encodeFixedLengthData({ digest.data(), sizeof(digest) });
}

std::optional<SHA1::Digest> Coder<SHA1::Digest>::decode(Decoder& decoder)
{
    SHA1::Digest tmp;
    if (!decoder.decodeFixedLengthData({ tmp.data(), sizeof(tmp) }))
        return std::nullopt;
    return tmp;
}

void Coder<WallTime>::encode(Encoder& encoder, const WallTime& time)
{
    encoder << time.secondsSinceEpoch().value();
}

std::optional<WallTime> Coder<WallTime>::decode(Decoder& decoder)
{
    std::optional<double> value;
    decoder >> value;
    if (!value)
        return std::nullopt;

    return WallTime::fromRawSeconds(*value);
}

void Coder<Seconds>::encode(Encoder& encoder, const Seconds& seconds)
{
    encoder << seconds.value();
}

std::optional<Seconds> Coder<Seconds>::decode(Decoder& decoder)
{
    std::optional<double> value;
    decoder >> value;
    if (!value)
        return std::nullopt;
    return Seconds(*value);
}

}
