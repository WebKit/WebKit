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

#if PLATFORM(IOS_FAMILY)

#import "WKBrowserEngineDefinitions.h"
#import "WKWebView.h"
#import <WebCore/FloatPoint.h>
#import <WebCore/FloatSize.h>
#import <wtf/RunLoop.h>
#import <wtf/WeakObjCPtr.h>

@class BEScrollViewScrollUpdate;
@class UIScrollEvent;
@class UIWindow;
@class WKBaseScrollView;

namespace WebKit {
class PointerTouchCompatibilitySimulator;
}

namespace WTF {
template<typename T> struct IsDeprecatedTimerSmartPointerException;
template<> struct IsDeprecatedTimerSmartPointerException<WebKit::PointerTouchCompatibilitySimulator> : std::true_type { };
}

namespace WebKit {

class PointerTouchCompatibilitySimulator {
    WTF_MAKE_NONCOPYABLE(PointerTouchCompatibilitySimulator);
    WTF_MAKE_TZONE_ALLOCATED(PointerTouchCompatibilitySimulator);
public:
    PointerTouchCompatibilitySimulator(WKWebView *);

#if HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING)
    bool handleScrollUpdate(WKBaseScrollView *, WKBEScrollViewScrollUpdate *);
#endif

    bool isSimulatingTouches() const { return !m_touchDelta.isZero(); }
    void setEnabled(bool);

    RetainPtr<WKWebView> view() const { return m_view.get(); }
    RetainPtr<UIWindow> window() const;

private:
    void resetState();
    WebCore::FloatPoint locationInScreen() const;

    const WeakObjCPtr<WKWebView> m_view;
    RunLoop::Timer m_stateResetWatchdogTimer;
    WebCore::FloatPoint m_centroid;
    WebCore::FloatSize m_touchDelta;
    WebCore::FloatSize m_initialDelta;
    bool m_isEnabled { false };
};

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
