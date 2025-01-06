/*
 * Copyright (C) 2015-2023 Apple, Inc.  All rights reserved.
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

#import "config.h"
#import "Cookie.h"
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

// FIXME: Remove NS_ASSUME_NONNULL_BEGIN/END and all _Nullable annotations once we remove the NSHTTPCookie forward declaration below.
NS_ASSUME_NONNULL_BEGIN

namespace WebCore {

static Vector<uint16_t> portVectorFromList(NSArray<NSNumber *> *portList)
{
    return Vector<uint16_t>(portList.count, [portList](size_t i) {
        NSNumber *port = portList[i];
        return port.unsignedShortValue;
    });
}

static NSString * _Nullable portStringFromVector(const Vector<uint16_t>& ports)
{
    if (ports.isEmpty())
        return nil;

    auto *string = [NSMutableString stringWithCapacity:ports.size() * 5];

    for (size_t i = 0; i < ports.size() - 1; ++i)
        [string appendFormat:@"%" PRIu16 ", ", ports[i]];

    [string appendFormat:@"%" PRIu16, ports.last()];

    return string;
}

static double cookieCreated(NSHTTPCookie *cookie)
{
    id value = cookie.properties[@"Created"];

    auto toCanonicalFormat = [](double referenceFormat) {
        return 1000.0 * (referenceFormat + NSTimeIntervalSince1970);
    };

    if (auto *number = dynamic_objc_cast<NSNumber>(value))
        return toCanonicalFormat(number.doubleValue);

    if (auto *string = dynamic_objc_cast<NSString>(value))
        return toCanonicalFormat(string.doubleValue);

    return 0;
}

static std::optional<double> cookieExpiry(NSHTTPCookie *cookie)
{
    NSDate *expiryDate = cookie.expiresDate;
    if (!expiryDate)
        return std::nullopt;
    return [expiryDate timeIntervalSince1970] * 1000.0;
}

static Cookie::SameSitePolicy coreSameSitePolicy(NSHTTPCookieStringPolicy _Nullable policy)
{
    if (!policy)
        return Cookie::SameSitePolicy::None;
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    if ([policy isEqualToString:NSHTTPCookieSameSiteLax])
        return Cookie::SameSitePolicy::Lax;
    if ([policy isEqualToString:NSHTTPCookieSameSiteStrict])
        return Cookie::SameSitePolicy::Strict;
ALLOW_NEW_API_WITHOUT_GUARDS_END
    ASSERT_NOT_REACHED();
    return Cookie::SameSitePolicy::None;
}

static NSHTTPCookieStringPolicy _Nullable nsSameSitePolicy(Cookie::SameSitePolicy policy)
{
    switch (policy) {
    case Cookie::SameSitePolicy::None:
        return nil;
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    case Cookie::SameSitePolicy::Lax:
        return NSHTTPCookieSameSiteLax;
    case Cookie::SameSitePolicy::Strict:
        return NSHTTPCookieSameSiteStrict;
ALLOW_NEW_API_WITHOUT_GUARDS_END
    }
}

Cookie::Cookie(NSHTTPCookie *cookie)
    : name { cookie.name }
    , value { cookie.value }
    , domain { cookie.domain }
    , path { cookie.path }
    , partitionKey { cookie._storagePartition }
    , created { cookieCreated(cookie) }
    , expires { cookieExpiry(cookie) }
    , httpOnly { static_cast<bool>(cookie.HTTPOnly) }
    , secure { static_cast<bool>(cookie.secure) }
    , session { static_cast<bool>(cookie.sessionOnly) }
    , comment { cookie.comment }
    , commentURL { cookie.commentURL }
    , ports { portVectorFromList(cookie.portList) }
{
    sameSite = coreSameSitePolicy(cookie.sameSitePolicy);
}

Cookie::operator NSHTTPCookie * _Nullable () const
{
    if (isNull())
        return nil;

    NSMutableDictionary *properties = [NSMutableDictionary dictionaryWithCapacity:14];

    if (!comment.isNull())
        [properties setObject:(NSString *)comment forKey:NSHTTPCookieComment];

    if (!commentURL.isNull())
        [properties setObject:(NSURL *)commentURL forKey:NSHTTPCookieCommentURL];

    if (!domain.isNull())
        [properties setObject:(NSString *)domain forKey:NSHTTPCookieDomain];

    if (!name.isNull())
        [properties setObject:(NSString *)name forKey:NSHTTPCookieName];

    if (!path.isNull())
        [properties setObject:(NSString *)path forKey:NSHTTPCookiePath];

    if (!partitionKey.isNull())
        [properties setObject:(NSString *)partitionKey forKey:@"StoragePartition"];

    if (!value.isNull())
        [properties setObject:(NSString *)value forKey:NSHTTPCookieValue];

    if (expires) {
        NSDate *expirationDate = [NSDate dateWithTimeIntervalSince1970:*expires / 1000.0];
        [properties setObject:expirationDate forKey:NSHTTPCookieExpires];
    }

    [properties setObject:@(created / 1000.0 - NSTimeIntervalSince1970) forKey:@"Created"];

    auto* portString = portStringFromVector(ports);
    if (portString)
        [properties setObject:portString forKey:NSHTTPCookiePort];

    if (secure)
        [properties setObject:@YES forKey:NSHTTPCookieSecure];

    if (session)
        [properties setObject:@YES forKey:NSHTTPCookieDiscard];
    
    if (httpOnly)
        [properties setObject:@YES forKey:@"HttpOnly"];

    if (auto* sameSitePolicy = nsSameSitePolicy(sameSite))
        [properties setObject:sameSitePolicy forKey:@"SameSite"];

    [properties setObject:@"1" forKey:NSHTTPCookieVersion];

    return [NSHTTPCookie cookieWithProperties:properties];
}
    
bool Cookie::operator==(const Cookie& other) const
{
    ASSERT(!name.isHashTableDeletedValue());
    bool thisNull = isNull();
    bool otherNull = other.isNull();
    if (thisNull || otherNull)
        return thisNull == otherNull;
    return [static_cast<NSHTTPCookie *>(*this) isEqual:other];
}
    
unsigned Cookie::hash() const
{
    ASSERT(!name.isHashTableDeletedValue());
    ASSERT(!isNull());
    return static_cast<NSHTTPCookie *>(*this).hash;
}

NS_ASSUME_NONNULL_END

} // namespace WebCore
