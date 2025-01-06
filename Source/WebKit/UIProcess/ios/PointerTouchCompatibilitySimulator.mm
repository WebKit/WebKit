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

#import "config.h"
#import "PointerTouchCompatibilitySimulator.h"

#if PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"
#import "UIKitUtilities.h"
#import "WKScrollView.h"
#import "WKWebViewInternal.h"
#import "_WKTouchEventGeneratorInternal.h"
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PointerTouchCompatibilitySimulator);

PointerTouchCompatibilitySimulator::PointerTouchCompatibilitySimulator(WKWebView *view)
    : m_view { view }
    , m_stateResetWatchdogTimer { RunLoop::main(), this, &PointerTouchCompatibilitySimulator::resetState }
{
}

#if HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING)

static constexpr auto predominantAxisDeltaRatio = 5;
static constexpr auto minimumPredominantAxisDelta = 15;
static constexpr auto stateResetTimerDelay = 250_ms;

static bool hasPredominantHorizontalAxis(WebCore::FloatSize delta)
{
    auto absoluteDeltaX = std::abs(delta.width());
    return absoluteDeltaX >= minimumPredominantAxisDelta && absoluteDeltaX > std::abs(delta.height() * predominantAxisDeltaRatio);
}

bool PointerTouchCompatibilitySimulator::handleScrollUpdate(WKBaseScrollView *scrollView, WKBEScrollViewScrollUpdate *update)
{
    if (!m_isEnabled)
        return false;

    RetainPtr view = m_view.get();
    RetainPtr window = [view window];
    if (!window) {
        resetState();
        return false;
    }

    switch (update.phase) {
    case WKBEScrollViewScrollUpdatePhaseEnded:
    case WKBEScrollViewScrollUpdatePhaseCancelled:
        resetState();
        return false;
    case WKBEScrollViewScrollUpdatePhaseBegan:
        m_initialDelta = { };
        break;
    default:
        break;
    }

    if (scrollView._wk_canScrollHorizontallyWithoutBouncing)
        return false;

#if USE(BROWSERENGINEKIT)
    auto translation = [update translationInView:view.get()];
#else
    auto delta = [update _adjustedAcceleratedDeltaInView:view.get()];
    auto translation = CGPointMake(delta.dx, delta.dy);
#endif
    m_initialDelta.expand(translation.x, translation.y);

    if (!isSimulatingTouches() && !hasPredominantHorizontalAxis(m_initialDelta))
        return false;

    m_centroid = [update locationInView:view.get()];
    m_stateResetWatchdogTimer.startOneShot(stateResetTimerDelay);

    if (!isSimulatingTouches()) {
        [[_WKTouchEventGenerator sharedTouchEventGenerator] touchDown:locationInScreen() window:window.get()];
        m_touchDelta = { m_initialDelta.width(), 0 };
        m_initialDelta = { };
    }

    m_touchDelta.expand(translation.x, 0);
    [[_WKTouchEventGenerator sharedTouchEventGenerator] moveToPoint:locationInScreen() duration:0 window:window.get()];
    return true;
}

#endif // HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING)

void PointerTouchCompatibilitySimulator::resetState()
{
    if (isSimulatingTouches())
        [[_WKTouchEventGenerator sharedTouchEventGenerator] liftUp:locationInScreen() window:window().get()];
    m_centroid = { };
    m_touchDelta = { };
    m_initialDelta = { };
    m_stateResetWatchdogTimer.stop();
}

WebCore::FloatPoint PointerTouchCompatibilitySimulator::locationInScreen() const
{
    CGPoint pointInView = m_centroid + m_touchDelta;
    return [view() convertPoint:pointInView toCoordinateSpace:[[[window() windowScene] screen] coordinateSpace]];
}

RetainPtr<UIWindow> PointerTouchCompatibilitySimulator::window() const
{
    return [m_view window];
}

void PointerTouchCompatibilitySimulator::setEnabled(bool enabled)
{
    if (m_isEnabled == enabled)
        return;

    m_isEnabled = enabled;

    if (!enabled)
        resetState();
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
