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

#include "ScrollAnimation.h"

#include "Timer.h"

namespace WebCore {

class FloatPoint;
class ScrollableArea;
enum class ScrollClamping : bool;

class ScrollAnimationSmooth final: public ScrollAnimation {
public:
    ScrollAnimationSmooth(ScrollableArea&, const FloatPoint&, WTF::Function<void (FloatPoint&&)>&& notifyPositionChangedFunction);
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
    void scroll(const FloatPoint&) override;
    void stop() override;
    void updateVisibleLengths() override;
    void setCurrentPosition(const FloatPoint&) override;

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
        MonotonicTime startTime;
        double startVelocity { 0 };

        Seconds animationTime;
        MonotonicTime lastAnimationTime;

        double attackPosition { 0 };
        Seconds attackTime;
        Curve attackCurve { Curve::Quadratic };

        double releasePosition { 0 };
        Seconds releaseTime;
        Curve releaseCurve { Curve::Quadratic };

        int visibleLength { 0 };
    };

    bool updatePerAxisData(PerAxisData&, ScrollGranularity, float delta, float minScrollPosition, float maxScrollPosition, double smoothFactor = 1);
    bool animateScroll(PerAxisData&, MonotonicTime currentTime);

    void requestAnimationTimerFired();
    void startNextTimer(Seconds delay);
    void animationTimerFired();
    bool animationTimerActive() const;

    WTF::Function<void (FloatPoint&&)> m_notifyPositionChangedFunction;

    PerAxisData m_horizontalData;
    PerAxisData m_verticalData;

    MonotonicTime m_startTime;
    Timer m_animationTimer;
};

} // namespace WebCore

