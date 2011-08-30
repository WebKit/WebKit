/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CookieStorage.h"

#include "CookieStorageCFNet.h"

#if USE(CFNETWORK) || USE(CFURLSTORAGESESSIONS)

#include "ResourceHandle.h"
#include <wtf/MainThread.h>

#if PLATFORM(MAC)
#include "WebCoreSystemInterface.h"
#elif PLATFORM(WIN)
#include "LoaderRunLoopCF.h"
#include <CFNetwork/CFHTTPCookiesPriv.h>
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#endif

#if USE(PLATFORM_STRATEGIES)
#include "CookiesStrategy.h"
#include "PlatformStrategies.h"
#endif

#endif

namespace WebCore {

#if PLATFORM(WIN)

static RetainPtr<CFHTTPCookieStorageRef>& cookieStorageOverride()
{
    DEFINE_STATIC_LOCAL(RetainPtr<CFHTTPCookieStorageRef>, cookieStorage, ());
    return cookieStorage;
}

#endif

#if USE(CFNETWORK) || USE(CFURLSTORAGESESSIONS)

RetainPtr<CFHTTPCookieStorageRef> currentCFHTTPCookieStorage()
{
#if PLATFORM(WIN)
    if (RetainPtr<CFHTTPCookieStorageRef>& override = cookieStorageOverride())
        return override;
#endif

#if USE(CFNETWORK) || USE(CFURLSTORAGESESSIONS)
    if (CFURLStorageSessionRef session = ResourceHandle::currentStorageSession())
        return RetainPtr<CFHTTPCookieStorageRef>(AdoptCF, wkCopyHTTPCookieStorage(session));
#endif

#if USE(CFNETWORK)
    return wkGetDefaultHTTPCookieStorage();
#else
    // When using NSURLConnection, we also use its default cookie storage.
    return 0;
#endif
}

#endif // USE(CFNETWORK) || USE(CFURLSTORAGESESSIONS)

#if USE(CFNETWORK) && PLATFORM(WIN)

void overrideCookieStorage(CFHTTPCookieStorageRef cookieStorage)
{
    ASSERT(isMainThread());
    // FIXME: Why don't we retain it? The only caller is an API method that takes cookie storage as a raw argument.
    cookieStorageOverride().adoptCF(cookieStorage);
}

void setCookieStoragePrivateBrowsingEnabled(bool)
{
    ASSERT(isMainThread());

    // Nothing to do here - we'll just use a private session from ResourceHandle.

    // FIXME: When Private Browsing is enabled, the Private Browsing Cookie Storage should be
    // observed for changes, not the default Cookie Storage.
}

static void notifyCookiesChangedOnMainThread(void*)
{
    ASSERT(isMainThread());

#if USE(PLATFORM_STRATEGIES)
    platformStrategies()->cookiesStrategy()->notifyCookiesChanged();
#endif
}

static void notifyCookiesChanged(CFHTTPCookieStorageRef, void *)
{
    callOnMainThread(notifyCookiesChangedOnMainThread, 0);
}

static inline CFRunLoopRef cookieStorageObserverRunLoop()
{
    // We're using the loader run loop because we need a CFRunLoop to 
    // call the CFNetwork cookie storage APIs with. Re-using the loader
    // run loop is less overhead than starting a new thread to just listen
    // for changes in cookies.
    
    // FIXME: The loaderRunLoop function name should be a little more generic.
    return loaderRunLoop();
}

void startObservingCookieChanges()
{
    ASSERT(isMainThread());

    CFRunLoopRef runLoop = cookieStorageObserverRunLoop();
    ASSERT(runLoop);

    RetainPtr<CFHTTPCookieStorageRef> cookieStorage = currentCFHTTPCookieStorage();
    ASSERT(cookieStorage);

    CFHTTPCookieStorageScheduleWithRunLoop(cookieStorage.get(), runLoop, kCFRunLoopCommonModes);
    CFHTTPCookieStorageAddObserver(cookieStorage.get(), runLoop, kCFRunLoopDefaultMode, notifyCookiesChanged, 0);
}

void stopObservingCookieChanges()
{
    ASSERT(isMainThread());

    CFRunLoopRef runLoop = cookieStorageObserverRunLoop();
    ASSERT(runLoop);

    RetainPtr<CFHTTPCookieStorageRef> cookieStorage = currentCFHTTPCookieStorage();
    ASSERT(cookieStorage);

    CFHTTPCookieStorageRemoveObserver(cookieStorage.get(), runLoop, kCFRunLoopDefaultMode, notifyCookiesChanged, 0);
    CFHTTPCookieStorageUnscheduleFromRunLoop(cookieStorage.get(), runLoop, kCFRunLoopCommonModes);
}

#endif // USE(CFNETWORK) && PLATFORM(WIN)

} // namespace WebCore
