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
#import <wtf/NeverDestroyed.h>
#import <wtf/ObjcRuntimeExtras.h>

namespace WebKit {

ApplicationStateTracker& ApplicationStateTracker::singleton()
{
    static NeverDestroyed<ApplicationStateTracker> applicationStateTracker;

    return applicationStateTracker;
}

static bool isViewService()
{
    if (_UIApplicationIsExtension())
        return true;

    if ([[NSBundle mainBundle].bundleIdentifier isEqualToString:@"com.apple.SafariViewService"])
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

ApplicationStateTracker::ApplicationStateTracker()
{
    if (isViewService()) {
        BKSApplicationStateMonitor *applicationStateMonitor = [[BKSApplicationStateMonitor alloc] init];

        m_isInBackground = isBackgroundState([applicationStateMonitor mostElevatedApplicationStateForPID:getpid()]);

        applicationStateMonitor.handler = [this](NSDictionary *userInfo) {
            pid_t pid = [userInfo[BKSApplicationStateProcessIDKey] integerValue];
            if (pid != getpid())
                return;

            BKSApplicationState newState = (BKSApplicationState)[userInfo[BKSApplicationStateMostElevatedStateForProcessIDKey] unsignedIntValue];
            bool newInBackground = isBackgroundState(newState);

            dispatch_async(dispatch_get_main_queue(), [this, newInBackground] {
                if (!m_isInBackground && newInBackground)
                    applicationDidEnterBackground();
                else if (m_isInBackground && !newInBackground)
                    applicationWillEnterForeground();
            });
        };
    } else {
        UIApplication *application = [UIApplication sharedApplication];
        NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];

        m_isInBackground = application.applicationState == UIApplicationStateBackground;

        [notificationCenter addObserverForName:UIApplicationDidEnterBackgroundNotification object:application queue:nil usingBlock:[this](NSNotification *) {
            applicationDidEnterBackground();
        }];

        [notificationCenter addObserverForName:UIApplicationWillEnterForegroundNotification object:application queue:nil usingBlock:[this](NSNotification *) {
            applicationWillEnterForeground();
        }];
    }
}

void ApplicationStateTracker::addListener(id object, SEL willEnterForegroundSelector, SEL didEnterBackgroundSelector)
{
    ASSERT([object respondsToSelector:willEnterForegroundSelector]);
    ASSERT([object respondsToSelector:didEnterBackgroundSelector]);

    m_listeners.append({ object, willEnterForegroundSelector, didEnterBackgroundSelector });
}

void ApplicationStateTracker::applicationDidEnterBackground()
{
    m_isInBackground = true;

    invokeListeners(&Listener::didEnterBackgroundSelector);
}

void ApplicationStateTracker::applicationWillEnterForeground()
{
    m_isInBackground = false;

    invokeListeners(&Listener::willEnterForegroundSelector);
}

void ApplicationStateTracker::invokeListeners(SEL Listener::* selector)
{
    bool shouldPruneListeners = false;

    for (auto& listener : m_listeners) {
        auto object = listener.object.get();
        if (!object) {
            shouldPruneListeners = true;
            continue;
        }

        wtfObjcMsgSend<void>(object.get(), listener.*selector);
    }

    if (shouldPruneListeners)
        pruneListeners();
}

void ApplicationStateTracker::pruneListeners()
{
    auto listeners = WTF::move(m_listeners);
    ASSERT(m_listeners.isEmpty());

    for (auto& listener : listeners) {
        if (!listener.object)
            continue;

        m_listeners.append(WTF::move(listener));
    }
}

}

#endif
