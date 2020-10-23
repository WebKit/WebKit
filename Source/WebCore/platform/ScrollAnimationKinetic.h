/*
 * Copyright (C) 2016 Igalia S.L.
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

#include "FloatPoint.h"
#include "ScrollAnimation.h"

#include <wtf/Optional.h>
#include <wtf/RunLoop.h>

namespace WebCore {

class PlatformWheelEvent;

class ScrollAnimationKinetic final : public ScrollAnimation {
private:
    class PerAxisData {
    public:
        PerAxisData(double lower, double upper, double initialPosition, double initialVelocity);

        double position() { return m_position; }

        bool animateScroll(Seconds timeDelta);

    private:
        double m_lower { 0 };
        double m_upper { 0 };

        double m_coef1 { 0 };
        double m_coef2 { 0 };

        Seconds m_elapsedTime;
        double m_position { 0 };
        double m_velocity { 0 };
    };

public:
    using ScrollExtentsCallback = WTF::Function<ScrollExtents(void)>;
    using NotifyPositionChangedCallback = WTF::Function<void(FloatPoint&&)>;

    ScrollAnimationKinetic(ScrollExtentsCallback&&, NotifyPositionChangedCallback&&);
    virtual ~ScrollAnimationKinetic();

    void appendToScrollHistory(const PlatformWheelEvent&);
    void clearScrollHistory();
    FloatPoint computeVelocity();

    void start(const FloatPoint& initialPosition, const FloatPoint& velocity, bool mayHScroll, bool mayVScroll);
    void stop() override;

private:
    void animationTimerFired();

    ScrollExtentsCallback m_scrollExtentsFunction;
    NotifyPositionChangedCallback m_notifyPositionChangedFunction;

    Optional<PerAxisData> m_horizontalData;
    Optional<PerAxisData> m_verticalData;

    MonotonicTime m_startTime;
    RunLoop::Timer<ScrollAnimationKinetic> m_animationTimer;
    FloatPoint m_position;
    Vector<PlatformWheelEvent> m_scrollHistory;
};

} // namespace WebCore
