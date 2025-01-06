/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "ScrollAnimationRubberBand.h"

#if HAVE(RUBBER_BANDING)

#import "FloatPoint.h"
#import "GeometryUtilities.h"
#import <pal/spi/mac/NSScrollViewSPI.h>
#import <wtf/TZoneMallocInlines.h>

static float elasticDeltaForTimeDelta(float initialPosition, float initialVelocity, Seconds elapsedTime)
{
    return _NSElasticDeltaForTimeDelta(initialPosition, initialVelocity, elapsedTime.seconds());
}

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollAnimationRubberBand);

static inline float roundTowardZero(float num)
{
    return num > 0 ? ceilf(num - 0.5f) : floorf(num + 0.5f);
}

static inline float roundToDevicePixelTowardZero(float num)
{
    float roundedNum = roundf(num);
    if (std::abs(num - roundedNum) < 0.125)
        num = roundedNum;

    return roundTowardZero(num);
}

ScrollAnimationRubberBand::ScrollAnimationRubberBand(ScrollAnimationClient& client)
    : ScrollAnimation(Type::RubberBand, client)
{
}

ScrollAnimationRubberBand::~ScrollAnimationRubberBand() = default;

bool ScrollAnimationRubberBand::startRubberBandAnimation(const FloatSize& initialVelocity, const FloatSize& initialOverscroll)
{
    m_initialVelocity = initialVelocity;
    m_initialOverscroll = initialOverscroll;

    didStart(MonotonicTime::now());
    return true;
}

bool ScrollAnimationRubberBand::retargetActiveAnimation(const FloatPoint&)
{
    return false;
}

void ScrollAnimationRubberBand::updateScrollExtents()
{
    // FIXME: If we're rubberbanding at the bottom and the content size changes we should fix up m_targetOffset.
}

void ScrollAnimationRubberBand::serviceAnimation(MonotonicTime currentTime)
{
    auto elapsedTime = timeSinceStart(currentTime);

    // This is very similar to ScrollingMomentumCalculator logic, but I wasn't able to get to ScrollingMomentumCalculator to
    // give the correct behavior when starting a rubberband with initial velocity (i.e. bouncing).

    auto rubberBandOffset = FloatSize {
        roundToDevicePixelTowardZero(elasticDeltaForTimeDelta(m_initialOverscroll.width(), -m_initialVelocity.width(), elapsedTime)),
        roundToDevicePixelTowardZero(elasticDeltaForTimeDelta(m_initialOverscroll.height(), -m_initialVelocity.height(), elapsedTime))
    };

    // We might be rubberbanding away from an edge and back, so wait a frame or two before checking for completion.
    bool animationComplete = rubberBandOffset.isZero() && elapsedTime > 24_ms;
    
    auto scrollDelta = rubberBandOffset - m_client.overscrollAmount(*this);
    m_currentOffset = m_client.scrollOffset(*this) + scrollDelta;

    m_client.scrollAnimationDidUpdate(*this, m_currentOffset);

    if (animationComplete)
        didEnd();
}

String ScrollAnimationRubberBand::debugDescription() const
{
    TextStream textStream;
    textStream << "ScrollAnimationRubberBand " << this << " active " << isActive() << " initial velocity " << m_initialVelocity << " initial overscroll " << m_initialOverscroll;
    return textStream.release();
}

} // namespace WebCore

#endif // HAVE(RUBBER_BANDING)
