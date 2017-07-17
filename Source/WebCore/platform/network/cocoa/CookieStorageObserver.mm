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
#import "CookieStorageObserver.h"

#import "NSURLConnectionSPI.h"
#import <wtf/MainThread.h>

namespace WebCore {

static void cookiesChanged(CFHTTPCookieStorageRef, void* context)
{
    ASSERT(!isMainThread());
    static_cast<CookieStorageObserver*>(context)->cookiesDidChange();
}

RefPtr<CookieStorageObserver> CookieStorageObserver::create(CFHTTPCookieStorageRef cookieStorage)
{
    return adoptRef(new CookieStorageObserver(cookieStorage));
}

CookieStorageObserver::CookieStorageObserver(CFHTTPCookieStorageRef cookieStorage)
    : m_cookieStorage(cookieStorage)
{
    ASSERT(isMainThread());
    ASSERT(m_cookieStorage);
}

CookieStorageObserver::~CookieStorageObserver()
{
    ASSERT(isMainThread());

    if (m_cookieChangeCallback)
        stopObserving();
}

void CookieStorageObserver::startObserving(WTF::Function<void()>&& callback)
{
    ASSERT(isMainThread());
    ASSERT(!m_cookieChangeCallback);
    m_cookieChangeCallback = WTFMove(callback);

    CFHTTPCookieStorageAddObserver(m_cookieStorage.get(), [NSURLConnection resourceLoaderRunLoop], kCFRunLoopCommonModes, cookiesChanged, this);
    CFHTTPCookieStorageScheduleWithRunLoop(m_cookieStorage.get(), [NSURLConnection resourceLoaderRunLoop], kCFRunLoopCommonModes);
}

void CookieStorageObserver::stopObserving()
{
    ASSERT(isMainThread());
    ASSERT(m_cookieChangeCallback);
    m_cookieChangeCallback = nullptr;

    CFHTTPCookieStorageRemoveObserver(m_cookieStorage.get(), [NSURLConnection resourceLoaderRunLoop], kCFRunLoopCommonModes, cookiesChanged, this);
}

void CookieStorageObserver::cookiesDidChange()
{
    callOnMainThread([protectedThis = makeRef(*this), this] {
        if (m_cookieChangeCallback)
            m_cookieChangeCallback();
    });
}

} // namespace WebCore
