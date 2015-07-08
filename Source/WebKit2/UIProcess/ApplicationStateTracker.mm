/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#import "ApplicationStateTracker.h"

#if PLATFORM(IOS)

#import "AssertionServicesSPI.h"
#import "UIKitSPI.h"
#import <UIKit/UIApplication.h>
#import <WebCore/SecuritySPI.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/ObjcRuntimeExtras.h>

namespace WebKit {

static bool hasEntitlement(NSString *entitlement)
{
#if PLATFORM(IOS_SIMULATOR)
    // The simulator doesn't support entitlements.
    return true;
#else
    auto task = adoptCF(SecTaskCreateFromSelf(CFAllocatorGetDefault()));
    if (!task)
        return false;

    auto value = adoptCF(SecTaskCopyValueForEntitlement(task.get(), (__bridge CFStringRef)entitlement, nullptr));
    if (!value)
        return false;

    if (CFGetTypeID(value.get()) != CFBooleanGetTypeID())
        return false;

    return CFBooleanGetValue(static_cast<CFBooleanRef>(value.get()));
#endif
}

static bool isViewService()
{
    if (_UIApplicationIsExtension())
        return true;

    if (hasEntitlement(@"com.apple.UIKit.vends-view-services"))
        return true;

    return false;
}

static bool isBackgroundState(BKSApplicationState state)
{
    switch (state) {
    case BKSApplicationStateBackgroundRunning:
    case BKSApplicationStateBackgroundTaskSuspended:
        return true;

    default:
        return false;
    }
}

ApplicationStateTracker::ApplicationStateTracker(UIView *view, SEL didEnterBackgroundSelector, SEL willEnterForegroundSelector)
    : m_view(view)
    , m_didEnterBackgroundSelector(didEnterBackgroundSelector)
    , m_willEnterForegroundSelector(willEnterForegroundSelector)
    , m_weakPtrFactory(this)
    , m_didEnterBackgroundObserver(nullptr)
    , m_willEnterForegroundObserver(nullptr)
{
    ASSERT([m_view.get() respondsToSelector:m_didEnterBackgroundSelector]);
    ASSERT([m_view.get() respondsToSelector:m_willEnterForegroundSelector]);

    if (isViewService()) {
        m_applicationStateMonitor = adoptNS([[BKSApplicationStateMonitor alloc] init]);

        m_isInBackground = isBackgroundState([m_applicationStateMonitor mostElevatedApplicationStateForPID:getpid()]);

        auto weakThis = m_weakPtrFactory.createWeakPtr();
        [m_applicationStateMonitor setHandler:[weakThis](NSDictionary *userInfo) {
            pid_t pid = [userInfo[BKSApplicationStateProcessIDKey] integerValue];
            if (pid != getpid())
                return;

            BKSApplicationState newState = (BKSApplicationState)[userInfo[BKSApplicationStateMostElevatedStateForProcessIDKey] unsignedIntValue];
            bool newInBackground = isBackgroundState(newState);

            dispatch_async(dispatch_get_main_queue(), [weakThis, newInBackground] {
                auto applicationStateTracker = weakThis.get();
                if (!applicationStateTracker)
                    return;

                if (!applicationStateTracker->m_isInBackground && newInBackground)
                    applicationStateTracker->applicationDidEnterBackground();
                else if (applicationStateTracker->m_isInBackground && !newInBackground)
                    applicationStateTracker->applicationWillEnterForeground();
            });
        }];
    } else {
        UIApplication *application = [UIApplication sharedApplication];
        NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];

        m_isInBackground = application.applicationState == UIApplicationStateBackground;

        m_didEnterBackgroundObserver = [notificationCenter addObserverForName:UIApplicationDidEnterBackgroundNotification object:application queue:nil usingBlock:[this](NSNotification *) {
            applicationDidEnterBackground();
        }];

        m_willEnterForegroundObserver = [notificationCenter addObserverForName:UIApplicationWillEnterForegroundNotification object:application queue:nil usingBlock:[this](NSNotification *) {
            applicationWillEnterForeground();
        }];

    }
}

ApplicationStateTracker::~ApplicationStateTracker()
{
    if (m_applicationStateMonitor) {
        [m_applicationStateMonitor invalidate];
        return;
    }

    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter removeObserver:m_didEnterBackgroundObserver];
    [notificationCenter removeObserver:m_willEnterForegroundObserver];
}

void ApplicationStateTracker::applicationDidEnterBackground()
{
    m_isInBackground = true;

    if (auto view = m_view.get())
        wtfObjcMsgSend<void>(view.get(), m_didEnterBackgroundSelector);
}

void ApplicationStateTracker::applicationWillEnterForeground()
{
    m_isInBackground = false;

    if (auto view = m_view.get())
        wtfObjcMsgSend<void>(view.get(), m_willEnterForegroundSelector);
}

}

#endif
