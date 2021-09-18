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

bool ScrollAnimationSmooth::startAnimatedScroll(ScrollbarOrientation orientation, ScrollGranularity, const FloatPoint& fromPosition, float step, float multiplier)
{
    m_startPosition = fromPosition;
    auto destinationPosition = fromPosition;
    switch (orientation) {
    case HorizontalScrollbar:
        destinationPosition.setX(destinationPosition.x() + step * multiplier);
        break;
    case VerticalScrollbar:
        destinationPosition.setY(destinationPosition.y() + step * multiplier);
        break;
    }

    m_duration = durationFromDistance(destinationPosition - m_startPosition);

    auto extents = m_client.scrollExtentsForAnimation(*this);
    return startOrRetargetAnimation(extents, destinationPosition);
}

bool ScrollAnimationSmooth::startAnimatedScrollToDestination(const FloatPoint& fromPosition, const FloatPoint& destinationPosition)
{
    m_startPosition = fromPosition;
    m_duration = durationFromDistance(destinationPosition - m_startPosition);

    auto extents = m_client.scrollExtentsForAnimation(*this);
    return startOrRetargetAnimation(extents, destinationPosition);
}

bool ScrollAnimationSmooth::retargetActiveAnimation(const FloatPoint& newDestination)
{
    if (!isActive())
        return false;

    auto extents = m_client.scrollExtentsForAnimation(*this);
    return startOrRetargetAnimation(extents, newDestination);
}

bool ScrollAnimationSmooth::startOrRetargetAnimation(const ScrollExtents& extents, const FloatPoint& destinationPosition)
{
    m_destinationPosition = destinationPosition.constrainedBetween(extents.minimumScrollPosition, extents.maximumScrollPosition);
    bool needToScroll = m_startPosition != m_destinationPosition;

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
    // FIXME: Ideally fix up m_startPosition so m_currentPosition doesn't go backwards.
    m_destinationPosition = m_destinationPosition.constrainedBetween(extents.minimumScrollPosition, extents.maximumScrollPosition);
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
    double progress = m_easeInOutTimingFunction->transformTime(fractionComplete, m_duration.value());

    m_currentPosition = {
        linearInterpolation(progress, m_startPosition.x(), m_destinationPosition.x()),
        linearInterpolation(progress, m_startPosition.y(), m_destinationPosition.y()),
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

    m_client.scrollAnimationDidUpdate(*this, m_currentPosition);
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
