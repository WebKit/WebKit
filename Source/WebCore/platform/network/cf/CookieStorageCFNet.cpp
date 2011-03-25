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
#include "CookieStorageCFNet.h"

#if USE(CFNETWORK)

#include "LoaderRunLoopCF.h"
#include "ResourceHandle.h"
#include <CFNetwork/CFHTTPCookiesPriv.h>
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#include <wtf/MainThread.h>
#include <wtf/RetainPtr.h>

#if USE(PLATFORM_STRATEGIES)
#include "CookiesStrategy.h"
#include "PlatformStrategies.h"
#endif

namespace WebCore {

static RetainPtr<CFHTTPCookieStorageRef>& privateCookieStorage()
{
    DEFINE_STATIC_LOCAL(RetainPtr<CFHTTPCookieStorageRef>, cookieStorage, ());
    return cookieStorage;
}

CFHTTPCookieStorageRef currentCookieStorage()
{
    ASSERT(isMainThread());

    if (CFHTTPCookieStorageRef cookieStorage = privateCookieStorage().get())
        return cookieStorage;
    return wkGetDefaultHTTPCookieStorage();
}

CFHTTPCookieStorageRef privateBrowsingCookieStorage()
{
    return privateCookieStorage().get();
}

void setCurrentCookieStorage(CFHTTPCookieStorageRef cookieStorage)
{
    ASSERT(isMainThread());

    privateCookieStorage().adoptCF(cookieStorage);
}

void setCookieStoragePrivateBrowsingEnabled(bool enabled)
{
    ASSERT(isMainThread());

    if (!enabled) {
        privateCookieStorage() = nullptr;
        return;
    }

#if USE(CFURLSTORAGESESSIONS)
    if (CFURLStorageSessionRef privateStorageSession = ResourceHandle::privateBrowsingStorageSession())
        privateCookieStorage().adoptCF(wkCopyHTTPCookieStorage(privateStorageSession));
    else
#endif
        privateCookieStorage().adoptCF(wkCreateInMemoryHTTPCookieStorage());
}

CFHTTPCookieStorageRef defaultCookieStorage()
{
    return wkGetDefaultHTTPCookieStorage();
}

static void notifyCookiesChangedOnMainThread(void* context)
{
    ASSERT(isMainThread());

#if USE(PLATFORM_STRATEGIES)
    platformStrategies()->cookiesStrategy()->notifyCookiesChanged();
#endif
}

static void notifyCookiesChanged(CFHTTPCookieStorageRef inStorage, void *context)
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

    CFHTTPCookieStorageRef cookieStorage = currentCookieStorage();
    ASSERT(cookieStorage);

    CFHTTPCookieStorageScheduleWithRunLoop(cookieStorage, runLoop, kCFRunLoopCommonModes);
    CFHTTPCookieStorageAddObserver(cookieStorage, runLoop, kCFRunLoopDefaultMode, notifyCookiesChanged, 0);
}

void stopObservingCookieChanges()
{
    ASSERT(isMainThread());

    CFRunLoopRef runLoop = cookieStorageObserverRunLoop();
    ASSERT(runLoop);

    CFHTTPCookieStorageRef cookieStorage = currentCookieStorage();
    ASSERT(cookieStorage);

    CFHTTPCookieStorageRemoveObserver(cookieStorage, runLoop, kCFRunLoopDefaultMode, notifyCookiesChanged, 0);
    CFHTTPCookieStorageUnscheduleFromRunLoop(cookieStorage, runLoop, kCFRunLoopCommonModes);
}

} // namespace WebCore

#endif // USE(CFNETWORK)
