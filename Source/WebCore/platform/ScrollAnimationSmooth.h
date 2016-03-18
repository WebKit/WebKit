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

#ifndef ScrollAnimationSmooth_h
#define ScrollAnimationSmooth_h

#include "ScrollAnimation.h"

#if ENABLE(SMOOTH_SCROLLING)

#if !ENABLE(REQUEST_ANIMATION_FRAME)
#error "SMOOTH_SCROLLING requires REQUEST_ANIMATION_FRAME to be enabled."
#endif

#include "Timer.h"

namespace WebCore {

class FloatPoint;
class ScrollableArea;

class ScrollAnimationSmooth final: public ScrollAnimation {
public:
    ScrollAnimationSmooth(ScrollableArea&, const FloatPoint&, std::function<void (FloatPoint&&)>&& notifyPositionChangedFunction);
    virtual ~ScrollAnimationSmooth();

    enum class Curve {
        Linear,
        Quadratic,
        Cubic,
        Quartic,
        Bounce
    };

private:
    bool scroll(ScrollbarOrientation, ScrollGranularity, float step, float multiplier) override;
    void stop() override;
    void updateVisibleLengths() override;
    void setCurrentPosition(const FloatPoint&) override;
#if !USE(REQUEST_ANIMATION_FRAME_TIMER)
    void serviceAnimation() override;
#endif

    struct PerAxisData {
        PerAxisData() = delete;

        PerAxisData(float position, int length)
            : currentPosition(position)
            , desiredPosition(position)
            , visibleLength(length)
        {
        }

        float currentPosition { 0 };
        double currentVelocity { 0 };

        double desiredPosition { 0 };
        double desiredVelocity { 0 };

        double startPosition { 0 };
        double startTime { 0 };
        double startVelocity { 0 };

        double animationTime { 0 };
        double lastAnimationTime { 0 };

        double attackPosition { 0 };
        double attackTime { 0 };
        Curve attackCurve { Curve::Quadratic };

        double releasePosition { 0 };
        double releaseTime { 0 };
        Curve releaseCurve { Curve::Quadratic };

        int visibleLength { 0 };
    };

    bool updatePerAxisData(PerAxisData&, ScrollGranularity, float delta, float minScrollPosition, float maxScrollPosition);
    bool animateScroll(PerAxisData&, double currentTime);

#if USE(REQUEST_ANIMATION_FRAME_TIMER)
    void requestAnimationTimerFired();
    void startNextTimer(double delay);
#else
    void startNextTimer();
#endif
    void animationTimerFired();
    bool animationTimerActive() const;

    std::function<void (FloatPoint&&)> m_notifyPositionChangedFunction;

    PerAxisData m_horizontalData;
    PerAxisData m_verticalData;

    double m_startTime { 0 };
#if USE(REQUEST_ANIMATION_FRAME_TIMER)
    Timer m_animationTimer;
#else
    bool m_animationActive { false };
#endif

};

} // namespace WebCore

#endif // ENABLE(SMOOTH_SCROLLING)
#endif // ScrollAnimationSmooth_h
