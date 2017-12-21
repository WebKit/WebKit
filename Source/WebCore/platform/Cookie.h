/*
 * Copyright (C) 2009 Joseph Pecoraro. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "URL.h"
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#ifdef __OBJC__
#include <objc/objc.h>
#endif

namespace WebCore {

struct Cookie {
    Cookie() { }
    
    Cookie(WTF::HashTableDeletedValueType)
        : name(WTF::HashTableDeletedValue)
    { }

    Cookie(const String& name, const String& value, const String& domain, const String& path, double created, double expires, bool httpOnly, bool secure, bool session, const String& comment, const URL& commentURL, const Vector<uint16_t> ports)
        : name(name)
        , value(value)
        , domain(domain)
        , path(path)
        , created(created)
        , expires(expires)
        , httpOnly(httpOnly)
        , secure(secure)
        , session(session)
        , comment(comment)
        , commentURL(commentURL)
        , ports(ports)
    {
    }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<Cookie> decode(Decoder&);

    WEBCORE_EXPORT bool operator==(const Cookie&) const;
    WEBCORE_EXPORT unsigned hash() const;

#ifdef __OBJC__
    WEBCORE_EXPORT Cookie(NSHTTPCookie *);
    WEBCORE_EXPORT operator NSHTTPCookie *() const;
#elif USE(SOUP)
    explicit Cookie(SoupCookie*);
    SoupCookie* toSoupCookie() const;
#endif

    bool isNull() const
    {
        return name.isNull()
        && value.isNull()
        && domain.isNull()
        && path.isNull()
        && created == 0
        && expires == 0
        && !httpOnly
        && !secure
        && !session
        && comment.isNull()
        && commentURL.isNull();
    }
    
    String name;
    String value;
    String domain;
    String path;
    // Creation and expiration dates are expressed as milliseconds since the UNIX epoch.
    double created { 0 };
    double expires { 0 };
    bool httpOnly { false };
    bool secure { false };
    bool session { false };
    String comment;
    URL commentURL;
    Vector<uint16_t> ports;
};

struct CookieHash {
    static unsigned hash(const Cookie& key)
    {
        return key.hash();
    }

    static bool equal(const Cookie& a, const Cookie& b)
    {
        return a == b;
    }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

template<class Encoder>
void Cookie::encode(Encoder& encoder) const
{
    encoder << name << value << domain << path << created << expires << httpOnly << secure << session << comment << commentURL << ports;
}

template<class Decoder>
std::optional<Cookie> Cookie::decode(Decoder& decoder)
{
    std::optional<String> name;
    decoder >> name;
    if (!name)
        return std::nullopt;
    
    std::optional<String> value;
    decoder >> value;
    if (!value)
        return std::nullopt;

    std::optional<String> domain;
    decoder >> domain;
    if (!domain)
        return std::nullopt;

    std::optional<String> path;
    decoder >> path;
    if (!path)
        return std::nullopt;

    std::optional<double> created;
    decoder >> created;
    if (!created)
        return std::nullopt;

    std::optional<double> expires;
    decoder >> expires;
    if (!expires)
        return std::nullopt;

    std::optional<bool> httpOnly;
    decoder >> httpOnly;
    if (!httpOnly)
        return std::nullopt;

    std::optional<bool> secure;
    decoder >> secure;
    if (!secure)
        return std::nullopt;

    std::optional<bool> session;
    decoder >> session;
    if (!session)
        return std::nullopt;

    std::optional<String> comment;
    decoder >> comment;
    if (!comment)
        return std::nullopt;

    URL commentURL;
    if (!decoder.decode(commentURL))
        return std::nullopt;

    std::optional<Vector<uint16_t>> ports;
    decoder >> ports;
    if (!ports)
        return std::nullopt;

    return {{ WTFMove(*name), WTFMove(*value), WTFMove(*domain), WTFMove(*path), WTFMove(*created), WTFMove(*expires), WTFMove(*httpOnly), WTFMove(*secure), WTFMove(*session), WTFMove(*comment), WTFMove(commentURL), WTFMove(*ports) }};
}

}

namespace WTF {
    template<typename T> struct DefaultHash;
    template<> struct DefaultHash<WebCore::Cookie> {
        typedef WebCore::CookieHash Hash;
    };
    template<> struct HashTraits<WebCore::Cookie> : GenericHashTraits<WebCore::Cookie> {
        static WebCore::Cookie emptyValue() { return { }; }
        static void constructDeletedValue(WebCore::Cookie& slot) { slot = WebCore::Cookie(WTF::HashTableDeletedValue); }
        static bool isDeletedValue(const WebCore::Cookie& slot) { return slot.name.isHashTableDeletedValue(); }
    };
}
