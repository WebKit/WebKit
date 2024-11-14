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
#import "WKBaseScrollView.h"
#import "WKWebViewInternal.h"
#import "_WKTouchEventGeneratorInternal.h"
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PointerTouchCompatibilitySimulator);

bool PointerTouchCompatibilitySimulator::requiresPointerTouchCompatibility()
{
    return WTF::IOSApplication::isFeedly();
}

PointerTouchCompatibilitySimulator::PointerTouchCompatibilitySimulator(WKWebView *view)
    : m_view { view }
    , m_stateResetWatchdogTimer { RunLoop::main(), this, &PointerTouchCompatibilitySimulator::resetState }
{
}

#if HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING)

static constexpr auto predominantAxisDeltaRatio = 10;
static constexpr auto minimumPredominantAxisDelta = 20;
static constexpr auto stateResetTimerDelay = 50_ms;

static bool hasPredominantHorizontalAxis(CGPoint delta)
{
    auto absoluteDeltaX = std::abs(delta.x);
    return absoluteDeltaX >= minimumPredominantAxisDelta && absoluteDeltaX > std::abs(delta.y * predominantAxisDeltaRatio);
}

void PointerTouchCompatibilitySimulator::handleScrollUpdate(WKBaseScrollView *scrollView, WKBEScrollViewScrollUpdate *update)
{
    RetainPtr view = m_view.get();
    RetainPtr window = [view window];
    if (!window) {
        resetState();
        return;
    }

    switch (update.phase) {
    case WKBEScrollViewScrollUpdatePhaseEnded:
    case WKBEScrollViewScrollUpdatePhaseCancelled:
        resetState();
        return;
    default:
        break;
    }

    if (scrollView._wk_canScrollHorizontallyWithoutBouncing)
        return;

#if USE(BROWSERENGINEKIT)
    auto translation = [update translationInView:view.get()];
#else
    auto delta = [update _adjustedAcceleratedDeltaInView:view.get()];
    auto translation = CGPointMake(delta.dx, delta.dy);
#endif
    if (!hasPredominantHorizontalAxis(translation))
        return;

    bool isFirstTouch = m_delta.isZero();
    m_delta.expand(translation.x, 0);
    m_centroid = [update locationInView:view.get()];
    m_stateResetWatchdogTimer.startOneShot(stateResetTimerDelay);

    if (isFirstTouch)
        [[_WKTouchEventGenerator sharedTouchEventGenerator] touchDown:locationInScreen() window:window.get()];
    else
        [[_WKTouchEventGenerator sharedTouchEventGenerator] moveToPoint:locationInScreen() duration:0 window:window.get()];
}

#endif // HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING)

void PointerTouchCompatibilitySimulator::resetState()
{
    if (RetainPtr window = this->window(); window && !m_delta.isZero())
        [[_WKTouchEventGenerator sharedTouchEventGenerator] liftUp:locationInScreen() window:window.get()];
    m_centroid = { };
    m_delta = { };
    m_stateResetWatchdogTimer.stop();
}

WebCore::FloatPoint PointerTouchCompatibilitySimulator::locationInScreen() const
{
    CGPoint pointInView = m_centroid + m_delta;
    return [view() convertPoint:pointInView toCoordinateSpace:[[[window() windowScene] screen] coordinateSpace]];
}

RetainPtr<UIWindow> PointerTouchCompatibilitySimulator::window() const
{
    return [m_view window];
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
