/*
 * Copyright (C) 2016 Igalia S.L.
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

#include "FloatPoint.h"
#include "ScrollAnimation.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class PlatformWheelEvent;

class ScrollAnimationKinetic final : public ScrollAnimation {
    WTF_MAKE_TZONE_ALLOCATED(ScrollAnimationKinetic);
private:
    class PerAxisData {
    public:
        PerAxisData(double lower, double upper, double initialOffset, double initialVelocity);

        double offset() { return m_offset; }
        double velocity() { return m_velocity; }

        bool animateScroll(Seconds timeDelta);

    private:
        double m_lower { 0 };
        double m_upper { 0 };

        double m_coef1 { 0 };
        double m_coef2 { 0 };

        Seconds m_elapsedTime;
        double m_offset { 0 };
        double m_velocity { 0 };
    };

public:
    ScrollAnimationKinetic(ScrollAnimationClient&);
    virtual ~ScrollAnimationKinetic();

    bool startAnimatedScrollWithInitialVelocity(const FloatPoint& initialOffset, const FloatSize& velocity, const FloatSize& previousVelocity, bool mayHScroll, bool mayVScroll);
    bool retargetActiveAnimation(const FloatPoint& newOffset) final;

    void appendToScrollHistory(const PlatformWheelEvent&);
    void clearScrollHistory();

    FloatSize computeVelocity();

    MonotonicTime startTime() { return m_startTime; }
    FloatSize initialVelocity() { return m_initialVelocity; }
    FloatPoint initialOffset() { return m_initialOffset; }

    FloatSize accumulateVelocityFromPreviousGesture(const MonotonicTime, const FloatPoint&, const FloatSize&);

private:
    void serviceAnimation(MonotonicTime) final;
    String debugDescription() const final;

    std::optional<PerAxisData> m_horizontalData;
    std::optional<PerAxisData> m_verticalData;

    Vector<PlatformWheelEvent> m_scrollHistory;
    FloatPoint m_initialOffset;
    FloatSize m_initialVelocity;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_SCROLL_ANIMATION(WebCore::ScrollAnimationKinetic, type() == WebCore::ScrollAnimation::Type::Kinetic)
