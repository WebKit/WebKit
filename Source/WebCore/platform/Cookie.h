/*
 * Copyright (C) 2009 Joseph Pecoraro. All rights reserved.
 * Copyright (C) 2017-2026 Apple Inc. All rights reserved.
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

#include <optional>
#include <wtf/Forward.h>
#include <wtf/RetainPtr.h>
#include <wtf/URL.h>
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

    WEBCORE_EXPORT bool operator==(const Cookie&) const;
    NODELETE WEBCORE_EXPORT unsigned hash() const;

#ifdef __OBJC__
    WEBCORE_EXPORT Cookie(NSHTTPCookie *);
    WEBCORE_EXPORT RetainPtr<NSHTTPCookie> createNSHTTPCookie() const;
#elif USE(SOUP)
    explicit Cookie(SoupCookie*);
    SoupCookie* toSoupCookie() const;
#endif

    bool isNull() const
    {
        return !name
            && !value
            && !domain
            && !path
            && !partitionKey
            && !created
            && !expires
            && !httpOnly
            && !secure
            && !session
            && !comment
            && commentURL.isNull();
    }
    
    bool isKeyEqual(const Cookie& otherCookie) const
    {
        return equal(name, otherCookie.name)
            && equal(domain, otherCookie.domain)
            && equal(path, otherCookie.path);
    }

    RefPtr<StringImpl> name;
    RefPtr<StringImpl> value;
    RefPtr<StringImpl> domain;
    RefPtr<StringImpl> path;
    RefPtr<StringImpl> partitionKey;
    // Creation and expiration dates are expressed as milliseconds since the UNIX epoch.
    double created { 0 };
    std::optional<double> expires;
    bool httpOnly { false };
    bool secure { false };
    bool session { false };
    RefPtr<StringImpl> comment;
    URL commentURL;
    Vector<uint16_t> ports;

    enum class SameSitePolicy : uint8_t { 
        None, 
        Lax, 
        Strict 
    };

    SameSitePolicy sameSite { SameSitePolicy::None };

    Cookie(RefPtr<StringImpl>&& name, RefPtr<StringImpl>&& value, RefPtr<StringImpl>&& domain, RefPtr<StringImpl>&& path, RefPtr<StringImpl>&& partitionKey, double created, std::optional<double> expires, bool httpOnly, bool secure, bool session, RefPtr<StringImpl>&& comment, URL&& commentURL, Vector<uint16_t> ports, SameSitePolicy sameSite)
        : name(WTF::move(name))
        , value(WTF::move(value))
        , domain(WTF::move(domain))
        , path(WTF::move(path))
        , partitionKey(WTF::move(partitionKey))
        , created(created)
        , expires(expires)
        , httpOnly(httpOnly)
        , secure(secure)
        , session(session)
        , comment(WTF::move(comment))
        , commentURL(WTF::move(commentURL))
        , ports(WTF::move(ports))
        , sameSite(sameSite)
    {
    }

    Cookie isolatedCopy() const & { return { WTF::isolatedCopy(name), WTF::isolatedCopy(value), WTF::isolatedCopy(domain), WTF::isolatedCopy(path), WTF::isolatedCopy(partitionKey), created, expires, httpOnly, secure, session, WTF::isolatedCopy(comment), commentURL.isolatedCopy(), ports, sameSite }; }
    Cookie isolatedCopy() && { return { WTF::isolatedCopy(WTF::move(name)), WTF::isolatedCopy(WTF::move(value)), WTF::isolatedCopy(WTF::move(domain)), WTF::isolatedCopy(WTF::move(path)), WTF::isolatedCopy(WTF::move(partitionKey)), created, expires, httpOnly, secure, session, WTF::isolatedCopy(WTF::move(comment)), WTF::move(commentURL).isolatedCopy(), WTF::move(ports), sameSite }; }
};

namespace CookieUtil {

WEBCORE_EXPORT RefPtr<StringImpl> defaultPathForURL(const URL&);

std::optional<RefPtr<StringImpl>> cookieStringWithDayFirstExpires(StringView);

// These functions can reinterpret a cookie string heading from DOM to storage
// or from storage to DOM. Some platforms choose to handle DOM strings differently
// depending on how the data is written to storage.
WEBCORE_EXPORT RefPtr<StringImpl> cookieStringForScript(StringImpl* storedValue);
WEBCORE_EXPORT RefPtr<StringImpl> cookieStringForScript(RefPtr<StringImpl>&& storedValue);
WEBCORE_EXPORT RefPtr<StringImpl> cookieStringForStorage(StringImpl* scriptValue);

} // namespace CookieUtil

} // namespace WebCore

namespace WTF {
    template<> struct HashTraits<WebCore::Cookie> : GenericHashTraits<WebCore::Cookie> {
        static WebCore::Cookie emptyValue() { return { }; }
        static void constructDeletedValue(WebCore::Cookie& slot) { new (NotNull, &slot.name) RefPtr<StringImpl>(WTF::HashTableDeletedValue); }
        static bool isDeletedValue(const WebCore::Cookie& slot) { return slot.name.isHashTableDeletedValue(); }

        static const bool hasIsEmptyValueFunction = true;
        static bool isEmptyValue(const WebCore::Cookie& slot) { return slot.isNull(); }
    };
}
