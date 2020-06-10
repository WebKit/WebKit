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
#import <wtf/WeakObjCPtr.h>
#import <wtf/WeakPtr.h>

OBJC_CLASS BKSApplicationStateMonitor;
OBJC_CLASS UIView;
OBJC_CLASS UIWindow;

namespace WebKit {

class ApplicationStateTracker : public CanMakeWeakPtr<ApplicationStateTracker> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ApplicationStateTracker(UIView *, SEL didEnterBackgroundSelector, SEL didFinishSnapshottingAfterEnteringBackgroundSelector, SEL willEnterForegroundSelector, SEL willBeginSnapshotSequenceSelector, SEL didCompleteSnapshotSequenceSelector);
    ~ApplicationStateTracker();

    bool isInBackground() const { return m_isInBackground; }

private:
    void applicationDidEnterBackground();
    void applicationDidFinishSnapshottingAfterEnteringBackground();
    void applicationWillEnterForeground();
    void willBeginSnapshotSequence();
    void didCompleteSnapshotSequence();

    WeakObjCPtr<UIView> m_view;
    SEL m_didEnterBackgroundSelector;
    SEL m_didFinishSnapshottingAfterEnteringBackgroundSelector;
    SEL m_willEnterForegroundSelector;
    SEL m_willBeginSnapshotSequenceSelector;
    SEL m_didCompleteSnapshotSequenceSelector;

    bool m_isInBackground;

    id m_didEnterBackgroundObserver;
    id m_didFinishSnapshottingAfterEnteringBackgroundObserver;
    id m_willEnterForegroundObserver;
    id m_willBeginSnapshotSequenceObserver;
    id m_didCompleteSnapshotSequenceObserver;
};

enum class ApplicationType {
    Application,
    ViewService,
    Extension,
};

ApplicationType applicationType(UIWindow *);

}

#endif
