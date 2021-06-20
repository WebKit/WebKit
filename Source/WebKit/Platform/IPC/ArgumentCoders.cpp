/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "ArgumentCoders.h"

#include "DataReference.h"
#include "StreamConnectionEncoder.h"
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace IPC {

void ArgumentCoder<WallTime>::encode(Encoder& encoder, const WallTime& time)
{
    encoder << time.secondsSinceEpoch().value();
}

WARN_UNUSED_RETURN bool ArgumentCoder<WallTime>::decode(Decoder& decoder, WallTime& time)
{
    double value;
    if (!decoder.decode(value))
        return false;

    time = WallTime::fromRawSeconds(value);
    return true;
}

WARN_UNUSED_RETURN std::optional<WallTime> ArgumentCoder<WallTime>::decode(Decoder& decoder)
{
    std::optional<double> time;
    decoder >> time;
    if (!time)
        return std::nullopt;
    return WallTime::fromRawSeconds(*time);
}

void ArgumentCoder<AtomString>::encode(Encoder& encoder, const AtomString& atomString)
{
    encoder << atomString.string();
}

WARN_UNUSED_RETURN bool ArgumentCoder<AtomString>::decode(Decoder& decoder, AtomString& atomString)
{
    String string;
    if (!decoder.decode(string))
        return false;

    atomString = string;
    return true;
}

void ArgumentCoder<CString>::encode(Encoder& encoder, const CString& string)
{
    // Special case the null string.
    if (string.isNull()) {
        encoder << std::numeric_limits<uint32_t>::max();
        return;
    }

    uint32_t length = string.length();
    encoder << length;
    encoder.encodeFixedLengthData(string.dataAsUInt8Ptr(), length, 1);
}

WARN_UNUSED_RETURN bool ArgumentCoder<CString>::decode(Decoder& decoder, CString& result)
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
    if (!decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(buffer), length, 1))
        return false;

    result = string;
    return true;
}

template<typename Encoder>
void ArgumentCoder<String>::encode(Encoder& encoder, const String& string)
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
        encoder.encodeFixedLengthData(string.characters8(), length * sizeof(LChar), alignof(LChar));
    else
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(string.characters16()), length * sizeof(UChar), alignof(UChar));
}
template
void ArgumentCoder<String>::encode<Encoder>(Encoder&, const String&);
template
void ArgumentCoder<String>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, const String&);

template <typename CharacterType>
static inline std::optional<String> decodeStringText(Decoder& decoder, uint32_t length)
{
    // Before allocating the string, make sure that the decoder buffer is big enough.
    if (!decoder.bufferIsLargeEnoughToContain<CharacterType>(length))
        return std::nullopt;
    
    CharacterType* buffer;
    String string = String::createUninitialized(length, buffer);
    if (!decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(buffer), length * sizeof(CharacterType), alignof(CharacterType)))
        return std::nullopt;
    
    return string;
}

WARN_UNUSED_RETURN std::optional<String> ArgumentCoder<String>::decode(Decoder& decoder)
{
    uint32_t length;
    if (!decoder.decode(length))
        return std::nullopt;
    
    if (length == std::numeric_limits<uint32_t>::max()) {
        // This is the null string.
        return String();
    }
    
    bool is8Bit;
    if (!decoder.decode(is8Bit))
        return std::nullopt;
    
    if (is8Bit)
        return decodeStringText<LChar>(decoder, length);
    return decodeStringText<UChar>(decoder, length);
}

WARN_UNUSED_RETURN bool ArgumentCoder<String>::decode(Decoder& decoder, String& result)
{
    std::optional<String> string;
    decoder >> string;
    if (!string)
        return false;
    result = WTFMove(*string);
    return true;
}

void ArgumentCoder<SHA1::Digest>::encode(Encoder& encoder, const SHA1::Digest& digest)
{
    encoder.encodeFixedLengthData(digest.data(), sizeof(digest), 1);
}

WARN_UNUSED_RETURN bool ArgumentCoder<SHA1::Digest>::decode(Decoder& decoder, SHA1::Digest& digest)
{
    return decoder.decodeFixedLengthData(digest.data(), sizeof(digest), 1);
}

#if HAVE(AUDIT_TOKEN)
void ArgumentCoder<audit_token_t>::encode(Encoder& encoder, const audit_token_t& auditToken)
{
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(auditToken.val); i++)
        encoder << auditToken.val[i];
}

WARN_UNUSED_RETURN bool ArgumentCoder<audit_token_t>::decode(Decoder& decoder, audit_token_t& auditToken)
{
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(auditToken.val); i++) {
        if (!decoder.decode(auditToken.val[i]))
            return false;
    }
    return true;
}
#endif

void ArgumentCoder<Monostate>::encode(Encoder&, const Monostate&)
{
}

WARN_UNUSED_RETURN std::optional<Monostate> ArgumentCoder<Monostate>::decode(Decoder&)
{
    return Monostate { };
}

} // namespace IPC
