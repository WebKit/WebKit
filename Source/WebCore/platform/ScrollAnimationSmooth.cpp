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
#include "ScrollableArea.h"

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace WebCore {

static const double frameRate = 60;
static const Seconds tickTime = 1_s / frameRate;
static const Seconds minimumTimerInterval { 1_ms };
static const double smoothFactorForProgrammaticScroll = 1;

ScrollAnimationSmooth::ScrollAnimationSmooth(ScrollAnimationClient& client)
    : ScrollAnimation(client)
    , m_animationTimer(RunLoop::current(), this, &ScrollAnimationSmooth::animationTimerFired)
{
#if USE(GLIB_EVENT_LOOP)
    m_animationTimer.setPriority(WTF::RunLoopSourcePriority::DisplayRefreshMonitorTimer);
#endif
}

ScrollAnimationSmooth::~ScrollAnimationSmooth() = default;

bool ScrollAnimationSmooth::startAnimatedScroll(ScrollbarOrientation orientation, ScrollGranularity granularity, const FloatPoint& fromPosition, float step, float multiplier)
{
    float minScrollPosition;
    float maxScrollPosition;
    auto extents = m_client.scrollExtentsForAnimation(*this);
    initializeAxesData(extents, fromPosition);

    if (orientation == HorizontalScrollbar) {
        minScrollPosition = extents.minimumScrollPosition.x();
        maxScrollPosition = extents.maximumScrollPosition.x();
    } else {
        minScrollPosition = extents.minimumScrollPosition.y();
        maxScrollPosition = extents.maximumScrollPosition.y();
    }
    auto& data = orientation == HorizontalScrollbar ? m_horizontalData : m_verticalData;
    bool needToScroll = updatePerAxisData(data, granularity, data.desiredPosition + (step * multiplier), minScrollPosition, maxScrollPosition);
    if (needToScroll && !isActive()) {
        m_startTime = orientation == HorizontalScrollbar ? m_horizontalData.startTime : m_verticalData.startTime;
        animationTimerFired();
    }
    return needToScroll;
}

bool ScrollAnimationSmooth::startAnimatedScrollToDestination(const FloatPoint& fromPosition, const FloatPoint& destinationPosition)
{
    auto extents = m_client.scrollExtentsForAnimation(*this);
    initializeAxesData(extents, fromPosition);
    
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
    auto granularity = ScrollByPage;
    bool needToScroll = updatePerAxisData(m_horizontalData, granularity, destinationPosition.x(), extents.minimumScrollPosition.x(), extents.maximumScrollPosition.x(), smoothFactorForProgrammaticScroll);
    needToScroll |=
        updatePerAxisData(m_verticalData, granularity, destinationPosition.y(), extents.minimumScrollPosition.y(), extents.maximumScrollPosition.y(), smoothFactorForProgrammaticScroll);
    if (needToScroll && !isActive()) {
        m_startTime = m_verticalData.startTime;
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
    m_horizontalData.visibleLength = extents.visibleSize.width();
    m_verticalData.visibleLength = extents.visibleSize.height();
}

void ScrollAnimationSmooth::initializeAxesData(const ScrollExtents& extents, const FloatPoint& startPosition)
{
    m_horizontalData = PerAxisData(startPosition.x(), extents.visibleSize.width());
    m_verticalData = PerAxisData(startPosition.y(), extents.visibleSize.height());
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

static inline void getAnimationParametersForGranularity(ScrollGranularity granularity, Seconds& animationDuration, Seconds& repeatMinimumSustainTime, Seconds& attackDuration, Seconds& releaseDuration, ScrollAnimationSmooth::Curve& coastTimeCurve, Seconds& maximumCoastTime)
{
    switch (granularity) {
    case ScrollByDocument:
        animationDuration = tickTime * 10;
        repeatMinimumSustainTime = tickTime * 10;
        attackDuration = tickTime * 10;
        releaseDuration = tickTime * 10;
        coastTimeCurve = ScrollAnimationSmooth::Curve::Linear;
        maximumCoastTime = 1_s;
        break;
    case ScrollByLine:
        animationDuration = tickTime * 10;
        repeatMinimumSustainTime = tickTime * 7;
        attackDuration = tickTime * 3;
        releaseDuration = tickTime * 3;
        coastTimeCurve = ScrollAnimationSmooth::Curve::Linear;
        maximumCoastTime = 1_s;
        break;
    case ScrollByPage:
        animationDuration = tickTime * 15;
        repeatMinimumSustainTime = tickTime * 10;
        attackDuration = tickTime * 5;
        releaseDuration = tickTime * 5;
        coastTimeCurve = ScrollAnimationSmooth::Curve::Linear;
        maximumCoastTime = 1_s;
        break;
    case ScrollByPixel:
        animationDuration = tickTime * 11;
        repeatMinimumSustainTime = tickTime * 2;
        attackDuration = tickTime * 3;
        releaseDuration = tickTime * 3;
        coastTimeCurve = ScrollAnimationSmooth::Curve::Quadratic;
        maximumCoastTime = 1250_ms;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

bool ScrollAnimationSmooth::updatePerAxisData(PerAxisData& data, ScrollGranularity granularity, float newPosition, float minScrollPosition, float maxScrollPosition, double smoothFactor)
{
    if (!data.startTime || newPosition == data.currentPosition) {
        data.desiredPosition = data.currentPosition;
        data.startTime = { };
    }

    newPosition = std::max(std::min(newPosition, maxScrollPosition), minScrollPosition);
    if (newPosition == data.desiredPosition)
        return false;

    Seconds animationDuration, repeatMinimumSustainTime, attackDuration, releaseDuration, maximumCoastTime;
    Curve coastTimeCurve;
    getAnimationParametersForGranularity(granularity, animationDuration, repeatMinimumSustainTime, attackDuration, releaseDuration, coastTimeCurve, maximumCoastTime);

    animationDuration *= smoothFactor;
    repeatMinimumSustainTime *= smoothFactor;
    attackDuration *= smoothFactor;
    releaseDuration *= smoothFactor;
    maximumCoastTime *= smoothFactor;

    data.desiredPosition = newPosition;
    if (!data.startTime)
        data.attackDuration = attackDuration;
    data.animationDuration = animationDuration;
    data.releaseDuration = releaseDuration;

    // Prioritize our way out of over constraint.
    if (data.attackDuration + data.releaseDuration > data.animationDuration) {
        if (data.releaseDuration > data.animationDuration)
            data.releaseDuration = data.animationDuration;
        data.attackDuration = data.animationDuration - data.releaseDuration;
    }

    if (!data.startTime) {
        // FIXME: This should be the time from the event that got us here.
        data.startTime = MonotonicTime::now() - tickTime / 2.;
        data.startPosition = data.currentPosition;
        data.lastAnimationTime = data.startTime;
    }
    data.startVelocity = data.currentVelocity;

    double remainingDelta = data.desiredPosition - data.currentPosition;
    double attackAreaLeft = 0;
    Seconds deltaTime = data.lastAnimationTime - data.startTime;
    Seconds attackTimeLeft = std::max(0_s, data.attackDuration - deltaTime);
    Seconds timeLeft = data.animationDuration - deltaTime;
    Seconds minTimeLeft = data.releaseDuration + std::min(repeatMinimumSustainTime, data.animationDuration - data.releaseDuration - attackTimeLeft);
    if (timeLeft < minTimeLeft) {
        data.animationDuration = deltaTime + minTimeLeft;
        timeLeft = minTimeLeft;
    }

    if (maximumCoastTime > (repeatMinimumSustainTime + releaseDuration)) {
        double targetMaxCoastVelocity = data.visibleLength * .25 * frameRate;
        // This needs to be as minimal as possible while not being intrusive to page up/down.
        double minCoastDelta = data.visibleLength;

        if (fabs(remainingDelta) > minCoastDelta) {
            double maxCoastDelta = maximumCoastTime.value() * targetMaxCoastVelocity;
            double coastFactor = std::min(1., (fabs(remainingDelta) - minCoastDelta) / (maxCoastDelta - minCoastDelta));

            // We could play with the curve here - linear seems a little soft. Initial testing makes me want to feed into the sustain time more aggressively.
            Seconds coastMinTimeLeft = std::min(maximumCoastTime, minTimeLeft + (maximumCoastTime - minTimeLeft) * coastCurve(coastTimeCurve, coastFactor));

            if (Seconds additionalTime = std::max(0_s, coastMinTimeLeft - minTimeLeft)) {
                Seconds additionalReleaseTime = std::min(additionalTime, additionalTime * (releaseDuration / (releaseDuration + repeatMinimumSustainTime)));
                data.releaseDuration = releaseDuration + additionalReleaseTime;
                data.animationDuration = deltaTime + coastMinTimeLeft;
                timeLeft = coastMinTimeLeft;
            }
        }
    }

    Seconds releaseTimeLeft = std::min(timeLeft, data.releaseDuration);
    Seconds sustainTimeLeft = std::max(0_s, timeLeft - releaseTimeLeft - attackTimeLeft);
    if (attackTimeLeft) {
        double attackSpot = deltaTime / data.attackDuration;
        attackAreaLeft = attackArea(Curve::Cubic, attackSpot, 1) * data.attackDuration.value();
    }

    double releaseSpot = (data.releaseDuration - releaseTimeLeft) / data.releaseDuration;
    double releaseAreaLeft = releaseArea(Curve::Cubic, releaseSpot, 1) * data.releaseDuration.value();

    data.desiredVelocity = remainingDelta / (attackAreaLeft + sustainTimeLeft.value() + releaseAreaLeft);
    data.releasePosition = data.desiredPosition - data.desiredVelocity * releaseAreaLeft;
    if (attackAreaLeft)
        data.attackPosition = data.startPosition + data.desiredVelocity * attackAreaLeft;
    else
        data.attackPosition = data.releasePosition - (data.animationDuration - data.releaseDuration - data.attackDuration).value() * data.desiredVelocity;

    if (sustainTimeLeft) {
        double roundOff = data.releasePosition - ((attackAreaLeft ? data.attackPosition : data.currentPosition) + data.desiredVelocity * sustainTimeLeft.value());
        data.desiredVelocity += roundOff / sustainTimeLeft.value();
    }

    return true;
}

bool ScrollAnimationSmooth::animateScroll(PerAxisData& data, MonotonicTime currentTime)
{
    if (!data.startTime)
        return false;

    Seconds lastScrollInterval = currentTime - data.lastAnimationTime;
    if (lastScrollInterval < minimumTimerInterval)
        return true;

    data.lastAnimationTime = currentTime;

    Seconds deltaTime = currentTime - data.startTime;
    double newPosition = data.currentPosition;

    if (deltaTime > data.animationDuration) {
        data = PerAxisData(data.desiredPosition, data.visibleLength);
        return false;
    }
    if (deltaTime < data.attackDuration)
        newPosition = attackCurve(Curve::Cubic, deltaTime.value(), data.attackDuration.value(), data.startPosition, data.attackPosition);
    else if (deltaTime < (data.animationDuration - data.releaseDuration))
        newPosition = data.attackPosition + (deltaTime - data.attackDuration).value() * data.desiredVelocity;
    else {
        // release is based on targeting the exact final position.
        Seconds releaseDeltaT = deltaTime - (data.animationDuration - data.releaseDuration);
        newPosition = releaseCurve(Curve::Cubic, releaseDeltaT.value(), data.releaseDuration.value(), data.releasePosition, data.desiredPosition);
    }

    // Normalize velocity to a per second amount. Could be used to check for jank.
    if (lastScrollInterval > 0_s)
        data.currentVelocity = (newPosition - data.currentPosition) / lastScrollInterval.value();
    data.currentPosition = newPosition;

    return true;
}

void ScrollAnimationSmooth::animationTimerFired()
{
    MonotonicTime currentTime = MonotonicTime::now();
    Seconds deltaToNextFrame = 1_s * ceil((currentTime - m_startTime).value() * frameRate) / frameRate - (currentTime - m_startTime);
    currentTime += deltaToNextFrame;

    bool continueAnimation = false;
    if (animateScroll(m_horizontalData, currentTime))
        continueAnimation = true;
    if (animateScroll(m_verticalData, currentTime))
        continueAnimation = true;

    if (continueAnimation)
        startNextTimer(std::max(minimumTimerInterval, deltaToNextFrame));

    m_client.scrollAnimationDidUpdate(*this, { m_horizontalData.currentPosition, m_verticalData.currentPosition });
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
