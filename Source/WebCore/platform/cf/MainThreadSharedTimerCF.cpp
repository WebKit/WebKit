/*
 * Copyright (C) 2006-2016 Apple Inc. All rights reserved.
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
#import "MainThreadSharedTimer.h"

#include <wtf/AutodrainedPool.h>

#if PLATFORM(MAC)
#import "PowerObserverMac.h"
#elif PLATFORM(IOS_FAMILY)
#import "WebCoreThreadInternal.h"
#import "WebCoreThreadRun.h"
#endif

namespace WebCore {

static CFRunLoopTimerRef sharedTimer;
static void timerFired(CFRunLoopTimerRef, void*);
static void restartSharedTimer();

static const CFTimeInterval kCFTimeIntervalDistantFuture = std::numeric_limits<CFTimeInterval>::max();

#if PLATFORM(IOS_FAMILY)
static void applicationDidBecomeActive(CFNotificationCenterRef, void*, CFStringRef, const void*, CFDictionaryRef)
{
    WebThreadRun(^{
        restartSharedTimer();
    });
}
#endif

static void setupPowerObserver()
{
#if PLATFORM(MAC)
    static PowerObserver* powerObserver;
    if (!powerObserver)
        powerObserver = makeUnique<PowerObserver>(restartSharedTimer).release();
#elif PLATFORM(IOS_FAMILY)
    static bool registeredForApplicationNotification = false;
    if (!registeredForApplicationNotification) {
        registeredForApplicationNotification = true;
        CFNotificationCenterRef notificationCenter = CFNotificationCenterGetLocalCenter();
        CFNotificationCenterAddObserver(notificationCenter, nullptr, applicationDidBecomeActive, CFSTR("UIApplicationDidBecomeActiveNotification"), nullptr, CFNotificationSuspensionBehaviorCoalesce);
    }
#endif
}

static void timerFired(CFRunLoopTimerRef, void*)
{
    AutodrainedPool pool;
    MainThreadSharedTimer::singleton().fired();
}

static void restartSharedTimer()
{
    if (!sharedTimer)
        return;

    MainThreadSharedTimer::singleton().stop();
    timerFired(0, 0);
}

void MainThreadSharedTimer::invalidate()
{
    if (!sharedTimer)
        return;

    CFRunLoopTimerInvalidate(sharedTimer);
    CFRelease(sharedTimer);
    sharedTimer = nullptr;
}

void MainThreadSharedTimer::setFireInterval(Seconds interval)
{
    ASSERT(m_firedFunction);

    CFAbsoluteTime fireDate = CFAbsoluteTimeGetCurrent() + interval.value();
    if (!sharedTimer) {
        sharedTimer = CFRunLoopTimerCreate(nullptr, fireDate, kCFTimeIntervalDistantFuture, 0, 0, timerFired, nullptr);
#if PLATFORM(IOS_FAMILY)
        CFRunLoopAddTimer(WebThreadRunLoop(), sharedTimer, kCFRunLoopCommonModes);
#else
        CFRunLoopAddTimer(CFRunLoopGetCurrent(), sharedTimer, kCFRunLoopCommonModes);
#endif

        setupPowerObserver();

        return;
    }

    CFRunLoopTimerSetNextFireDate(sharedTimer, fireDate);
}

void MainThreadSharedTimer::stop()
{
    if (!sharedTimer)
        return;

    CFRunLoopTimerSetNextFireDate(sharedTimer, kCFTimeIntervalDistantFuture);
}

} // namespace WebCore
