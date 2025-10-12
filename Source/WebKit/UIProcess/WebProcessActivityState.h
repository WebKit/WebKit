/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include <wtf/WeakRef.h>

namespace WebKit {

class ProcessAssertion;
class ProcessThrottlerActivity;
class ProcessThrottlerTimedActivity;
class RemotePageProxy;
class WebPageProxy;
class WebProcessProxy;

class WebProcessActivityState {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(WebProcessActivityState);
public:
    explicit WebProcessActivityState(WebPageProxy&);
    explicit WebProcessActivityState(RemotePageProxy&);

    void takeVisibleActivity();
    void takeAudibleActivity();
    void takeCapturingActivity();
    void takeMutedCaptureAssertion();

    void reset();
    void dropVisibleActivity();
    void dropAudibleActivity();
    void dropCapturingActivity();
    void dropMutedCaptureAssertion();

    bool hasValidVisibleActivity() const;
    bool hasValidAudibleActivity() const;
    bool hasValidCapturingActivity() const;
    bool hasValidMutedCaptureAssertion() const;

#if PLATFORM(IOS_FAMILY)
    void takeOpeningAppLinkActivity();
    void dropOpeningAppLinkActivity();
    bool hasValidOpeningAppLinkActivity() const;
#endif

#if ENABLE(WEB_PROCESS_SUSPENSION_DELAY)
    void updateWebProcessSuspensionDelay();
    void takeAccessibilityActivityWhenInWindow();
    void takeAccessibilityActivity();
    bool hasAccessibilityActivityForTesting() const;
    void viewDidEnterWindow();
    void viewDidLeaveWindow();
#endif

private:
    WebProcessProxy& process() const;
    Ref<WebProcessProxy> protectedProcess() const;

    Variant<WeakRef<WebPageProxy>, WeakRef<RemotePageProxy>> m_page;

    RefPtr<ProcessThrottlerActivity> m_isVisibleActivity;
#if ENABLE(WEB_PROCESS_SUSPENSION_DELAY)
    const Ref<ProcessThrottlerTimedActivity> m_wasRecentlyVisibleActivity;
    RefPtr<ProcessThrottlerActivity> m_accessibilityActivity;
    bool m_takeAccessibilityActivityWhenInWindow { false };
#endif
    RefPtr<ProcessThrottlerActivity> m_isAudibleActivity;
    RefPtr<ProcessThrottlerActivity> m_isCapturingActivity;
    RefPtr<ProcessAssertion> m_isMutedCaptureAssertion;
#if PLATFORM(IOS_FAMILY)
    RefPtr<ProcessThrottlerActivity> m_openingAppLinkActivity;
#endif
};

} // namespace WebKit
