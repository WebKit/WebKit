 /*
 * Copyright (C) 2011, 2015 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/URL.h>

namespace WebCore {

class Frame;
class SecurityOrigin;

struct SecurityOriginData {
    SecurityOriginData() = default;
    SecurityOriginData(const String& protocol, const String& host, Optional<uint16_t> port)
        : protocol(protocol)
        , host(host)
        , port(port)
    {
    }
    SecurityOriginData(WTF::HashTableDeletedValueType)
        : protocol(WTF::HashTableDeletedValue)
    {
    }
    
    WEBCORE_EXPORT static SecurityOriginData fromFrame(Frame*);
    static SecurityOriginData fromURL(const URL& url)
    {
        return SecurityOriginData {
            url.protocol().isNull() ? emptyString() : url.protocol().convertToASCIILowercase(),
            url.host().isNull() ? emptyString() : url.host().convertToASCIILowercase(),
            url.port()
        };
    }

    WEBCORE_EXPORT Ref<SecurityOrigin> securityOrigin() const;

    // FIXME <rdar://9018386>: We should be sending more state across the wire than just the protocol,
    // host, and port.

    String protocol;
    String host;
    Optional<uint16_t> port;

    WEBCORE_EXPORT SecurityOriginData isolatedCopy() const;

    // Serialize the security origin to a string that could be used as part of
    // file names. This format should be used in storage APIs only.
    WEBCORE_EXPORT String databaseIdentifier() const;
    WEBCORE_EXPORT static Optional<SecurityOriginData> fromDatabaseIdentifier(const String&);
    
    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<SecurityOriginData> decode(Decoder&);

    bool isEmpty() const
    {
        return protocol.isNull() && host.isNull() && port == WTF::nullopt;
    }
    
    bool isHashTableDeletedValue() const
    {
        return protocol.isHashTableDeletedValue();
    }
    
    WEBCORE_EXPORT String toString() const;

#if !LOG_DISABLED
    String debugString() const { return toString(); }
#endif
};

WEBCORE_EXPORT bool operator==(const SecurityOriginData&, const SecurityOriginData&);
inline bool operator!=(const SecurityOriginData& first, const SecurityOriginData& second) { return !(first == second); }

template<class Encoder>
void SecurityOriginData::encode(Encoder& encoder) const
{
    encoder << protocol;
    encoder << host;
    encoder << port;
}

template<class Decoder>
Optional<SecurityOriginData> SecurityOriginData::decode(Decoder& decoder)
{
    Optional<String> protocol;
    decoder >> protocol;
    if (!protocol)
        return WTF::nullopt;
    
    Optional<String> host;
    decoder >> host;
    if (!host)
        return WTF::nullopt;
    
    Optional<uint16_t> port;
    if (!decoder.decode(port))
        return WTF::nullopt;
    
    return {{ WTFMove(*protocol), WTFMove(*host), WTFMove(port) }};
}

struct SecurityOriginDataHashTraits : WTF::SimpleClassHashTraits<SecurityOriginData> {
    static const bool hasIsEmptyValueFunction = true;
    static const bool emptyValueIsZero = false;
    static bool isEmptyValue(const SecurityOriginData& data) { return data.isEmpty(); }
};

struct SecurityOriginDataHash {
    static unsigned hash(const SecurityOriginData& data)
    {
        unsigned hashCodes[3] = {
            data.protocol.impl() ? data.protocol.impl()->hash() : 0,
            data.host.impl() ? data.host.impl()->hash() : 0,
            data.port.valueOr(0)
        };
        return StringHasher::hashMemory<sizeof(hashCodes)>(hashCodes);
    }
    static bool equal(const SecurityOriginData& a, const SecurityOriginData& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

} // namespace WebCore

namespace WTF {

template<> struct HashTraits<WebCore::SecurityOriginData> : WebCore::SecurityOriginDataHashTraits { };
template<> struct DefaultHash<WebCore::SecurityOriginData> {
    typedef WebCore::SecurityOriginDataHash Hash;
};

} // namespace WTF
