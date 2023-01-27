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

#include <wtf/Hasher.h>
#include <wtf/URL.h>

namespace WebCore {

class Frame;
class SecurityOrigin;

struct SecurityOriginData {
    SecurityOriginData() = default;
    SecurityOriginData(const String& protocol, const String& host, std::optional<uint16_t> port)
        : protocol(protocol)
        , host(host)
        , port(port)
    {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!isHashTableDeletedValue());
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
    std::optional<uint16_t> port;

    WEBCORE_EXPORT SecurityOriginData isolatedCopy() const &;
    WEBCORE_EXPORT SecurityOriginData isolatedCopy() &&;

    // Serialize the security origin to a string that could be used as part of
    // file names. This format should be used in storage APIs only.
    WEBCORE_EXPORT String databaseIdentifier() const;
    WEBCORE_EXPORT static std::optional<SecurityOriginData> fromDatabaseIdentifier(StringView);

    bool isNull() const
    {
        return protocol.isNull() && host.isNull() && port == std::nullopt;
    }
    bool isOpaque() const
    {
        return protocol == emptyString() && host == emptyString() && !port;
    }

    bool isHashTableDeletedValue() const
    {
        return protocol.isHashTableDeletedValue();
    }
    
    WEBCORE_EXPORT String toString() const;

    URL toURL() const;

#if !LOG_DISABLED
    String debugString() const { return toString(); }
#endif
};

WEBCORE_EXPORT bool operator==(const SecurityOriginData&, const SecurityOriginData&);
inline bool operator!=(const SecurityOriginData& first, const SecurityOriginData& second) { return !(first == second); }

inline void add(Hasher& hasher, const SecurityOriginData& data)
{
    add(hasher, data.protocol, data.host, data.port);
}

struct SecurityOriginDataHashTraits : SimpleClassHashTraits<SecurityOriginData> {
    static const bool hasIsEmptyValueFunction = true;
    static const bool emptyValueIsZero = false;
    static bool isEmptyValue(const SecurityOriginData& data) { return data.isNull(); }
};

struct SecurityOriginDataHash {
    static unsigned hash(const SecurityOriginData& data) { return computeHash(data); }
    static bool equal(const SecurityOriginData& a, const SecurityOriginData& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

} // namespace WebCore

namespace WTF {

template<> struct HashTraits<WebCore::SecurityOriginData> : WebCore::SecurityOriginDataHashTraits { };
template<> struct DefaultHash<WebCore::SecurityOriginData> : WebCore::SecurityOriginDataHash { };

} // namespace WTF
