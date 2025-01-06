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
#include "ScrollExtents.h"
#include "ScrollableArea.h"
#include "TimingFunction.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollAnimationSmooth);

static const float animationSpeed { 1000.0f };
static const Seconds maxAnimationDuration { 200_ms };

ScrollAnimationSmooth::ScrollAnimationSmooth(ScrollAnimationClient& client)
    : ScrollAnimation(Type::Smooth, client)
    , m_timingFunction(CubicBezierTimingFunction::create())
{
}

ScrollAnimationSmooth::~ScrollAnimationSmooth() = default;

bool ScrollAnimationSmooth::startAnimatedScrollToDestination(const FloatPoint& fromOffset, const FloatPoint& destinationOffset)
{
    auto extents = m_client.scrollExtentsForAnimation(*this);

    m_currentOffset = m_startOffset = fromOffset;
    m_destinationOffset = destinationOffset.constrainedBetween(extents.minimumScrollOffset(), extents.maximumScrollOffset());

    if (!isActive() && fromOffset == m_destinationOffset)
        return false;

    m_duration = durationFromDistance(m_destinationOffset - m_startOffset);
    if (!m_duration)
        return false;

    downcast<CubicBezierTimingFunction>(*m_timingFunction).setTimingFunctionPreset(CubicBezierTimingFunction::TimingFunctionPreset::EaseInOut);

    if (!isActive())
        didStart(MonotonicTime::now());

    return true;
}

bool ScrollAnimationSmooth::retargetActiveAnimation(const FloatPoint& newOffset)
{
    if (!isActive())
        return false;

    auto extents = m_client.scrollExtentsForAnimation(*this);

    m_startTime = MonotonicTime::now();
    m_startOffset = m_currentOffset;
    m_destinationOffset = newOffset.constrainedBetween(extents.minimumScrollOffset(), extents.maximumScrollOffset());
    m_duration = durationFromDistance(m_destinationOffset - m_startOffset);
    downcast<CubicBezierTimingFunction>(*m_timingFunction).setTimingFunctionPreset(CubicBezierTimingFunction::TimingFunctionPreset::EaseOut);
    m_timingFunction = CubicBezierTimingFunction::create(CubicBezierTimingFunction::TimingFunctionPreset::EaseOut);

    if (m_currentOffset == m_destinationOffset || !m_duration)
        return false;

    return true;
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

void ScrollAnimationSmooth::serviceAnimation(MonotonicTime currentTime)
{
    bool animationActive = animateScroll(currentTime);
    m_client.scrollAnimationDidUpdate(*this, m_currentOffset);
    if (!animationActive)
        didEnd();
}

bool ScrollAnimationSmooth::animateScroll(MonotonicTime currentTime)
{
    MonotonicTime endTime = m_startTime + m_duration;
    currentTime = std::min(currentTime, endTime);

    double fractionComplete = (currentTime - m_startTime) / m_duration;
    double progress = m_timingFunction->transformProgress(fractionComplete, m_duration.value());

    m_currentOffset = {
        linearInterpolation(progress, m_startOffset.x(), m_destinationOffset.x()),
        linearInterpolation(progress, m_startOffset.y(), m_destinationOffset.y()),
    };

    return currentTime < endTime;
}

String ScrollAnimationSmooth::debugDescription() const
{
    TextStream textStream;
    textStream << "ScrollAnimationSmooth " << this << " active " << isActive() << " from " << m_startOffset << " to " << m_destinationOffset << " current offset " << currentOffset();
    return textStream.release();
}

} // namespace WebCore
