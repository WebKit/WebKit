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
#include "NetworkCacheCoders.h"

#if ENABLE(NETWORK_CACHE)

#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
namespace NetworkCache {

void Coder<AtomicString>::encode(Encoder& encoder, const AtomicString& atomicString)
{
    encoder << atomicString.string();
}

bool Coder<AtomicString>::decode(Decoder& decoder, AtomicString& atomicString)
{
    String string;
    if (!decoder.decode(string))
        return false;

    atomicString = string;
    return true;
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

bool Coder<CString>::decode(Decoder& decoder, CString& result)
{
    uint32_t length;
    if (!decoder.decode(length))
        return false;

    if (length == std::numeric_limits<uint32_t>::max()) {
        // This is the null string.
        result = CString();
        return true;
    }

    // Before allocating the string, make sure that the decoder buffer is big enough.
    if (!decoder.bufferIsLargeEnoughToContain<char>(length))
        return false;

    char* buffer;
    CString string = CString::newUninitialized(length, buffer);
    if (!decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(buffer), length))
        return false;

    result = string;
    return true;
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
static inline bool decodeStringText(Decoder& decoder, uint32_t length, String& result)
{
    // Before allocating the string, make sure that the decoder buffer is big enough.
    if (!decoder.bufferIsLargeEnoughToContain<CharacterType>(length))
        return false;

    CharacterType* buffer;
    String string = String::createUninitialized(length, buffer);
    if (!decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(buffer), length * sizeof(CharacterType)))
        return false;
    
    result = string;
    return true;    
}

bool Coder<String>::decode(Decoder& decoder, String& result)
{
    uint32_t length;
    if (!decoder.decode(length))
        return false;

    if (length == std::numeric_limits<uint32_t>::max()) {
        // This is the null string.
        result = String();
        return true;
    }

    bool is8Bit;
    if (!decoder.decode(is8Bit))
        return false;

    if (is8Bit)
        return decodeStringText<LChar>(decoder, length, result);
    return decodeStringText<UChar>(decoder, length, result);
}


void Coder<SHA1::Digest>::encode(Encoder& encoder, const SHA1::Digest& digest)
{
    encoder.encodeFixedLengthData(digest.data(), sizeof(digest));
}

bool Coder<SHA1::Digest>::decode(Decoder& decoder, SHA1::Digest& digest)
{
    return decoder.decodeFixedLengthData(digest.data(), sizeof(digest));
}

// Store common HTTP headers as strings instead of using their value in the HTTPHeaderName enumeration
// so that the headers stored in the cache stays valid even after HTTPHeaderName.in gets updated.
void Coder<WebCore::HTTPHeaderMap>::encode(Encoder& encoder, const WebCore::HTTPHeaderMap& headers)
{
    encoder << static_cast<uint64_t>(headers.size());
    for (auto& keyValue : headers) {
        encoder << keyValue.key;
        encoder << keyValue.value;
    }
}

bool Coder<WebCore::HTTPHeaderMap>::decode(Decoder& decoder, WebCore::HTTPHeaderMap& headers)
{
    uint64_t headersSize;
    if (!decoder.decode(headersSize))
        return false;
    for (uint64_t i = 0; i < headersSize; ++i) {
        String name;
        if (!decoder.decode(name))
            return false;
        String value;
        if (!decoder.decode(value))
            return false;
        headers.add(name, value);
    }
    return true;
}

}
}

#endif
