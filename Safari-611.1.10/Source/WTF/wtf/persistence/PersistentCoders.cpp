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

#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WTF {
namespace Persistence {

void Coder<AtomString>::encode(Encoder& encoder, const AtomString& atomString)
{
    encoder << atomString.string();
}

Optional<AtomString> Coder<AtomString>::decode(Decoder& decoder)
{
    Optional<String> string;
    decoder >> string;
    if (!string)
        return WTF::nullopt;

    return {{ WTFMove(*string) }};
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
    encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(string.data()), length);
}

Optional<CString> Coder<CString>::decode(Decoder& decoder)
{
    Optional<uint32_t> length;
    decoder >> length;
    if (!length)
        return WTF::nullopt;

    if (length == std::numeric_limits<uint32_t>::max()) {
        // This is the null string.
        return CString();
    }

    // Before allocating the string, make sure that the decoder buffer is big enough.
    if (!decoder.bufferIsLargeEnoughToContain<char>(*length))
        return WTF::nullopt;

    char* buffer;
    CString string = CString::newUninitialized(*length, buffer);
    if (!decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(buffer), *length))
        return WTF::nullopt;

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
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(string.characters8()), length * sizeof(LChar));
    else
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(string.characters16()), length * sizeof(UChar));
}

template <typename CharacterType>
static inline Optional<String> decodeStringText(Decoder& decoder, uint32_t length)
{
    // Before allocating the string, make sure that the decoder buffer is big enough.
    if (!decoder.bufferIsLargeEnoughToContain<CharacterType>(length))
        return WTF::nullopt;

    CharacterType* buffer;
    String string = String::createUninitialized(length, buffer);
    if (!decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(buffer), length * sizeof(CharacterType)))
        return WTF::nullopt;
    
    return string;
}

Optional<String> Coder<String>::decode(Decoder& decoder)
{
    Optional<uint32_t> length;
    decoder >> length;
    if (!length)
        return WTF::nullopt;

    if (*length == std::numeric_limits<uint32_t>::max()) {
        // This is the null string.
        return String();
    }

    Optional<bool> is8Bit;
    decoder >> is8Bit;
    if (!is8Bit)
        return WTF::nullopt;

    if (*is8Bit)
        return decodeStringText<LChar>(decoder, *length);
    return decodeStringText<UChar>(decoder, *length);
}

void Coder<SHA1::Digest>::encode(Encoder& encoder, const SHA1::Digest& digest)
{
    encoder.encodeFixedLengthData(digest.data(), sizeof(digest));
}

Optional<SHA1::Digest> Coder<SHA1::Digest>::decode(Decoder& decoder)
{
    SHA1::Digest tmp;
    if (!decoder.decodeFixedLengthData(tmp.data(), sizeof(tmp)))
        return WTF::nullopt;
    return tmp;
}

}
}
