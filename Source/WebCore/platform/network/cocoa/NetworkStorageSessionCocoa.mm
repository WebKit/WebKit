/*
 * Copyright (C) 2015-2018 Apple Inc.  All rights reserved.
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
#import "NetworkStorageSession.h"

#import "Cookie.h"
#import "CookieStorageObserver.h"
#import "URL.h"
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/ProcessPrivilege.h>

namespace WebCore {

void NetworkStorageSession::setCookie(const Cookie& cookie)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [nsCookieStorage() setCookie:(NSHTTPCookie *)cookie];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void NetworkStorageSession::setCookies(const Vector<Cookie>& cookies, const URL& url, const URL& mainDocumentURL)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    RetainPtr<NSMutableArray> nsCookies = adoptNS([[NSMutableArray alloc] initWithCapacity:cookies.size()]);
    for (const auto& cookie : cookies)
        [nsCookies addObject:(NSHTTPCookie *)cookie];

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [nsCookieStorage() setCookies:nsCookies.get() forURL:(NSURL *)url mainDocumentURL:(NSURL *)mainDocumentURL];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void NetworkStorageSession::deleteCookie(const Cookie& cookie)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    [nsCookieStorage() deleteCookie:(NSHTTPCookie *)cookie];
}

static Vector<Cookie> nsCookiesToCookieVector(NSArray<NSHTTPCookie *> *nsCookies)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    Vector<Cookie> cookies;
    cookies.reserveInitialCapacity(nsCookies.count);
    for (NSHTTPCookie *nsCookie in nsCookies)
        cookies.uncheckedAppend(nsCookie);

    return cookies;
}

Vector<Cookie> NetworkStorageSession::getAllCookies()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    return nsCookiesToCookieVector(nsCookieStorage().cookies);
}

Vector<Cookie> NetworkStorageSession::getCookies(const URL& url)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    return nsCookiesToCookieVector([nsCookieStorage() cookiesForURL:(NSURL *)url]);
}

void NetworkStorageSession::flushCookieStore()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    [nsCookieStorage() _saveCookies];
}

NSHTTPCookieStorage *NetworkStorageSession::nsCookieStorage() const
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    auto cfCookieStorage = cookieStorage();
    if (!cfCookieStorage || [NSHTTPCookieStorage sharedHTTPCookieStorage]._cookieStorage == cfCookieStorage)
        return [NSHTTPCookieStorage sharedHTTPCookieStorage];

    return [[[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:cfCookieStorage.get()] autorelease];
}

CookieStorageObserver& NetworkStorageSession::cookieStorageObserver() const
{
    if (!m_cookieStorageObserver)
        m_cookieStorageObserver = CookieStorageObserver::create(nsCookieStorage());

    return *m_cookieStorageObserver;
}

CFURLStorageSessionRef createPrivateStorageSession(CFStringRef identifier)
{
    const void* sessionPropertyKeys[] = { _kCFURLStorageSessionIsPrivate };
    const void* sessionPropertyValues[] = { kCFBooleanTrue };
    auto sessionProperties = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, sessionPropertyKeys, sessionPropertyValues, sizeof(sessionPropertyKeys) / sizeof(*sessionPropertyKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    auto storageSession = adoptCF(_CFURLStorageSessionCreate(kCFAllocatorDefault, identifier, sessionProperties.get()));

    if (!storageSession)
        return nullptr;

    // The private storage session should have the same properties as the default storage session,
    // with the exception that it should be in-memory only storage.

    // FIXME 9199649: If any of the storages do not exist, do no use the storage session.
    // This could occur if there is an issue figuring out where to place a storage on disk (e.g. the
    // sandbox does not allow CFNetwork access).

    auto cache = adoptCF(_CFURLStorageSessionCopyCache(kCFAllocatorDefault, storageSession.get()));
    if (!cache)
        return nullptr;

    CFURLCacheSetDiskCapacity(cache.get(), 0); // Setting disk cache size should not be necessary once <rdar://problem/12656814> is fixed.
    CFURLCacheSetMemoryCapacity(cache.get(), [[NSURLCache sharedURLCache] memoryCapacity]);

    if (!NetworkStorageSession::processMayUseCookieAPI())
        return storageSession.leakRef();

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    auto cookieStorage = adoptCF(_CFURLStorageSessionCopyCookieStorage(kCFAllocatorDefault, storageSession.get()));
    if (!cookieStorage)
        return nullptr;

    // FIXME: Use _CFHTTPCookieStorageGetDefault when USE(CFNETWORK) is defined in WebKit for consistency.
    CFHTTPCookieStorageSetCookieAcceptPolicy(cookieStorage.get(), [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookieAcceptPolicy]);

    return storageSession.leakRef();
}

} // namespace WebCore
