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

#if PLATFORM(IOS_FAMILY)

#import "AssertionServicesSPI.h"
#import "EndowmentStateTracker.h"
#import "Logging.h"
#import "SandboxUtilities.h"
#import "UIKitSPI.h"
#import <wtf/ObjCRuntimeExtras.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/spi/cocoa/SecuritySPI.h>

@interface UIWindow (WKDetails)
- (BOOL)_isHostedInAnotherProcess;
@end

#if HAVE(UISCENE_BASED_VIEW_SERVICE_STATE_NOTIFICATIONS)
static NSNotificationName const viewServiceBackgroundNotificationName = @"_UIViewServiceHostSceneDidEnterBackgroundNotification";
static NSNotificationName const viewServiceForegroundNotificationName = @"_UIViewServiceHostSceneWillEnterForegroundNotification";
#else
static NSNotificationName const viewServiceBackgroundNotificationName = @"_UIViewServiceHostDidEnterBackgroundNotification";
static NSNotificationName const viewServiceForegroundNotificationName = @"_UIViewServiceHostWillEnterForegroundNotification";
#endif

namespace WebKit {

ApplicationType applicationType(UIWindow *window)
{
    if (_UIApplicationIsExtension())
        return ApplicationType::Extension;

    if (WTF::processHasEntitlement("com.apple.UIKit.vends-view-services") && window._isHostedInAnotherProcess)
        return ApplicationType::ViewService;

    return ApplicationType::Application;
}

ApplicationStateTracker::ApplicationStateTracker(UIView *view, SEL didEnterBackgroundSelector, SEL didFinishSnapshottingAfterEnteringBackgroundSelector, SEL willEnterForegroundSelector, SEL willBeginSnapshotSequenceSelector, SEL didCompleteSnapshotSequenceSelector)
    : m_view(view)
    , m_didEnterBackgroundSelector(didEnterBackgroundSelector)
    , m_didFinishSnapshottingAfterEnteringBackgroundSelector(didFinishSnapshottingAfterEnteringBackgroundSelector)
    , m_willEnterForegroundSelector(willEnterForegroundSelector)
    , m_willBeginSnapshotSequenceSelector(willBeginSnapshotSequenceSelector)
    , m_didCompleteSnapshotSequenceSelector(didCompleteSnapshotSequenceSelector)
    , m_isInBackground(true)
    , m_didEnterBackgroundObserver(nullptr)
    , m_didFinishSnapshottingAfterEnteringBackgroundObserver(nullptr)
    , m_willEnterForegroundObserver(nullptr)
{
    ASSERT([m_view.get() respondsToSelector:m_didEnterBackgroundSelector]);
    ASSERT([m_view.get() respondsToSelector:m_didFinishSnapshottingAfterEnteringBackgroundSelector]);
    ASSERT([m_view.get() respondsToSelector:m_willEnterForegroundSelector]);

    UIWindow *window = [m_view.get() window];
    RELEASE_ASSERT(window);

    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    UIApplication *application = [UIApplication sharedApplication];

    auto weakThis = makeWeakPtr(*this);

    m_didFinishSnapshottingAfterEnteringBackgroundObserver = [notificationCenter addObserverForName:@"_UIApplicationDidFinishSuspensionSnapshotNotification" object:application queue:nil usingBlock:[weakThis](NSNotification *) {
        auto applicationStateTracker = weakThis.get();
        if (!applicationStateTracker)
            return;
        applicationStateTracker->applicationDidFinishSnapshottingAfterEnteringBackground();
    }];

    switch (applicationType(window)) {
    case ApplicationType::Application: {
        m_isInBackground = window.windowScene.activationState == UISceneActivationStateBackground || window.windowScene.activationState == UISceneActivationStateUnattached;
        RELEASE_LOG(ViewState, "%p - ApplicationStateTracker::ApplicationStateTracker(): m_isInBackground: %d", this, m_isInBackground);

        m_didEnterBackgroundObserver = [notificationCenter addObserverForName:UISceneDidEnterBackgroundNotification object:nil queue:nil usingBlock:[this](NSNotification *notification) {
            if (notification.object == [[m_view window] windowScene]) {
                RELEASE_LOG(ViewState, "%p - ApplicationStateTracker: UISceneDidEnterBackground", this);
                applicationDidEnterBackground();
            }
        }];

        m_willEnterForegroundObserver = [notificationCenter addObserverForName:UISceneWillEnterForegroundNotification object:nil queue:nil usingBlock:[this](NSNotification *notification) {
            if (notification.object == [[m_view window] windowScene]) {
                RELEASE_LOG(ViewState, "%p - ApplicationStateTracker: UISceneWillEnterForeground", this);
                applicationWillEnterForeground();
            }
        }];

        m_willBeginSnapshotSequenceObserver = [notificationCenter addObserverForName:_UISceneWillBeginSystemSnapshotSequence object:nil queue:nil usingBlock:[this](NSNotification *notification) {
            willBeginSnapshotSequence();
        }];

        m_didCompleteSnapshotSequenceObserver = [notificationCenter addObserverForName:_UISceneDidCompleteSystemSnapshotSequence object:nil queue:nil usingBlock:[this](NSNotification *notification) {
            didCompleteSnapshotSequence();
        }];
        break;
    }

    case ApplicationType::Extension:
    case ApplicationType::ViewService: {
        UIViewController *serviceViewController = nil;

        for (UIView *view = m_view.get().get(); view; view = view.superview) {
            UIViewController *viewController = [UIViewController viewControllerForView:view];

            if (viewController._hostProcessIdentifier) {
                serviceViewController = viewController;
                break;
            }
        }

        ASSERT(serviceViewController);

        pid_t applicationPID = serviceViewController._hostProcessIdentifier;
        ASSERT(applicationPID);

        m_isInBackground = !EndowmentStateTracker::isApplicationForeground(applicationPID);

        // Workaround for <rdar://problem/34028921>. If the host application is StoreKitUIService then it is also a ViewService
        // and is always in the background. We need to treat StoreKitUIService as foreground for the purpose of process suspension
        // or its ViewServices will get suspended.
        if ([serviceViewController._hostApplicationBundleIdentifier isEqualToString:@"com.apple.ios.StoreKitUIService"])
            m_isInBackground = false;

        RELEASE_LOG(ProcessSuspension, "%{public}s has PID %d, host application PID: %d, isInBackground: %d", _UIApplicationIsExtension() ? "Extension" : "ViewService", getpid(), applicationPID, m_isInBackground);

        m_didEnterBackgroundObserver = [notificationCenter addObserverForName:viewServiceBackgroundNotificationName object:serviceViewController queue:nil usingBlock:[this, applicationPID](NSNotification *) {
            RELEASE_LOG(ProcessSuspension, "%{public}s has PID %d, host application PID: %d, didEnterBackground", _UIApplicationIsExtension() ? "Extension" : "ViewService", getpid(), applicationPID);
            applicationDidEnterBackground();
        }];
        m_willEnterForegroundObserver = [notificationCenter addObserverForName:viewServiceForegroundNotificationName object:serviceViewController queue:nil usingBlock:[this, applicationPID](NSNotification *) {
            RELEASE_LOG(ProcessSuspension, "%{public}s has PID %d, host application PID: %d, willEnterForeground", _UIApplicationIsExtension() ? "Extension" : "ViewService", getpid(), applicationPID);
            applicationWillEnterForeground();
        }];

        break;
    }
    }
}

ApplicationStateTracker::~ApplicationStateTracker()
{
    RELEASE_LOG(ViewState, "%p - ~ApplicationStateTracker", this);

    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter removeObserver:m_didEnterBackgroundObserver];
    [notificationCenter removeObserver:m_didFinishSnapshottingAfterEnteringBackgroundObserver];
    [notificationCenter removeObserver:m_willEnterForegroundObserver];
    [notificationCenter removeObserver:m_willBeginSnapshotSequenceObserver];
    [notificationCenter removeObserver:m_didCompleteSnapshotSequenceObserver];
}

void ApplicationStateTracker::applicationDidEnterBackground()
{
    m_isInBackground = true;

    if (auto view = m_view.get())
        wtfObjCMsgSend<void>(view.get(), m_didEnterBackgroundSelector);
}

void ApplicationStateTracker::applicationDidFinishSnapshottingAfterEnteringBackground()
{
    if (auto view = m_view.get())
        wtfObjCMsgSend<void>(view.get(), m_didFinishSnapshottingAfterEnteringBackgroundSelector);
}

void ApplicationStateTracker::applicationWillEnterForeground()
{
    m_isInBackground = false;

    if (auto view = m_view.get())
        wtfObjCMsgSend<void>(view.get(), m_willEnterForegroundSelector);
}

void ApplicationStateTracker::willBeginSnapshotSequence()
{
    RELEASE_LOG(ViewState, "%p - ApplicationStateTracker:willBeginSnapshotSequence()", this);
    if (auto view = m_view.get())
        wtfObjCMsgSend<void>(view.get(), m_willBeginSnapshotSequenceSelector);
}

void ApplicationStateTracker::didCompleteSnapshotSequence()
{
    RELEASE_LOG(ViewState, "%p - ApplicationStateTracker:didCompleteSnapshotSequence()", this);
    if (auto view = m_view.get())
        wtfObjCMsgSend<void>(view.get(), m_didCompleteSnapshotSequenceSelector);
}

}

#endif
