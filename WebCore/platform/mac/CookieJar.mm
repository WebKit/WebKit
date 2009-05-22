/*
 * Copyright (C) 2003, 2006, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "CookieJar.h"

#import "BlockExceptions.h"
#import "Document.h"
#import "KURL.h"
#import <wtf/RetainPtr.h>

#ifdef BUILDING_ON_TIGER
typedef unsigned NSUInteger;
#endif

@interface NSHTTPCookie (WebCoreHTTPOnlyCookies)
- (BOOL)isHTTPOnly;
@end

namespace WebCore {

static bool isHTTPOnly(NSHTTPCookie *cookie)
{
    // Once we require a newer version of Foundation with the isHTTPOnly method,
    // we can eliminate the instancesRespondToSelector: check.
    static bool supportsHTTPOnlyCookies = [NSHTTPCookie instancesRespondToSelector:@selector(isHTTPOnly)];
    return supportsHTTPOnlyCookies && [cookie isHTTPOnly];
}

static RetainPtr<NSArray> filterCookies(NSArray *unfilteredCookies)
{
    NSUInteger count = [unfilteredCookies count];
    RetainPtr<NSMutableArray> filteredCookies(AdoptNS, [[NSMutableArray alloc] initWithCapacity:count]);

    for (NSUInteger i = 0; i < count; ++i) {
        NSHTTPCookie *cookie = (NSHTTPCookie *)[unfilteredCookies objectAtIndex:i];

        // <rdar://problem/5632883> On 10.5, NSHTTPCookieStorage would store an empty cookie,
        // which would be sent as "Cookie: =". We have a workaround in setCookies() to prevent
        // that, but we also need to avoid sending cookies that were previously stored, and
        // there's no harm to doing this check because such a cookie is never valid.
        if (![[cookie name] length])
            continue;

        if (isHTTPOnly(cookie))
            continue;

        [filteredCookies.get() addObject:cookie];
    }

    return filteredCookies;
}

String cookies(const Document*, const KURL& url)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSURL *cookieURL = url;
    NSArray *cookies = [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookiesForURL:cookieURL];
    return [[NSHTTPCookie requestHeaderFieldsWithCookies:filterCookies(cookies).get()] objectForKey:@"Cookie"];

    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

void setCookies(Document* document, const KURL& url, const String& cookieStr)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    // <rdar://problem/5632883> On 10.5, NSHTTPCookieStorage would store an empty cookie,
    // which would be sent as "Cookie: =".
    if (cookieStr.isEmpty())
        return;

    // <http://bugs.webkit.org/show_bug.cgi?id=6531>, <rdar://4409034>
    // cookiesWithResponseHeaderFields doesn't parse cookies without a value
    String cookieString = cookieStr.contains('=') ? cookieStr : cookieStr + "=";

    NSURL *cookieURL = url;    
    NSArray *cookies = [NSHTTPCookie cookiesWithResponseHeaderFields:[NSDictionary dictionaryWithObject:cookieString forKey:@"Set-Cookie"] forURL:cookieURL];
    [[NSHTTPCookieStorage sharedHTTPCookieStorage] setCookies:filterCookies(cookies).get() forURL:cookieURL mainDocumentURL:document->firstPartyForCookies()];

    END_BLOCK_OBJC_EXCEPTIONS;
}

bool cookiesEnabled(const Document*)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSHTTPCookieAcceptPolicy cookieAcceptPolicy = [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookieAcceptPolicy];
    return cookieAcceptPolicy == NSHTTPCookieAcceptPolicyAlways || cookieAcceptPolicy == NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;

    END_BLOCK_OBJC_EXCEPTIONS;
    return false;
}

}
