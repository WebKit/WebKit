/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#include "DisplayLink.h"
#include "DisplayLinkObserverID.h"
#include "ViewGestureController.h"
#include <WebCore/VelocityData.h>

namespace WebKit {

class WebBackForwardListItem;

class SwipeProgressTracker final : public DisplayLink::Client {
    WTF_MAKE_TZONE_ALLOCATED(SwipeProgressTracker);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SwipeProgressTracker);
public:
    SwipeProgressTracker(WebPageProxy&, ViewGestureController&);
    ~SwipeProgressTracker();

    void startTracking(RefPtr<WebBackForwardListItem>&&, ViewGestureController::SwipeDirection);
    void reset();
    bool handleEvent(PlatformScrollEvent);
    void animationTimerFired();

private:
    void displayLinkFired(WebCore::PlatformDisplayID, WebCore::DisplayUpdate, bool, bool) override;

    enum class State : uint8_t {
        None,
        Pending,
        Swiping,
        Animating,
        Done
    };

    double totalSwipeDistance() const;
    bool shouldCancel();
    void startAnimation(bool forceCancelled = false);
    void endAnimation();

    void startDisplayLinkObserver();
    void stopDisplayLinkObserver();

    State m_state { State::None };
    ViewGestureController::SwipeDirection m_direction { ViewGestureController::SwipeDirection::Back };
    RefPtr<WebBackForwardListItem> m_targetItem;

    double m_progress { 0 };
    double m_averageVelocity { 0 };

    WebCore::HistoricalVelocityData m_velocityData;

    double m_animationStartProgress { 0 };
    double m_animationEndProgress { 0 };
    MonotonicTime m_animationStartTime;
    MonotonicTime m_animationEndTime;

    bool m_cancelled { false };

    std::optional<DisplayLinkObserverID> m_displayLinkObserverID;

    WeakRef<ViewGestureController> m_viewGestureController;
    WeakRef<WebPageProxy> m_webPageProxy;
};

} // namespace WebKit

#endif // PLATFORM(MAC)
