/*
 * Copyright (C) 2009 Joseph Pecoraro. All rights reserved.
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

    Cookie(const String& name, const String& value, const String& domain, const String& path, double expires, bool httpOnly, bool secure, bool session, const String& comment, const URL& commentURL, const Vector<uint16_t> ports)
        : name(name)
        , value(value)
        , domain(domain)
        , path(path)
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
    template<class Decoder> static bool decode(Decoder&, Cookie&);

#ifdef __OBJC__
    WEBCORE_EXPORT Cookie(NSHTTPCookie *);
    WEBCORE_EXPORT operator NSHTTPCookie *() const;
#endif

    String name;
    String value;
    String domain;
    String path;
    // Expiration date, expressed as milliseconds since the UNIX epoch.
    double expires;
    bool httpOnly;
    bool secure;
    bool session;
    String comment;
    URL commentURL;
    Vector<uint16_t> ports;

};

struct CookieHash {
    static unsigned hash(const Cookie& key)
    { 
        return StringHash::hash(key.name) + StringHash::hash(key.domain) + StringHash::hash(key.path) + key.secure;
    }

    static bool equal(const Cookie& a, const Cookie& b)
    {
        return a.name == b.name && a.domain == b.domain && a.path == b.path && a.secure == b.secure;
    }
};

template<class Encoder>
void Cookie::encode(Encoder& encoder) const
{
    encoder << name << value << domain << path << expires << httpOnly << secure << session << comment << commentURL << ports;
}

template<class Decoder>
bool Cookie::decode(Decoder& decoder, Cookie& cookie)
{
    if (!decoder.decode(cookie.name))
        return false;
    if (!decoder.decode(cookie.value))
        return false;
    if (!decoder.decode(cookie.domain))
        return false;
    if (!decoder.decode(cookie.path))
        return false;
    if (!decoder.decode(cookie.expires))
        return false;
    if (!decoder.decode(cookie.httpOnly))
        return false;
    if (!decoder.decode(cookie.secure))
        return false;
    if (!decoder.decode(cookie.session))
        return false;
    if (!decoder.decode(cookie.comment))
        return false;
    if (!decoder.decode(cookie.commentURL))
        return false;
    if (!decoder.decode(cookie.ports))
        return false;

    return true;
}

}

namespace WTF {
    template<typename T> struct DefaultHash;
    template<> struct DefaultHash<WebCore::Cookie> {
        typedef WebCore::CookieHash Hash;
    };
}
