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

#pragma once

#if PLATFORM(IOS_FAMILY)

#import <wtf/Forward.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/WeakPtr.h>

OBJC_CLASS BKSApplicationStateMonitor;
OBJC_CLASS UIView;
OBJC_CLASS UIViewController;
OBJC_CLASS UIWindow;
OBJC_CLASS UIScene;
OBJC_CLASS WKUIWindowSceneObserver;

namespace WebKit {
class ApplicationStateTracker;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebKit::ApplicationStateTracker> : std::true_type { };
}

namespace WebKit {

enum class ApplicationType : uint8_t {
    Application,
    ViewService,
    Extension,
};

class ApplicationStateTracker : public CanMakeWeakPtr<ApplicationStateTracker> {
    WTF_MAKE_TZONE_ALLOCATED(ApplicationStateTracker);
public:
    ApplicationStateTracker(UIView *, SEL didEnterBackgroundSelector, SEL willEnterForegroundSelector, SEL willBeginSnapshotSequenceSelector, SEL didCompleteSnapshotSequenceSelector);
    ~ApplicationStateTracker();

    bool isInBackground() const { return m_isInBackground; }

    void setWindow(UIWindow *);
    void setScene(UIScene *);

private:
    void setViewController(UIViewController *);

    void applicationDidEnterBackground();
    void applicationDidFinishSnapshottingAfterEnteringBackground();
    void applicationWillEnterForeground();
    void willBeginSnapshotSequence();
    void didCompleteSnapshotSequence();
    void removeAllObservers();

    WeakObjCPtr<UIView> m_view;
    WeakObjCPtr<UIWindow> m_window;
    WeakObjCPtr<UIScene> m_scene;
    WeakObjCPtr<UIViewController> m_viewController;

    RetainPtr<WKUIWindowSceneObserver> m_observer;

    ApplicationType m_applicationType { ApplicationType::Application };

    SEL m_didEnterBackgroundSelector;
    SEL m_willEnterForegroundSelector;
    SEL m_willBeginSnapshotSequenceSelector;
    SEL m_didCompleteSnapshotSequenceSelector;

    bool m_isInBackground;

    WeakObjCPtr<NSObject> m_didEnterBackgroundObserver;
    WeakObjCPtr<NSObject> m_willEnterForegroundObserver;
    WeakObjCPtr<NSObject> m_willBeginSnapshotSequenceObserver;
    WeakObjCPtr<NSObject> m_didCompleteSnapshotSequenceObserver;
};

ApplicationType applicationType(UIWindow *);

}

#endif
