/*
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
#import "CookieStorageObserver.h"

#import <pal/spi/cocoa/NSURLConnectionSPI.h>
#import <wtf/CheckedPtr.h>
#import <wtf/MainThread.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/TZoneMallocInlines.h>

@interface WKNSHTTPCookieStorageDummyForInternalAccess : NSObject {
@public
    RetainPtr<NSHTTPCookieStorageInternal> _internal;
}
@end

@implementation WKNSHTTPCookieStorageDummyForInternalAccess
@end

@interface NSHTTPCookieStorageInternal : NSObject
- (void)registerForPostingNotificationsWithContext:(NSHTTPCookieStorage *)context;
@end

@interface WKCookieObserverAdapter : NSObject {
    WeakPtr<WebKit::CookieStorageObserver> observer;
}
- (instancetype)initWithObserver:(WebKit::CookieStorageObserver&)theObserver;
- (void)cookiesChangedNotificationHandler:(NSNotification *)notification;

@end

@implementation WKCookieObserverAdapter

- (instancetype)initWithObserver:(WebKit::CookieStorageObserver&)theObserver
{
    self = [super init];
    if (!self)
        return nil;

    observer = theObserver;

    return self;
}

- (void)cookiesChangedNotificationHandler:(NSNotification *)notification
{
    UNUSED_PARAM(notification);
    ensureOnMainThread([weakObserver = observer] {
        if (CheckedPtr observer = weakObserver.get())
            observer->cookiesDidChange();
    });
}

@end

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CookieStorageObserver);

CookieStorageObserver::CookieStorageObserver(NSHTTPCookieStorage *cookieStorage)
    : m_cookieStorage(cookieStorage)
{
    ASSERT(isMainRunLoop());
    ASSERT(m_cookieStorage);
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
}

CookieStorageObserver::~CookieStorageObserver()
{
    ASSERT(isMainRunLoop());

    if (m_cookieChangeCallback) {
        ASSERT(m_observerAdapter);
        stopObserving();
    }
}

void CookieStorageObserver::startObserving(WTF::Function<void()>&& callback)
{
    ASSERT(isMainRunLoop());
    ASSERT(!m_cookieChangeCallback);
    ASSERT(!m_observerAdapter);
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    m_cookieChangeCallback = WTF::move(callback);
    m_observerAdapter = adoptNS([[WKCookieObserverAdapter alloc] initWithObserver:*this]);

    if (!m_hasRegisteredInternalsForNotifications) {
        registerInternalsForNotifications(false);
        m_hasRegisteredInternalsForNotifications = true;
    }

    [[NSNotificationCenter defaultCenter] addObserver:m_observerAdapter.get() selector:@selector(cookiesChangedNotificationHandler:) name:NSHTTPCookieManagerCookiesChangedNotification object:m_cookieStorage.get()];
}

void CookieStorageObserver::registerInternalsForNotifications(bool isReregistering)
{
    // FIXME: rdar://168454473 (Remove workaround in CookieStorageObserver once CFNetwork bug is resolved)
    ASSERT(isMainRunLoop());
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    if (isReregistering && !m_hasRegisteredInternalsForNotifications)
        return;

    if (m_cookieStorage.get() != [NSHTTPCookieStorage sharedHTTPCookieStorage]) {
        RetainPtr internalObject = (static_cast<WKNSHTTPCookieStorageDummyForInternalAccess *>(m_cookieStorage.get()))->_internal;
        [internalObject registerForPostingNotificationsWithContext:m_cookieStorage.get()];
    }
}

void CookieStorageObserver::stopObserving()
{
    ASSERT(isMainRunLoop());
    ASSERT(m_cookieChangeCallback);
    ASSERT(m_observerAdapter);
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    [[NSNotificationCenter defaultCenter] removeObserver:m_observerAdapter.get() name:NSHTTPCookieManagerCookiesChangedNotification object:nil];

    m_cookieChangeCallback = nullptr;
    m_observerAdapter = nil;
}

void CookieStorageObserver::cookiesDidChange()
{
    ASSERT(isMainRunLoop());
    if (m_cookieChangeCallback)
        m_cookieChangeCallback();
}

} // namespace WebKit
