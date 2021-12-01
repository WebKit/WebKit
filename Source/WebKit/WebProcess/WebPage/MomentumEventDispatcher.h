/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)

// FIXME: Remove this once we decide which version we want.
#define USE_MOMENTUM_EVENT_DISPATCHER_PREMATURE_ROUNDING 0
#define USE_MOMENTUM_EVENT_DISPATCHER_TEMPORARY_LOGGING 1

#include "ScrollingAccelerationCurve.h"
#include "WebWheelEvent.h"
#include <WebCore/FloatSize.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/RectEdges.h>
#include <memory>
#include <wtf/Noncopyable.h>

namespace WebCore {
struct DisplayUpdate;
using PlatformDisplayID = uint32_t;
}

namespace WebKit {

class EventDispatcher;

class MomentumEventDispatcher {
    WTF_MAKE_NONCOPYABLE(MomentumEventDispatcher);
    WTF_MAKE_FAST_ALLOCATED;
public:
    MomentumEventDispatcher(EventDispatcher&);
    ~MomentumEventDispatcher();

    bool handleWheelEvent(WebCore::PageIdentifier, const WebWheelEvent&, WebCore::RectEdges<bool> rubberBandableEdges);

    void setScrollingAccelerationCurve(WebCore::PageIdentifier, std::optional<ScrollingAccelerationCurve>);

    void displayWasRefreshed(WebCore::PlatformDisplayID, const WebCore::DisplayUpdate&);

    void pageScreenDidChange(WebCore::PageIdentifier, WebCore::PlatformDisplayID);

private:
    void didStartMomentumPhase(WebCore::PageIdentifier, const WebWheelEvent&);
    void didEndMomentumPhase();

    bool eventShouldStartSyntheticMomentumPhase(WebCore::PageIdentifier, const WebWheelEvent&) const;

    void startDisplayLink();
    void stopDisplayLink();

    WebCore::PlatformDisplayID displayID() const;

    void dispatchSyntheticMomentumEvent(WebWheelEvent::Phase, WebCore::FloatSize delta);

    void buildOffsetTableWithInitialDelta(FloatSize);

    FloatSize offsetAtTime(Seconds);
    std::pair<FloatSize, FloatSize> computeNextDelta(FloatSize currentUnacceleratedDelta);

    void didReceiveScrollEventWithInterval(FloatSize, Seconds);
    void didReceiveScrollEvent(const WebWheelEvent&);

    struct Delta {
        float rawPlatformDelta;
        Seconds frameInterval;
    };
    static constexpr unsigned deltaHistoryQueueSize = 9;
    typedef Deque<Delta, deltaHistoryQueueSize> HistoricalDeltas;
    HistoricalDeltas m_deltaHistoryX;
    HistoricalDeltas m_deltaHistoryY;

    std::optional<WallTime> m_lastScrollTimestamp;
    std::optional<WebWheelEvent> m_lastIncomingEvent;
    WebCore::RectEdges<bool> m_lastRubberBandableEdges;
#if !RELEASE_LOG_DISABLED
    FloatSize m_lastActivePhaseDelta;
#endif

    struct {
        bool active { false };

        WebCore::PageIdentifier pageIdentifier;
        std::optional<ScrollingAccelerationCurve> accelerationCurve;
        std::optional<WebWheelEvent> initiatingEvent;

        FloatSize currentOffset;
        WallTime startTime;

        Vector<FloatSize> offsetTable;

#if !RELEASE_LOG_DISABLED
        FloatSize accumulatedEventOffset;
        bool didLogInitialQueueState { false };
#endif

#if USE(MOMENTUM_EVENT_DISPATCHER_PREMATURE_ROUNDING)
        FloatSize carryOffset;
#endif
    } m_currentGesture;

    DisplayLinkObserverID m_observerID;
    HashMap<WebCore::PageIdentifier, WebCore::PlatformDisplayID> m_displayIDs;
    HashMap<WebCore::PageIdentifier, std::optional<ScrollingAccelerationCurve>> m_accelerationCurves;
    EventDispatcher& m_dispatcher;
};

} // namespace WebKit

#endif
