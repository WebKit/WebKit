/*
 * Copyright (C) 2016 Igalia S.L.
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
 * Copyright (c) 2011, Google Inc. All rights reserved.
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

#include "config.h"
#include "ScrollAnimationSmooth.h"

#include "FloatPoint.h"
#include "GeometryUtilities.h"
#include "ScrollableArea.h"
#include "TimingFunction.h"

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace WebCore {

static const float animationSpeed { 1000.0f };
static const Seconds maxAnimationDuration { 200_ms };

ScrollAnimationSmooth::ScrollAnimationSmooth(ScrollAnimationClient& client)
    : ScrollAnimation(client)
    , m_animationTimer(RunLoop::current(), this, &ScrollAnimationSmooth::animationTimerFired)
    , m_easeInOutTimingFunction(CubicBezierTimingFunction::create(CubicBezierTimingFunction::TimingFunctionPreset::EaseInOut))
{
#if USE(GLIB_EVENT_LOOP)
    m_animationTimer.setPriority(WTF::RunLoopSourcePriority::DisplayRefreshMonitorTimer);
#endif
}

ScrollAnimationSmooth::~ScrollAnimationSmooth() = default;

bool ScrollAnimationSmooth::startAnimatedScroll(ScrollbarOrientation orientation, ScrollGranularity, const FloatPoint& fromOffset, float step, float multiplier)
{
    m_startOffset = fromOffset;
    auto destinationOffset = fromOffset;
    switch (orientation) {
    case HorizontalScrollbar:
        destinationOffset.setX(destinationOffset.x() + step * multiplier);
        break;
    case VerticalScrollbar:
        destinationOffset.setY(destinationOffset.y() + step * multiplier);
        break;
    }

    m_duration = durationFromDistance(destinationOffset - m_startOffset);

    auto extents = m_client.scrollExtentsForAnimation(*this);
    return startOrRetargetAnimation(extents, destinationOffset);
}

bool ScrollAnimationSmooth::startAnimatedScrollToDestination(const FloatPoint& fromOffset, const FloatPoint& destinationOffset)
{
    m_startOffset = fromOffset;
    m_duration = durationFromDistance(destinationOffset - m_startOffset);

    auto extents = m_client.scrollExtentsForAnimation(*this);
    return startOrRetargetAnimation(extents, destinationOffset);
}

bool ScrollAnimationSmooth::retargetActiveAnimation(const FloatPoint& newOffset)
{
    if (!isActive())
        return false;

    auto extents = m_client.scrollExtentsForAnimation(*this);
    return startOrRetargetAnimation(extents, newOffset);
}

bool ScrollAnimationSmooth::startOrRetargetAnimation(const ScrollExtents& extents, const FloatPoint& destinationOffset)
{
    m_destinationOffset = destinationOffset.constrainedBetween(extents.minimumScrollOffset(), extents.maximumScrollOffset());
    bool needToScroll = m_startOffset != m_destinationOffset;

    if (needToScroll && !isActive()) {
        m_startTime = MonotonicTime::now();
        animationTimerFired();
    }
    return needToScroll;
}

void ScrollAnimationSmooth::stop()
{
    m_animationTimer.stop();
    m_client.scrollAnimationDidEnd(*this);
}

void ScrollAnimationSmooth::updateScrollExtents()
{
    auto extents = m_client.scrollExtentsForAnimation(*this);
    // FIXME: Ideally fix up m_startOffset so m_currentOffset doesn't go backwards.
    m_destinationOffset = m_destinationOffset.constrainedBetween(extents.minimumScrollOffset(), extents.maximumScrollOffset());
}

Seconds ScrollAnimationSmooth::durationFromDistance(const FloatSize& delta) const
{
    float distance = euclidianDistance(delta);
    return std::min(Seconds(distance / animationSpeed), maxAnimationDuration);
}

inline float linearInterpolation(float progress, float a, float b)
{
    return a + progress * (b - a);
}

bool ScrollAnimationSmooth::animateScroll(MonotonicTime currentTime)
{
    MonotonicTime endTime = m_startTime + m_duration;
    currentTime = std::min(currentTime, endTime);

    double fractionComplete = (currentTime - m_startTime) / m_duration;
    double progress = m_easeInOutTimingFunction->transformProgress(fractionComplete, m_duration.value());

    m_currentOffset = {
        linearInterpolation(progress, m_startOffset.x(), m_destinationOffset.x()),
        linearInterpolation(progress, m_startOffset.y(), m_destinationOffset.y()),
    };

    return currentTime < endTime;
}

void ScrollAnimationSmooth::animationTimerFired()
{
    MonotonicTime currentTime = MonotonicTime::now();
    Seconds deltaToNextFrame = 1_s * ceil((currentTime - m_startTime).value() * frameRate) / frameRate - (currentTime - m_startTime);
    currentTime += deltaToNextFrame;

    bool continueAnimation = animateScroll(currentTime);
    if (continueAnimation)
        startNextTimer(std::max(minimumTimerInterval, deltaToNextFrame));

    m_client.scrollAnimationDidUpdate(*this, m_currentOffset);
    if (!continueAnimation)
        m_client.scrollAnimationDidEnd(*this);
}

void ScrollAnimationSmooth::startNextTimer(Seconds delay)
{
    m_animationTimer.startOneShot(delay);
}

bool ScrollAnimationSmooth::isActive() const
{
    return m_animationTimer.isActive();
}

} // namespace WebCore
