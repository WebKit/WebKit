/*
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

#import "config.h"
#import "WKHTTPCookieStoreInternal.h"

#import <WebCore/Cookie.h>
#import <WebCore/HTTPCookieAcceptPolicy.h>
#import <WebCore/HTTPCookieAcceptPolicyCocoa.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/URL.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/VectorCocoa.h>

static NSHTTPCookie *clearCookiePartitionPropertyIfNeeded(NSHTTPCookie *cookie, bool isCookiePartitioningEnabled)
{
#if ENABLE(OPT_IN_PARTITIONED_COOKIES) && defined(CFN_COOKIE_ACCEPTS_POLICY_PARTITION) && CFN_COOKIE_ACCEPTS_POLICY_PARTITION
    if (!cookie)
        return cookie;

    if (!isCookiePartitioningEnabled)
        return cookie;

    if (!cookie._storagePartition)
        return cookie;

    auto properties = adoptNS([[cookie properties] mutableCopy]);
    [properties removeObjectForKey:@"StoragePartition"];
    return [NSHTTPCookie cookieWithProperties:properties.get()];
#else
    UNUSED_PARAM(isCookiePartitioningEnabled);
    return cookie;
#endif
}

static NSArray<NSHTTPCookie *> *coreCookiesToNSCookies(const Vector<WebCore::Cookie>& coreCookies, bool isCookiePartitioningEnabled)
{
    return createNSArray(coreCookies, [isCookiePartitioningEnabled] (auto& cookie) -> NSHTTPCookie * {
        return clearCookiePartitionPropertyIfNeeded(cookie.createNSHTTPCookie().autorelease(), isCookiePartitioningEnabled);
    }).autorelease();
}

class WKHTTPCookieStoreObserver : public API::HTTPCookieStoreObserver {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(WKHTTPCookieStoreObserver);
public:
    static Ref<WKHTTPCookieStoreObserver> create(id<WKHTTPCookieStoreObserver> observer)
    {
        return adoptRef(*new WKHTTPCookieStoreObserver(observer));
    }

private:
    explicit WKHTTPCookieStoreObserver(id<WKHTTPCookieStoreObserver> observer)
        : m_observer(observer)
    {
    }

    void cookiesDidChange(API::HTTPCookieStore& cookieStore) final
    {
        RetainPtr observer = m_observer.get();
        if ([observer respondsToSelector:@selector(cookiesDidChangeInCookieStore:)])
            [observer cookiesDidChangeInCookieStore:(RetainPtr { wrapper(cookieStore) }.get())];
    }

    WeakObjCPtr<id<WKHTTPCookieStoreObserver>> m_observer;
};

@implementation WKHTTPCookieStore {
    HashMap<CFTypeRef, RefPtr<WKHTTPCookieStoreObserver>> _observers;
}

WK_OBJECT_DISABLE_DISABLE_KVC_IVAR_ACCESS;

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKHTTPCookieStore.class, self))
        return;

    for (RefPtr observer : _observers.values())
        protect(*_cookieStore)->unregisterObserver(*observer);

    SUPPRESS_UNCOUNTED_ARG _cookieStore->API::HTTPCookieStore::~HTTPCookieStore();

    [super dealloc];
}

- (void)getAllCookies:(void (^)(NSArray<NSHTTPCookie *> *))completionHandler
{
    bool isOptInCookiePartitioningEnabled = protect(*_cookieStore)->isOptInCookiePartitioningEnabled();
    protect(*_cookieStore)->cookies([isOptInCookiePartitioningEnabled, handler = adoptNS([completionHandler copy])](const Vector<WebCore::Cookie>& cookies) {
        auto rawHandler = (void (^)(NSArray<NSHTTPCookie *> *))handler.get();
        rawHandler(coreCookiesToNSCookies(cookies, isOptInCookiePartitioningEnabled));
    });
}

- (void)setCookie:(NSHTTPCookie *)cookie completionHandler:(void (^)(void))completionHandler
{
    protect(*_cookieStore)->setCookies({ cookie }, [handler = adoptNS([completionHandler copy])]() {
        auto rawHandler = (void (^)())handler.get();
        if (rawHandler)
            rawHandler();
    });

}

static std::optional<WebCore::Cookie> makeVectorElement(const WebCore::Cookie*, id arrayElement)
{
    if (NSHTTPCookie *nsCookie = dynamic_objc_cast<NSHTTPCookie>(arrayElement))
        return WebCore::Cookie { nsCookie };

    return std::nullopt;
}

- (void)setCookies:(NSArray<NSHTTPCookie *> *)cookies completionHandler:(void (^)(void))completionHandler
{
    protect(*_cookieStore)->setCookies(makeVector<WebCore::Cookie>(cookies), [completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
    });
}

- (void)deleteCookie:(NSHTTPCookie *)cookie completionHandler:(void (^)(void))completionHandler
{
    protect(*_cookieStore)->deleteCookie(cookie, [handler = adoptNS([completionHandler copy])]() {
        auto rawHandler = (void (^)())handler.get();
        if (rawHandler)
            rawHandler();
    });
}

- (void)addObserver:(id<WKHTTPCookieStoreObserver>)observer
{
    auto result = _observers.add((__bridge CFTypeRef)observer, nullptr);
    if (!result.isNewEntry)
        return;

    result.iterator->value = WKHTTPCookieStoreObserver::create(observer);
    protect(*_cookieStore)->registerObserver(Ref { *result.iterator->value }.get());
}

- (void)removeObserver:(id<WKHTTPCookieStoreObserver>)observer
{
    auto result = _observers.take((__bridge CFTypeRef)observer);
    if (!result)
        return;

    protect(*_cookieStore)->unregisterObserver(*result);
}

static WebCore::HTTPCookieAcceptPolicy NODELETE toHTTPCookieAcceptPolicy(WKCookiePolicy wkCookiePolicy)
{
    switch (wkCookiePolicy) {
    case WKCookiePolicyAllow:
        return WebCore::HTTPCookieAcceptPolicy::OnlyFromMainDocumentDomain;
    case WKCookiePolicyDisallow:
        return WebCore::HTTPCookieAcceptPolicy::Never;
    }
    ASSERT_NOT_REACHED();
}

static WKCookiePolicy NODELETE toWKCookiePolicy(WebCore::HTTPCookieAcceptPolicy policy)
{
    switch (policy) {
    case WebCore::HTTPCookieAcceptPolicy::OnlyFromMainDocumentDomain:
        return WKCookiePolicyAllow;
    case WebCore::HTTPCookieAcceptPolicy::Never:
        return WKCookiePolicyDisallow;
    case WebCore::HTTPCookieAcceptPolicy::AlwaysAccept:
    case WebCore::HTTPCookieAcceptPolicy::ExclusivelyFromMainDocumentDomain:
        return WKCookiePolicyAllow;
    }
}

- (void)setCookiePolicy:(WKCookiePolicy)policy completionHandler:(void (^)(void))completionHandler
{
    protect(*_cookieStore)->setHTTPCookieAcceptPolicy(toHTTPCookieAcceptPolicy(policy), [completionHandler = makeBlockPtr(completionHandler)] {
        if (completionHandler)
            completionHandler.get()();
    });
}

- (void)getCookiePolicy:(void (^)(WKCookiePolicy))completionHandler
{
    protect(*_cookieStore)->getHTTPCookieAcceptPolicy([completionHandler = makeBlockPtr(completionHandler)] (WebCore::HTTPCookieAcceptPolicy policy) {
        completionHandler(toWKCookiePolicy(policy));
    });
}

- (void)getCookiesForURL:(NSURL *)url completionHandler:(void (^)(NSArray<NSHTTPCookie *> *))completionHandler
{
    bool isOptInCookiePartitioningEnabled = protect(*_cookieStore)->isOptInCookiePartitioningEnabled();
    protect(*_cookieStore)->cookiesForURL(url, [isOptInCookiePartitioningEnabled, handler = makeBlockPtr(completionHandler)] (const Vector<WebCore::Cookie>& cookies) {
        handler.get()(coreCookiesToNSCookies(cookies, isOptInCookiePartitioningEnabled));
    });
}
#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_cookieStore;
}

@end

@implementation WKHTTPCookieStore (WKPrivate)

- (void)_getCookiesForURL:(NSURL *)url completionHandler:(void (^)(NSArray<NSHTTPCookie *> *))completionHandler
{
    [self getCookiesForURL:url completionHandler:completionHandler];
}

- (void)_setCookieAcceptPolicy:(NSHTTPCookieAcceptPolicy)policy completionHandler:(void (^)())completionHandler
{
    protect(*_cookieStore)->setHTTPCookieAcceptPolicy(WebCore::toHTTPCookieAcceptPolicy(policy), [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler.get()();
    });
}

- (void)_flushCookiesToDiskWithCompletionHandler:(void(^)(void))completionHandler
{
    protect(*_cookieStore)->flushCookies([completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
    });
}

#if !TARGET_OS_IPHONE

- (void)_setCookies:(NSArray<NSHTTPCookie *> *)cookies completionHandler:(void (^)(void))completionHandler
{
    protect(*_cookieStore)->setCookies(makeVector<WebCore::Cookie>(cookies), [completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
    });
}

#endif

@end
