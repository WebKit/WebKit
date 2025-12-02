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

#import "EndowmentStateTracker.h"
#import "Logging.h"
#import "ProcessAssertion.h"
#import "SandboxUtilities.h"
#import "UIKitSPI.h"
#import <WebCore/UIViewControllerUtilities.h>
#import <algorithm>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/ObjCRuntimeExtras.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/spi/cocoa/SecuritySPI.h>
#import <wtf/text/TextStream.h>


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
void* WKUIWindowSceneObserverContext = &WKUIWindowSceneObserverContext;
}

@interface WKUIWindowSceneObserver : NSObject {
    WeakPtr<WebKit::ApplicationStateTracker> _parent;
    WeakObjCPtr<UIWindow> _window;
}
- (id)initWithParent:(WebKit::ApplicationStateTracker&)parent;
- (void)setObservedWindow:(UIWindow *)window;
@end

@implementation WKUIWindowSceneObserver

- (id)initWithParent:(WebKit::ApplicationStateTracker&)parent
{
    self = [super init];
    if (!self)
        return nil;

    _parent = parent;
    return self;
}

- (void)setObservedWindow:(UIWindow *)window
{
    if (!linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ApplicationStateTrackerDoesNotObserveWindow))
        return;

    RetainPtr newWindow = window;
    RetainPtr oldWindow = _window.get();
    if (oldWindow == newWindow)
        return;

    if (oldWindow) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        // -removeObserver:forKeyPath: will throw an exception if this
        // object wasn't registered as an observer of _window at
        // this keyPath.
        [oldWindow removeObserver:self forKeyPath:@"windowScene"];
        END_BLOCK_OBJC_EXCEPTIONS
    }

    _window = newWindow.get();

    if (newWindow)
        [newWindow addObserver:self forKeyPath:@"windowScene" options:NSKeyValueObservingOptionNew context:WebKit::WKUIWindowSceneObserverContext];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSKeyValueChangeKey, id> *)change context:(void *)context
{
    if (context != WebKit::WKUIWindowSceneObserverContext)
        return;

    ensureOnMainThread([self, protectedSelf = RetainPtr { self }, change = RetainPtr { change }] {
        if (!_parent)
            return;

        id scene = [change valueForKey:NSKeyValueChangeNewKey];
        if ([scene isKindOfClass:UIWindowScene.class])
            _parent->setScene((UIWindowScene *)scene);
        else
            _parent->setScene(nil);
    });
}
@end

namespace WebKit {

static WeakHashSet<ApplicationStateTracker>& allApplicationStateTrackers()
{
    static NeverDestroyed<WeakHashSet<ApplicationStateTracker>> trackers;
    return trackers;
}

static void updateApplicationBackgroundState()
{
    static bool s_isApplicationInBackground = false;
    auto isAnyStateTrackerInForeground = []() -> bool {
        return std::ranges::any_of(allApplicationStateTrackers(), [](auto& tracker) {
            return !tracker.isInBackground();
        });
    };
    bool isApplicationInBackground = !isAnyStateTrackerInForeground();
    if (s_isApplicationInBackground == isApplicationInBackground)
        return;

    s_isApplicationInBackground = isApplicationInBackground;
    ProcessAndUIAssertion::setProcessStateMonitorEnabled(isApplicationInBackground);
}

ApplicationType applicationType(UIWindow *window)
{
    if (_UIApplicationIsExtension())
        return ApplicationType::Extension;

    if (WTF::processHasEntitlement("com.apple.UIKit.vends-view-services"_s) && window._isHostedInAnotherProcess)
        return ApplicationType::ViewService;

    return ApplicationType::Application;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(ApplicationStateTracker);

ApplicationStateTracker::ApplicationStateTracker(UIView *view, SEL didEnterBackgroundSelector, SEL willEnterForegroundSelector, SEL willBeginSnapshotSequenceSelector, SEL didCompleteSnapshotSequenceSelector)
    : m_view(view)
    , m_observer { adoptNS([[WKUIWindowSceneObserver alloc] initWithParent:*this]) }
    , m_didEnterBackgroundSelector(didEnterBackgroundSelector)
    , m_willEnterForegroundSelector(willEnterForegroundSelector)
    , m_willBeginSnapshotSequenceSelector(willBeginSnapshotSequenceSelector)
    , m_didCompleteSnapshotSequenceSelector(didCompleteSnapshotSequenceSelector)
    , m_isInBackground(true)
{
    ASSERT([m_view.get() respondsToSelector:m_didEnterBackgroundSelector]);
    ASSERT([m_view.get() respondsToSelector:m_willEnterForegroundSelector]);

    allApplicationStateTrackers().add(*this);
}

ApplicationStateTracker::~ApplicationStateTracker()
{
    RELEASE_LOG(ViewState, "%p - ~ApplicationStateTracker", this);

    setWindow(nil);
    setScene(nil);
    setViewController(nil);

    removeAllObservers();

    allApplicationStateTrackers().remove(*this);
    updateApplicationBackgroundState();
}

void ApplicationStateTracker::removeAllObservers()
{
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    if (m_didEnterBackgroundObserver) {
        [notificationCenter removeObserver:m_didEnterBackgroundObserver.get().get()];
        m_didEnterBackgroundObserver = nil;
    }
    if (m_willEnterForegroundObserver) {
        [notificationCenter removeObserver:m_willEnterForegroundObserver.get().get()];
        m_willEnterForegroundObserver = nil;
    }
    if (m_willBeginSnapshotSequenceObserver) {
        [notificationCenter removeObserver:m_willBeginSnapshotSequenceObserver.get().get()];
        m_willBeginSnapshotSequenceObserver = nil;
    }
    if (m_didCompleteSnapshotSequenceObserver) {
        [notificationCenter removeObserver:m_didCompleteSnapshotSequenceObserver.get().get()];
        m_didCompleteSnapshotSequenceObserver = nil;
    }
}

void ApplicationStateTracker::setWindow(UIWindow *window)
{
    if (window == m_window.get())
        return;

    if (m_window) {
        switch (m_applicationType) {
        case ApplicationType::Application:
            // Do not clear the scene when the window changes; the client should still
            // receive notifications about the view's previous window scene when that
            // scene is backgrounded.
            break;
        case ApplicationType::Extension:
        case ApplicationType::ViewService:
            setViewController(nil);
            break;
        }
    }

    m_window = window;
    [m_observer setObservedWindow:window];

    if (!m_window)
        return;

    m_applicationType = applicationType(window);

    switch (m_applicationType) {
    case ApplicationType::Application:
        setViewController(nil);
        setScene(window.windowScene);
        break;

    case ApplicationType::Extension:
    case ApplicationType::ViewService: {
        UIViewController *serviceViewController = nil;

        for (RetainPtr view = m_view.get(); view; view = view.get().superview) {
            auto viewController = WebCore::viewController(view.get());

            if (viewController._hostProcessIdentifier) {
                serviceViewController = viewController;
                break;
            }
        }

        ASSERT(serviceViewController);
        setScene(nil);
        setViewController(serviceViewController);
        break;
    }
    }

    updateApplicationBackgroundState();
}

void ApplicationStateTracker::setScene(UIScene *scene)
{
    auto isWindowAndSceneInBackground = [] (const auto& window, const auto& scene) {
        if (!scene) {
            // TestWebKitAPI will create a WKWebView and place it into a UIWindow that
            // has no UIWindowScene. For the purposes of keeping tests passing, treat
            // the combination of a window without a windowScene as not in the background:
            return !window;
        }

        return [scene activationState] == UISceneActivationStateBackground || [scene activationState] == UISceneActivationStateUnattached;
    };

    if (m_scene.get() == scene) {
        setIsInBackground(isWindowAndSceneInBackground(m_window, m_scene));
        return;
    }

    removeAllObservers();

    if (!scene && m_scene && m_isInBackground) {
        m_scene = nil;
        return;
    }

    m_scene = scene;
    setIsInBackground(isWindowAndSceneInBackground(m_window, m_scene));

    if (!m_scene)
        return;

    RELEASE_LOG(ViewState, "%p - ApplicationStateTracker: add observers for scene", this);

    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    m_didEnterBackgroundObserver = [notificationCenter addObserverForName:UISceneDidEnterBackgroundNotification object:scene queue:nil usingBlock:[this, weakThis = WeakPtr { *this }](NSNotification *notification) {
        if (!weakThis)
            return;
        RELEASE_LOG(ViewState, "%p - ApplicationStateTracker: UISceneDidEnterBackground", this);
        applicationDidEnterBackground();
    }];

    m_willEnterForegroundObserver = [notificationCenter addObserverForName:UISceneWillEnterForegroundNotification object:scene queue:nil usingBlock:[this, weakThis = WeakPtr { *this }](NSNotification *notification) {
        if (!weakThis)
            return;
        RELEASE_LOG(ViewState, "%p - ApplicationStateTracker: UISceneWillEnterForeground", this);
        applicationWillEnterForeground();
    }];

    m_willBeginSnapshotSequenceObserver = [notificationCenter addObserverForName:_UISceneWillBeginSystemSnapshotSequence object:scene queue:nil usingBlock:[this, weakThis = WeakPtr { *this }](NSNotification *notification) {
        if (!weakThis)
            return;
        willBeginSnapshotSequence();
    }];

    m_didCompleteSnapshotSequenceObserver = [notificationCenter addObserverForName:_UISceneDidCompleteSystemSnapshotSequence object:scene queue:nil usingBlock:[this, weakThis = WeakPtr { *this }](NSNotification *notification) {
        if (!weakThis)
            return;
        didCompleteSnapshotSequence();
    }];
}

void ApplicationStateTracker::setViewController(UIViewController *serviceViewController)
{
    if (m_viewController.get() == serviceViewController)
        return;

    removeAllObservers();

    m_viewController = serviceViewController;
    if (!m_viewController) {
        setIsInBackground(true);
        return;
    }

    pid_t applicationPID = serviceViewController._hostProcessIdentifier;
    ASSERT(applicationPID);

    bool isInBackground = !EndowmentStateTracker::isApplicationForeground(applicationPID);

    // Workaround for <rdar://problem/34028921>. If the host application is StoreKitUIService then it is also a ViewService
    // and is always in the background. We need to treat StoreKitUIService as foreground for the purpose of process suspension
    // or its ViewServices will get suspended.
    if ([serviceViewController._hostApplicationBundleIdentifier isEqualToString:@"com.apple.ios.StoreKitUIService"])
        isInBackground = false;

    setIsInBackground(isInBackground);

    RELEASE_LOG(ProcessSuspension, "%{public}s has PID %d, host application PID=%d, isInBackground=%d", _UIApplicationIsExtension() ? "Extension" : "ViewService", getpid(), applicationPID, m_isInBackground);

    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    m_didEnterBackgroundObserver = [notificationCenter addObserverForName:viewServiceBackgroundNotificationName object:serviceViewController queue:nil usingBlock:[this, weakThis = WeakPtr { *this }, applicationPID](NSNotification *) {
        if (!weakThis)
            return;
        RELEASE_LOG(ProcessSuspension, "%{public}s has PID %d, host application PID=%d, didEnterBackground", _UIApplicationIsExtension() ? "Extension" : "ViewService", getpid(), applicationPID);
        applicationDidEnterBackground();
    }];
    m_willEnterForegroundObserver = [notificationCenter addObserverForName:viewServiceForegroundNotificationName object:serviceViewController queue:nil usingBlock:[this, weakThis = WeakPtr { *this }, applicationPID](NSNotification *) {
        if (!weakThis)
            return;
        RELEASE_LOG(ProcessSuspension, "%{public}s has PID %d, host application PID=%d, willEnterForeground", _UIApplicationIsExtension() ? "Extension" : "ViewService", getpid(), applicationPID);
        applicationWillEnterForeground();
    }];
}

void ApplicationStateTracker::setIsInBackground(bool isInBackground)
{
    if (m_isInBackground == isInBackground)
        return;

    RELEASE_LOG(ViewState, "%p - ApplicationStateTracker::setIsInBackground: %d", this, isInBackground);

    m_isInBackground = isInBackground;
}

void ApplicationStateTracker::applicationDidEnterBackground()
{
    setIsInBackground(true);
    updateApplicationBackgroundState();

    if (auto view = m_view.get())
        wtfObjCMsgSend<void>(view.get(), m_didEnterBackgroundSelector);
}

void ApplicationStateTracker::applicationWillEnterForeground()
{
    setIsInBackground(false);
    updateApplicationBackgroundState();

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
