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

#include "config.h"
#include "CookieStorage.h"

#include "LoaderRunLoopCF.h"
#include "NetworkStorageSession.h"
#include <CFNetwork/CFHTTPCookiesPriv.h>
#include <pal/spi/win/CFNetworkSPIWin.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static HashMap<CFHTTPCookieStorageRef, WTF::Function<void ()>>& cookieChangeCallbackMap()
{
    static NeverDestroyed<HashMap<CFHTTPCookieStorageRef, WTF::Function<void ()>>> map;
    return map;
}

static void notifyCookiesChanged(CFHTTPCookieStorageRef cookieStorage, void *)
{
    callOnMainThread([cookieStorage] {
        auto it = cookieChangeCallbackMap().find(cookieStorage);
        if (it != cookieChangeCallbackMap().end())
            it->value();
    });
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

void startObservingCookieChanges(const NetworkStorageSession& storageSession, WTF::Function<void ()>&& callback)
{
    ASSERT(isMainThread());

    CFRunLoopRef runLoop = cookieStorageObserverRunLoop();
    ASSERT(runLoop);

    RetainPtr<CFHTTPCookieStorageRef> cookieStorage = storageSession.cookieStorage();
    ASSERT(cookieStorage);

    ASSERT(cookieChangeCallbackMap().contains(cookieStorage.get()));
    cookieChangeCallbackMap().add(cookieStorage.get(), WTFMove(callback));

    CFHTTPCookieStorageScheduleWithRunLoop(cookieStorage.get(), runLoop, kCFRunLoopCommonModes);
    CFHTTPCookieStorageAddObserver(cookieStorage.get(), runLoop, kCFRunLoopDefaultMode, notifyCookiesChanged, 0);
}

void stopObservingCookieChanges(const NetworkStorageSession& storageSession)
{
    ASSERT(isMainThread());

    CFRunLoopRef runLoop = cookieStorageObserverRunLoop();
    ASSERT(runLoop);

    RetainPtr<CFHTTPCookieStorageRef> cookieStorage = storageSession.cookieStorage();
    ASSERT(cookieStorage);

    cookieChangeCallbackMap().remove(cookieStorage.get());

    CFHTTPCookieStorageRemoveObserver(cookieStorage.get(), runLoop, kCFRunLoopDefaultMode, notifyCookiesChanged, 0);
    CFHTTPCookieStorageUnscheduleFromRunLoop(cookieStorage.get(), runLoop, kCFRunLoopCommonModes);
}

} // namespace WebCore
