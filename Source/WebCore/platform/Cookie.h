/*
 * Copyright (C) 2009 Joseph Pecoraro. All rights reserved.
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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

#include <wtf/Optional.h>
#include <wtf/URL.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#ifdef __OBJC__
#include <objc/objc.h>
#endif

#if USE(SOUP)
typedef struct _SoupCookie SoupCookie;
#endif

namespace WebCore {

struct Cookie {
    Cookie() = default;
    Cookie(WTF::HashTableDeletedValueType)
        : name(WTF::HashTableDeletedValue)
    {
    }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Cookie> decode(Decoder&);

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
            && !created
            && !expires
            && !httpOnly
            && !secure
            && !session
            && comment.isNull()
            && commentURL.isNull();
    }
    
    bool isKeyEqual(const Cookie& otherCookie) const
    {
        return name == otherCookie.name
            && domain == otherCookie.domain
            && path == otherCookie.path;
    }

    String name;
    String value;
    String domain;
    String path;
    // Creation and expiration dates are expressed as milliseconds since the UNIX epoch.
    double created { 0 };
    Optional<double> expires;
    bool httpOnly { false };
    bool secure { false };
    bool session { false };
    String comment;
    URL commentURL;
    Vector<uint16_t> ports;

    enum class SameSitePolicy { None, Lax, Strict };
    SameSitePolicy sameSite { SameSitePolicy::None };
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
    encoder << name;
    encoder << value;
    encoder << domain;
    encoder << path;
    encoder << created;
    encoder << expires;
    encoder << httpOnly;
    encoder << secure;
    encoder << session;
    encoder << comment;
    encoder << commentURL;
    encoder << ports;
    encoder << sameSite;
}

template<class Decoder>
Optional<Cookie> Cookie::decode(Decoder& decoder)
{
    Cookie cookie;
    if (!decoder.decode(cookie.name))
        return WTF::nullopt;
    if (!decoder.decode(cookie.value))
        return WTF::nullopt;
    if (!decoder.decode(cookie.domain))
        return WTF::nullopt;
    if (!decoder.decode(cookie.path))
        return WTF::nullopt;
    if (!decoder.decode(cookie.created))
        return WTF::nullopt;
    if (!decoder.decode(cookie.expires))
        return WTF::nullopt;
    if (!decoder.decode(cookie.httpOnly))
        return WTF::nullopt;
    if (!decoder.decode(cookie.secure))
        return WTF::nullopt;
    if (!decoder.decode(cookie.session))
        return WTF::nullopt;
    if (!decoder.decode(cookie.comment))
        return WTF::nullopt;
    if (!decoder.decode(cookie.commentURL))
        return WTF::nullopt;
    if (!decoder.decode(cookie.ports))
        return WTF::nullopt;
    if (!decoder.decode(cookie.sameSite))
        return WTF::nullopt;
    return cookie;
}

}

namespace WTF {
    template<typename T> struct DefaultHash;
    template<> struct DefaultHash<WebCore::Cookie> : WebCore::CookieHash { };
    template<> struct HashTraits<WebCore::Cookie> : GenericHashTraits<WebCore::Cookie> {
        static WebCore::Cookie emptyValue() { return { }; }
        static void constructDeletedValue(WebCore::Cookie& slot) { slot = WebCore::Cookie(WTF::HashTableDeletedValue); }
        static bool isDeletedValue(const WebCore::Cookie& slot) { return slot.name.isHashTableDeletedValue(); }

        static const bool hasIsEmptyValueFunction = true;
        static bool isEmptyValue(const WebCore::Cookie& slot) { return slot.isNull(); }
    };
    template<> struct EnumTraits<WebCore::Cookie::SameSitePolicy> {
    using values = EnumValues<
        WebCore::Cookie::SameSitePolicy,
        WebCore::Cookie::SameSitePolicy::None,
        WebCore::Cookie::SameSitePolicy::Lax,
        WebCore::Cookie::SameSitePolicy::Strict
    >;
};
}
