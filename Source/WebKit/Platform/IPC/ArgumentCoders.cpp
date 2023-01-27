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
void ArgumentCoder<CString>::encode(Encoder& encoder, const CString& string)
{
    // Special case the null string.
    if (string.isNull()) {
        encoder << std::numeric_limits<size_t>::max();
        return;
    }

    encoder << static_cast<size_t>(string.length());
    encoder.encodeSpan(string.bytes());
}
template void ArgumentCoder<CString>::encode<Encoder>(Encoder&, const CString&);

template<typename Decoder>
std::optional<CString> ArgumentCoder<CString>::decode(Decoder& decoder)
{
    auto length = decoder.template decode<size_t>();
    if (!length)
        return std::nullopt;

    if (*length == std::numeric_limits<size_t>::max()) {
        // This is the null string.
        return CString();
    }

    auto data = decoder.template decodeSpan<uint8_t>(*length);
    if (!data.data())
        return std::nullopt;

    char* buffer;
    CString string = CString::newUninitialized(*length, buffer);
    static_assert(sizeof(typename decltype(data)::element_type) == 1);
    memcpy(buffer, data.data(), data.size_bytes());
    return string;
}
template
std::optional<CString> ArgumentCoder<CString>::decode<Decoder>(Decoder&);

template<typename Encoder>
void ArgumentCoder<String>::encode(Encoder& encoder, const String& string)
{
    // Special case the null string.
    if (string.isNull()) {
        encoder << std::numeric_limits<unsigned>::max();
        return;
    }

    unsigned length = string.length();
    bool is8Bit = string.is8Bit();

    encoder << length << is8Bit;

    if (is8Bit)
        encoder.encodeSpan(Span { string.characters8(), length });
    else
        encoder.encodeSpan(Span { string.characters16(), length });
}
template
void ArgumentCoder<String>::encode<Encoder>(Encoder&, const String&);
template
void ArgumentCoder<String>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, const String&);

template<typename CharacterType, typename Decoder>
static inline std::optional<String> decodeStringText(Decoder& decoder, unsigned length)
{
    auto data = decoder.template decodeSpan<CharacterType>(length);
    if (!data.data())
        return std::nullopt;
    return std::make_optional<String>(data);
}

template<typename Decoder>
WARN_UNUSED_RETURN std::optional<String> ArgumentCoder<String>::decode(Decoder& decoder)
{
    auto length = decoder.template decode<unsigned>();
    if (!length)
        return std::nullopt;
    
    if (*length == std::numeric_limits<unsigned>::max()) {
        // This is the null string.
        return String();
    }

    auto is8Bit = decoder.template decode<bool>();
    if (!is8Bit)
        return std::nullopt;
    
    if (*is8Bit)
        return decodeStringText<LChar>(decoder, *length);
    return decodeStringText<UChar>(decoder, *length);
}
template
std::optional<String> ArgumentCoder<String>::decode<Decoder>(Decoder&);

template<typename Encoder>
void ArgumentCoder<StringView>::encode(Encoder& encoder, StringView string)
{
    // Special case the null string.
    if (string.isNull()) {
        encoder << std::numeric_limits<uint32_t>::max();
        return;
    }

    unsigned length = string.length();
    bool is8Bit = string.is8Bit();

    encoder << length << is8Bit;

    if (is8Bit)
        encoder.encodeSpan(string.span8());
    else
        encoder.encodeSpan(string.span16());
}
template
void ArgumentCoder<StringView>::encode<Encoder>(Encoder&, StringView);
template
void ArgumentCoder<StringView>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, StringView);

#if HAVE(AUDIT_TOKEN)

void ArgumentCoder<audit_token_t>::encode(Encoder& encoder, const audit_token_t& auditToken)
{
    for (unsigned i = 0; i < std::size(auditToken.val); i++)
        encoder << auditToken.val[i];
}

WARN_UNUSED_RETURN bool ArgumentCoder<audit_token_t>::decode(Decoder& decoder, audit_token_t& auditToken)
{
    for (unsigned i = 0; i < std::size(auditToken.val); i++) {
        if (!decoder.decode(auditToken.val[i]))
            return false;
    }
    return true;
}

#endif // HAVE(AUDIT_TOKEN)

} // namespace IPC
