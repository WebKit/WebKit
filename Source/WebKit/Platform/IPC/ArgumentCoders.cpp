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

#include "DaemonDecoder.h"
#include "DaemonEncoder.h"
#include "DataReference.h"
#include "StreamConnectionEncoder.h"
#include <wtf/text/AtomString.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace IPC {

template<typename Encoder>
void ArgumentCoder<WallTime>::encode(Encoder& encoder, const WallTime& time)
{
    encoder << time.secondsSinceEpoch().value();
}
template
void ArgumentCoder<WallTime>::encode<Encoder>(Encoder&, const WallTime&);
template
void ArgumentCoder<WallTime>::encode<WebKit::Daemon::Encoder>(WebKit::Daemon::Encoder&, const WallTime&);

WARN_UNUSED_RETURN bool ArgumentCoder<WallTime>::decode(Decoder& decoder, WallTime& time)
{
    double value;
    if (!decoder.decode(value))
        return false;

    time = WallTime::fromRawSeconds(value);
    return true;
}

template<typename Decoder>
WARN_UNUSED_RETURN std::optional<WallTime> ArgumentCoder<WallTime>::decode(Decoder& decoder)
{
    std::optional<double> time;
    decoder >> time;
    if (!time)
        return std::nullopt;
    return WallTime::fromRawSeconds(*time);
}
template
std::optional<WallTime> ArgumentCoder<WallTime>::decode<Decoder>(Decoder&);
template
std::optional<WallTime> ArgumentCoder<WallTime>::decode<WebKit::Daemon::Decoder>(WebKit::Daemon::Decoder&);

void ArgumentCoder<AtomString>::encode(Encoder& encoder, const AtomString& atomString)
{
    encoder << atomString.string();
}

WARN_UNUSED_RETURN bool ArgumentCoder<AtomString>::decode(Decoder& decoder, AtomString& atomString)
{
    String string;
    if (!decoder.decode(string))
        return false;

    atomString = AtomString { string };
    return true;
}

template<typename Encoder>
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
template void ArgumentCoder<CString>::encode<Encoder>(Encoder&, const CString&);
template void ArgumentCoder<CString>::encode<WebKit::Daemon::Encoder>(WebKit::Daemon::Encoder&, const CString&);

template<typename Decoder>
std::optional<CString> ArgumentCoder<CString>::decode(Decoder& decoder)
{
    std::optional<uint32_t> length;
    decoder >> length;
    if (!length)
        return std::nullopt;

    if (*length == std::numeric_limits<uint32_t>::max()) {
        // This is the null string.
        return CString();
    }

    // Before allocating the string, make sure that the decoder buffer is big enough.
    if (!decoder.template bufferIsLargeEnoughToContain<char>(*length))
        return std::nullopt;

    char* buffer;
    CString string = CString::newUninitialized(*length, buffer);
    if (!decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(buffer), *length, 1))
        return std::nullopt;

    return string;
}
template
std::optional<CString> ArgumentCoder<CString>::decode<Decoder>(Decoder&);
template
std::optional<CString> ArgumentCoder<CString>::decode<WebKit::Daemon::Decoder>(WebKit::Daemon::Decoder&);

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
template
void ArgumentCoder<String>::encode<WebKit::Daemon::Encoder>(WebKit::Daemon::Encoder&, const String&);

template<typename CharacterType, typename Decoder>
static inline std::optional<String> decodeStringText(Decoder& decoder, uint32_t length)
{
    // Before allocating the string, make sure that the decoder buffer is big enough.
    if (!decoder.template bufferIsLargeEnoughToContain<CharacterType>(length))
        return std::nullopt;
    
    CharacterType* buffer;
    String string = String::createUninitialized(length, buffer);
    if (!decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(buffer), length * sizeof(CharacterType), alignof(CharacterType)))
        return std::nullopt;
    
    return string;
}

template<typename Decoder>
WARN_UNUSED_RETURN std::optional<String> ArgumentCoder<String>::decode(Decoder& decoder)
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
template
std::optional<String> ArgumentCoder<String>::decode<Decoder>(Decoder&);
template
std::optional<String> ArgumentCoder<String>::decode<WebKit::Daemon::Decoder>(WebKit::Daemon::Decoder&);

WARN_UNUSED_RETURN bool ArgumentCoder<String>::decode(Decoder& decoder, String& result)
{
    std::optional<String> string;
    decoder >> string;
    if (!string)
        return false;
    result = WTFMove(*string);
    return true;
}

template<typename Encoder>
void ArgumentCoder<StringView>::encode(Encoder& encoder, StringView string)
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
void ArgumentCoder<StringView>::encode<Encoder>(Encoder&, StringView);
template
void ArgumentCoder<StringView>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, StringView);
template
void ArgumentCoder<StringView>::encode<WebKit::Daemon::Encoder>(WebKit::Daemon::Encoder&, StringView);

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

#endif // HAVE(AUDIT_TOKEN)

} // namespace IPC
