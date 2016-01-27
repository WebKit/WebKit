/*
 * Copyright (C) 2016 Igalia S.L.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#if ENABLE(SMOOTH_SCROLLING)

#include "FloatPoint.h"
#include "ScrollableArea.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

static const double frameRate = 60;
static const double tickTime = 1 / frameRate;
static const double minimumTimerInterval = .001;

ScrollAnimationSmooth::ScrollAnimationSmooth(ScrollableArea& scrollableArea, std::function<void (FloatPoint&&)>&& notifyPositionChangedFunction)
    : ScrollAnimation(scrollableArea)
    , m_notifyPositionChangedFunction(WTFMove(notifyPositionChangedFunction))
#if USE(REQUEST_ANIMATION_FRAME_TIMER)
    , m_animationTimer(*this, &ScrollAnimationSmooth::animationTimerFired)
#else
    , m_animationActive(false)
#endif
{
}

bool ScrollAnimationSmooth::scroll(ScrollbarOrientation orientation, ScrollGranularity granularity, float step, float multiplier)
{
    float minScrollPosition;
    float maxScrollPosition;
    if (orientation == HorizontalScrollbar) {
        minScrollPosition = m_scrollableArea.minimumScrollPosition().x();
        maxScrollPosition = m_scrollableArea.maximumScrollPosition().x();
    } else {
        minScrollPosition = m_scrollableArea.minimumScrollPosition().y();
        maxScrollPosition = m_scrollableArea.maximumScrollPosition().y();
    }
    bool needToScroll = updatePerAxisData(orientation == HorizontalScrollbar ? m_horizontalData : m_verticalData, granularity, step * multiplier, minScrollPosition, maxScrollPosition);
    if (needToScroll && !animationTimerActive()) {
        m_startTime = orientation == HorizontalScrollbar ? m_horizontalData.startTime : m_verticalData.startTime;
        animationTimerFired();
    }
    return needToScroll;
}

void ScrollAnimationSmooth::stop()
{
#if USE(REQUEST_ANIMATION_FRAME_TIMER)
    m_animationTimer.stop();
#else
    m_animationActive = false;
#endif
}

void ScrollAnimationSmooth::updateVisibleLengths()
{
    m_horizontalData.visibleLength = m_scrollableArea.visibleWidth();
    m_verticalData.visibleLength = m_scrollableArea.visibleHeight();
}

void ScrollAnimationSmooth::setCurrentPosition(const FloatPoint& position)
{
    stop();
    m_horizontalData = PerAxisData();
    m_horizontalData.currentPosition = position.x();
    m_horizontalData.desiredPosition = m_horizontalData.currentPosition;

    m_verticalData = PerAxisData();
    m_verticalData.currentPosition = position.y();
    m_verticalData.desiredPosition = m_verticalData.currentPosition;
}

#if !USE(REQUEST_ANIMATION_FRAME_TIMER)
void ScrollAnimationSmooth::serviceAnimation()
{
    if (m_animationActive)
        animationTimerFired();
}
#endif

ScrollAnimationSmooth::~ScrollAnimationSmooth()
{
}

static inline double curveAt(ScrollAnimationSmooth::Curve curve, double t)
{
    switch (curve) {
    case ScrollAnimationSmooth::Curve::Linear:
        return t;
    case ScrollAnimationSmooth::Curve::Quadratic:
        return t * t;
    case ScrollAnimationSmooth::Curve::Cubic:
        return t * t * t;
    case ScrollAnimationSmooth::Curve::Quartic:
        return t * t * t * t;
    case ScrollAnimationSmooth::Curve::Bounce:
        // Time base is chosen to keep the bounce points simpler:
        // 1 (half bounce coming in) + 1 + .5 + .25
        static const double timeBase = 2.75;
        static const double timeBaseSquared = timeBase * timeBase;
        if (t < 1 / timeBase)
            return timeBaseSquared * t * t;
        if (t < 2 / timeBase) {
            // Invert a [-.5,.5] quadratic parabola, center it in [1,2].
            double t1 = t - 1.5 / timeBase;
            const double parabolaAtEdge = 1 - .5 * .5;
            return timeBaseSquared * t1 * t1 + parabolaAtEdge;
        }
        if (t < 2.5 / timeBase) {
            // Invert a [-.25,.25] quadratic parabola, center it in [2,2.5].
            double t2 = t - 2.25 / timeBase;
            const double parabolaAtEdge = 1 - .25 * .25;
            return timeBaseSquared * t2 * t2 + parabolaAtEdge;
        }
        // Invert a [-.125,.125] quadratic parabola, center it in [2.5,2.75].
        const double parabolaAtEdge = 1 - .125 * .125;
        t -= 2.625 / timeBase;
        return timeBaseSquared * t * t + parabolaAtEdge;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static inline double attackCurve(ScrollAnimationSmooth::Curve curve, double deltaTime, double curveT, double startPosition, double attackPosition)
{
    double t = deltaTime / curveT;
    double positionFactor = curveAt(curve, t);
    return startPosition + positionFactor * (attackPosition - startPosition);
}

static inline double releaseCurve(ScrollAnimationSmooth::Curve curve, double deltaTime, double curveT, double releasePosition, double desiredPosition)
{
    double t = deltaTime / curveT;
    double positionFactor = 1 - curveAt(curve, 1 - t);
    return releasePosition + (positionFactor * (desiredPosition - releasePosition));
}

static inline double coastCurve(ScrollAnimationSmooth::Curve curve, double factor)
{
    return 1 - curveAt(curve, 1 - factor);
}

static inline double curveIntegralAt(ScrollAnimationSmooth::Curve curve, double t)
{
    switch (curve) {
    case ScrollAnimationSmooth::Curve::Linear:
        return t * t / 2;
    case ScrollAnimationSmooth::Curve::Quadratic:
        return t * t * t / 3;
    case ScrollAnimationSmooth::Curve::Cubic:
        return t * t * t * t / 4;
    case ScrollAnimationSmooth::Curve::Quartic:
        return t * t * t * t * t / 5;
    case ScrollAnimationSmooth::Curve::Bounce:
        static const double timeBase = 2.75;
        static const double timeBaseSquared = timeBase * timeBase;
        static const double timeBaseSquaredOverThree = timeBaseSquared / 3;
        double area;
        double t1 = std::min(t, 1 / timeBase);
        area = timeBaseSquaredOverThree * t1 * t1 * t1;
        if (t < 1 / timeBase)
            return area;

        t1 = std::min(t - 1 / timeBase, 1 / timeBase);
        // The integral of timeBaseSquared * (t1 - .5 / timeBase) * (t1 - .5 / timeBase) + parabolaAtEdge
        static const double secondInnerOffset = timeBaseSquared * .5 / timeBase;
        double bounceArea = t1 * (t1 * (timeBaseSquaredOverThree * t1 - secondInnerOffset) + 1);
        area += bounceArea;
        if (t < 2 / timeBase)
            return area;

        t1 = std::min(t - 2 / timeBase, 0.5 / timeBase);
        // The integral of timeBaseSquared * (t1 - .25 / timeBase) * (t1 - .25 / timeBase) + parabolaAtEdge
        static const double thirdInnerOffset = timeBaseSquared * .25 / timeBase;
        bounceArea =  t1 * (t1 * (timeBaseSquaredOverThree * t1 - thirdInnerOffset) + 1);
        area += bounceArea;
        if (t < 2.5 / timeBase)
            return area;

        t1 = t - 2.5 / timeBase;
        // The integral of timeBaseSquared * (t1 - .125 / timeBase) * (t1 - .125 / timeBase) + parabolaAtEdge
        static const double fourthInnerOffset = timeBaseSquared * .125 / timeBase;
        bounceArea = t1 * (t1 * (timeBaseSquaredOverThree * t1 - fourthInnerOffset) + 1);
        area += bounceArea;
        return area;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static inline double attackArea(ScrollAnimationSmooth::Curve curve, double startT, double endT)
{
    double startValue = curveIntegralAt(curve, startT);
    double endValue = curveIntegralAt(curve, endT);
    return endValue - startValue;
}

static inline double releaseArea(ScrollAnimationSmooth::Curve curve, double startT, double endT)
{
    double startValue = curveIntegralAt(curve, 1 - endT);
    double endValue = curveIntegralAt(curve, 1 - startT);
    return endValue - startValue;
}

static inline void getAnimationParametersForGranularity(ScrollGranularity granularity, double& animationTime, double& repeatMinimumSustainTime, double& attackTime, double& releaseTime, ScrollAnimationSmooth::Curve& coastTimeCurve, double& maximumCoastTime)
{
    switch (granularity) {
    case ScrollByDocument:
        animationTime = 20 * tickTime;
        repeatMinimumSustainTime = 10 * tickTime;
        attackTime = 10 * tickTime;
        releaseTime = 10 * tickTime;
        coastTimeCurve = ScrollAnimationSmooth::Curve::Linear;
        maximumCoastTime = 1;
        break;
    case ScrollByLine:
        animationTime = 10 * tickTime;
        repeatMinimumSustainTime = 7 * tickTime;
        attackTime = 3 * tickTime;
        releaseTime = 3 * tickTime;
        coastTimeCurve = ScrollAnimationSmooth::Curve::Linear;
        maximumCoastTime = 1;
        break;
    case ScrollByPage:
        animationTime = 15 * tickTime;
        repeatMinimumSustainTime = 10 * tickTime;
        attackTime = 5 * tickTime;
        releaseTime = 5 * tickTime;
        coastTimeCurve = ScrollAnimationSmooth::Curve::Linear;
        maximumCoastTime = 1;
        break;
    case ScrollByPixel:
        animationTime = 1 * tickTime;
        repeatMinimumSustainTime = 2 * tickTime;
        attackTime = 3 * tickTime;
        releaseTime = 3 * tickTime;
        coastTimeCurve = ScrollAnimationSmooth::Curve::Quadratic;
        maximumCoastTime = 1.25;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

bool ScrollAnimationSmooth::updatePerAxisData(PerAxisData& data, ScrollGranularity granularity, float delta, float minScrollPosition, float maxScrollPosition)
{
    if (!data.startTime || !delta || (delta < 0) != (data.desiredPosition - data.currentPosition < 0)) {
        data.desiredPosition = data.currentPosition;
        data.startTime = 0;
    }
    float newPosition = data.desiredPosition + delta;

    newPosition = std::max(std::min(newPosition, maxScrollPosition), minScrollPosition);

    if (newPosition == data.desiredPosition)
        return false;

    double animationTime, repeatMinimumSustainTime, attackTime, releaseTime, maximumCoastTime;
    Curve coastTimeCurve;
    getAnimationParametersForGranularity(granularity, animationTime, repeatMinimumSustainTime, attackTime, releaseTime, coastTimeCurve, maximumCoastTime);

    data.desiredPosition = newPosition;
    if (!data.startTime)
        data.attackTime = attackTime;
    data.animationTime = animationTime;
    data.releaseTime = releaseTime;

    // Prioritize our way out of over constraint.
    if (data.attackTime + data.releaseTime > data.animationTime) {
        if (data.releaseTime > data.animationTime)
            data.releaseTime = data.animationTime;
        data.attackTime = data.animationTime - data.releaseTime;
    }

    if (!data.startTime) {
        // FIXME: This should be the time from the event that got us here.
        data.startTime = monotonicallyIncreasingTime() - tickTime / 2;
        data.startPosition = data.currentPosition;
        data.lastAnimationTime = data.startTime;
    }
    data.startVelocity = data.currentVelocity;

    double remainingDelta = data.desiredPosition - data.currentPosition;
    double attackAreaLeft = 0;
    double deltaTime = data.lastAnimationTime - data.startTime;
    double attackTimeLeft = std::max(0., data.attackTime - deltaTime);
    double timeLeft = data.animationTime - deltaTime;
    double minTimeLeft = data.releaseTime + std::min(repeatMinimumSustainTime, data.animationTime - data.releaseTime - attackTimeLeft);
    if (timeLeft < minTimeLeft) {
        data.animationTime = deltaTime + minTimeLeft;
        timeLeft = minTimeLeft;
    }

    if (maximumCoastTime > (repeatMinimumSustainTime + releaseTime)) {
        double targetMaxCoastVelocity = data.visibleLength * .25 * frameRate;
        // This needs to be as minimal as possible while not being intrusive to page up/down.
        double minCoastDelta = data.visibleLength;

        if (fabs(remainingDelta) > minCoastDelta) {
            double maxCoastDelta = maximumCoastTime * targetMaxCoastVelocity;
            double coastFactor = std::min(1., (fabs(remainingDelta) - minCoastDelta) / (maxCoastDelta - minCoastDelta));

            // We could play with the curve here - linear seems a little soft. Initial testing makes me want to feed into the sustain time more aggressively.
            double coastMinTimeLeft = std::min(maximumCoastTime, minTimeLeft + coastCurve(coastTimeCurve, coastFactor) * (maximumCoastTime - minTimeLeft));

            if (double additionalTime = std::max(0., coastMinTimeLeft - minTimeLeft)) {
                double additionalReleaseTime = std::min(additionalTime, releaseTime / (releaseTime + repeatMinimumSustainTime) * additionalTime);
                data.releaseTime = releaseTime + additionalReleaseTime;
                data.animationTime = deltaTime + coastMinTimeLeft;
                timeLeft = coastMinTimeLeft;
            }
        }
    }

    double releaseTimeLeft = std::min(timeLeft, data.releaseTime);
    double sustainTimeLeft = std::max(0., timeLeft - releaseTimeLeft - attackTimeLeft);
    if (attackTimeLeft) {
        double attackSpot = deltaTime / data.attackTime;
        attackAreaLeft = attackArea(Curve::Cubic, attackSpot, 1) * data.attackTime;
    }

    double releaseSpot = (data.releaseTime - releaseTimeLeft) / data.releaseTime;
    double releaseAreaLeft = releaseArea(Curve::Cubic, releaseSpot, 1) * data.releaseTime;

    data.desiredVelocity = remainingDelta / (attackAreaLeft + sustainTimeLeft + releaseAreaLeft);
    data.releasePosition = data.desiredPosition - data.desiredVelocity * releaseAreaLeft;
    if (attackAreaLeft)
        data.attackPosition = data.startPosition + data.desiredVelocity * attackAreaLeft;
    else
        data.attackPosition = data.releasePosition - (data.animationTime - data.releaseTime - data.attackTime) * data.desiredVelocity;

    if (sustainTimeLeft) {
        double roundOff = data.releasePosition - ((attackAreaLeft ? data.attackPosition : data.currentPosition) + data.desiredVelocity * sustainTimeLeft);
        data.desiredVelocity += roundOff / sustainTimeLeft;
    }

    return true;
}

bool ScrollAnimationSmooth::animateScroll(PerAxisData& data, double currentTime)
{
    if (!data.startTime)
        return false;

    double lastScrollInterval = currentTime - data.lastAnimationTime;
    if (lastScrollInterval < minimumTimerInterval)
        return true;

    data.lastAnimationTime = currentTime;

    double deltaTime = currentTime - data.startTime;
    double newPosition = data.currentPosition;

    if (deltaTime > data.animationTime) {
        double desiredPosition = data.desiredPosition;
        data = PerAxisData();
        data.currentPosition = desiredPosition;
        return false;
    }
    if (deltaTime < data.attackTime)
        newPosition = attackCurve(Curve::Cubic, deltaTime, data.attackTime, data.startPosition, data.attackPosition);
    else if (deltaTime < (data.animationTime - data.releaseTime))
        newPosition = data.attackPosition + (deltaTime - data.attackTime) * data.desiredVelocity;
    else {
        // release is based on targeting the exact final position.
        double releaseDeltaT = deltaTime - (data.animationTime - data.releaseTime);
        newPosition = releaseCurve(Curve::Cubic, releaseDeltaT, data.releaseTime, data.releasePosition, data.desiredPosition);
    }

    // Normalize velocity to a per second amount. Could be used to check for jank.
    if (lastScrollInterval > 0)
        data.currentVelocity = (newPosition - data.currentPosition) / lastScrollInterval;
    data.currentPosition = newPosition;

    return true;
}

void ScrollAnimationSmooth::animationTimerFired()
{
    double currentTime = monotonicallyIncreasingTime();
    double deltaToNextFrame = ceil((currentTime - m_startTime) * frameRate) / frameRate - (currentTime - m_startTime);
    currentTime += deltaToNextFrame;

    bool continueAnimation = false;
    if (animateScroll(m_horizontalData, currentTime))
        continueAnimation = true;
    if (animateScroll(m_verticalData, currentTime))
        continueAnimation = true;

    if (continueAnimation)
#if USE(REQUEST_ANIMATION_FRAME_TIMER)
        startNextTimer(std::max(minimumTimerInterval, deltaToNextFrame));
#else
        startNextTimer();
    else
        m_animationActive = false;
#endif

    m_notifyPositionChangedFunction(FloatPoint(m_horizontalData.currentPosition, m_verticalData.currentPosition));
}

#if USE(REQUEST_ANIMATION_FRAME_TIMER)
void ScrollAnimationSmooth::startNextTimer(double delay)
{
    m_animationTimer.startOneShot(delay);
}
#else
void ScrollAnimationSmooth::startNextTimer()
{
    if (m_scrollableArea.scheduleAnimation())
        m_animationActive = true;
}
#endif

bool ScrollAnimationSmooth::animationTimerActive() const
{
#if USE(REQUEST_ANIMATION_FRAME_TIMER)
    return m_animationTimer.isActive();
#else
    return m_animationActive;
#endif
}

} // namespace WebCore

#endif // ENABLE(SMOOTH_SCROLLING)
