/*
 * Copyright (c) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(SMOOTH_SCROLLING)

#include "ScrollAnimatorNone.h"

#include "FloatPoint.h"
#include "NotImplemented.h"
#include "OwnArrayPtr.h"
#include "ScrollableArea.h"
#include "ScrollbarTheme.h"
#include "TraceEvent.h"
#include <algorithm>
#include <wtf/CurrentTime.h>
#include <wtf/PassOwnPtr.h>

using namespace std;

namespace WebCore {

static double kFrameRate = 60;
static double kTickTime = 1 / kFrameRate;
static double kMinimumTimerInterval = .001;

PassOwnPtr<ScrollAnimator> ScrollAnimator::create(ScrollableArea* scrollableArea)
{
    if (scrollableArea && scrollableArea->scrollAnimatorEnabled())
        return adoptPtr(new ScrollAnimatorNone(scrollableArea));
    return adoptPtr(new ScrollAnimator(scrollableArea));
}

ScrollAnimatorNone::Parameters::Parameters()
    : m_isEnabled(false)
{
}

ScrollAnimatorNone::Parameters::Parameters(bool isEnabled, double animationTime, double repeatMinimumSustainTime, Curve attackCurve, double attackTime, Curve releaseCurve, double releaseTime)
    : m_isEnabled(isEnabled)
    , m_animationTime(animationTime)
    , m_repeatMinimumSustainTime(repeatMinimumSustainTime)
    , m_attackCurve(attackCurve)
    , m_attackTime(attackTime)
    , m_releaseCurve(releaseCurve)
    , m_releaseTime(releaseTime)
{
}

double ScrollAnimatorNone::PerAxisData::curveAt(Curve curve, double t)
{
    switch (curve) {
    case Linear:
        return t * t;
    case Quadratic:
        return t * t * t;
    case Cubic:
        return t * t * t * t;
    case Bounce:
        if (t < 1 / 2.75)
            return 7.5625 * t * t;
        if (t < 2 / 2.75) {
            double t1 = t - 1.5 / 2.75;
            return 7.5625 * t1 * t1 + .75;
        }
        if (t < 2.5 / 2.75) {
            double t2 = t - 2.25 / 2.75;
            return 7.5625 * t2 * t2 + .9375;
        }
        t -= 2.625 / 2.75;
        return 7.5625 * t * t + .984375;
    }
}

double ScrollAnimatorNone::PerAxisData::attackCurve(Curve curve, double deltaTime, double curveT, double startPosition, double attackPosition)
{
    double t = deltaTime / curveT;
    double positionFactor = curveAt(curve, t);
    return startPosition + positionFactor * (attackPosition - startPosition);
}

double ScrollAnimatorNone::PerAxisData::releaseCurve(Curve curve, double deltaTime, double curveT, double releasePosition, double desiredPosition)
{
    double t = deltaTime / curveT;
    double positionFactor = 1 - curveAt(curve, 1 - t);
    return releasePosition + (positionFactor * (desiredPosition - releasePosition));
}

double ScrollAnimatorNone::PerAxisData::curveDerivativeAt(Curve curve, double t)
{
    switch (curve) {
    case Linear:
        return t * 2;
    case Quadratic:
        return t * t * 3;
    case Cubic:
        return t * t * t * 4;
    case Bounce:
        return t;
    }
}

ScrollAnimatorNone::PerAxisData::PerAxisData(ScrollAnimatorNone* parent, float* currentPosition)
    : m_currentPosition(currentPosition)
{
    reset();
}

void ScrollAnimatorNone::PerAxisData::reset()
{
    m_currentVelocity = 0;

    m_desiredPosition = 0;
    m_desiredVelocity = 0;

    m_startPosition = 0;
    m_startTime = 0;
    m_startVelocity = 0;

    m_animationTime = 0;
    m_lastAnimationTime = 0;

    m_attackPosition = 0;
    m_attackTime = 0;
    m_attackCurve = Quadratic;

    m_releasePosition = 0;
    m_releaseTime = 0;
    m_releaseCurve = Quadratic;
}


bool ScrollAnimatorNone::PerAxisData::updateDataFromParameters(ScrollbarOrientation orientation, float step, float multiplier, float scrollableSize, double currentTime, Parameters* parameters)
{
    if (parameters->m_animationTime > m_animationTime)
        m_animationTime = parameters->m_animationTime;
    m_attackTime = parameters->m_attackTime;
    m_releaseTime = parameters->m_releaseTime;
    m_attackCurve = parameters->m_attackCurve;
    m_releaseCurve = parameters->m_releaseCurve;

    // Prioritize our way out of over constraint.
    if (m_attackTime + m_releaseTime > m_animationTime) {
        if (m_releaseTime > m_animationTime)
            m_releaseTime = m_animationTime;
        m_attackTime = m_animationTime - m_releaseTime;
    }

    m_orientation = orientation;

    if (!m_startTime)
        m_desiredPosition = *m_currentPosition;
    float newPosition = m_desiredPosition + (step * multiplier);

    if (newPosition < 0 || newPosition > scrollableSize)
        newPosition = max(min(newPosition, scrollableSize), 0.0f);

    if (newPosition == m_desiredPosition)
        return false;

    m_desiredPosition = newPosition;

    if (!m_startTime) {
        // FIXME: This should be the time from the event that got us here.
        m_startTime = currentTime - kTickTime / 2;
        m_startPosition = *m_currentPosition;
        m_lastAnimationTime = m_startTime;
    }
    m_startVelocity = m_currentVelocity;

    double remainingDelta = m_desiredPosition - *m_currentPosition;

    double attackAreaLeft = 0;

    double deltaTime = m_lastAnimationTime - m_startTime;
    double attackTimeLeft = max(0., m_attackTime - deltaTime);
    double timeLeft = m_animationTime - deltaTime;
    double minTimeLeft = m_releaseTime + min(parameters->m_repeatMinimumSustainTime, m_animationTime - m_releaseTime - attackTimeLeft);
    if (timeLeft < minTimeLeft) {
        m_animationTime = deltaTime + minTimeLeft;
        timeLeft = minTimeLeft;
    }

    double releaseTimeLeft = min(timeLeft, m_releaseTime);
    double sustainTimeLeft = max(0., timeLeft - releaseTimeLeft - attackTimeLeft);

    if (attackTimeLeft) {
        double attackSpot = deltaTime / m_attackTime;
        attackAreaLeft = attackTimeLeft / (curveDerivativeAt(m_attackCurve, 1) - curveDerivativeAt(m_attackCurve, attackSpot));
    }

    double releaseSpot = (m_releaseTime - releaseTimeLeft) / m_releaseTime;
    double releaseAreaLeft  = releaseTimeLeft / (curveDerivativeAt(m_releaseCurve, 1) - curveDerivativeAt(m_releaseCurve, releaseSpot));

    m_desiredVelocity = remainingDelta / (attackAreaLeft + sustainTimeLeft + releaseAreaLeft);
    m_releasePosition = m_desiredPosition - m_desiredVelocity * releaseAreaLeft;
    if (attackAreaLeft)
        m_attackPosition = m_startPosition + m_desiredVelocity * attackAreaLeft;
    else
        m_attackPosition = m_releasePosition - (m_animationTime - m_releaseTime - m_attackTime) * m_desiredVelocity;

    if (sustainTimeLeft) {
        double roundOff = m_releasePosition - ((attackAreaLeft ? m_attackPosition : *m_currentPosition) + m_desiredVelocity * sustainTimeLeft);
        m_desiredVelocity += roundOff / sustainTimeLeft;
    }

    return true;
}

// FIXME: Add in jank detection trace events into this function.
bool ScrollAnimatorNone::PerAxisData::animateScroll(double currentTime)
{
    double lastScrollInterval = currentTime - m_lastAnimationTime;
    if (lastScrollInterval < kMinimumTimerInterval)
        return true;

    m_lastAnimationTime = currentTime;

    double deltaTime = currentTime - m_startTime;
    double newPosition = *m_currentPosition;

    if (deltaTime > m_animationTime) {
        *m_currentPosition = m_desiredPosition;
        reset();
        return false;
    }
    if (deltaTime < m_attackTime)
        newPosition = attackCurve(m_attackCurve, deltaTime, m_attackTime, m_startPosition, m_attackPosition);
    else if (deltaTime < (m_animationTime - m_releaseTime))
        newPosition = m_attackPosition + (deltaTime - m_attackTime) * m_desiredVelocity;
    else {
        // release is based on targeting the exact final position.
        double releaseDeltaT = deltaTime - (m_animationTime - m_releaseTime);
        newPosition = releaseCurve(m_releaseCurve, releaseDeltaT, m_releaseTime, m_releasePosition, m_desiredPosition);
    }

    // Normalize velocity to a per second amount. Could be used to check for jank.
    if (lastScrollInterval > 0)
        m_currentVelocity = (newPosition - *m_currentPosition) / lastScrollInterval;
    *m_currentPosition = newPosition;

    return true;
}

ScrollAnimatorNone::ScrollAnimatorNone(ScrollableArea* scrollableArea)
    : ScrollAnimator(scrollableArea)
    , m_horizontalData(this, &m_currentPosX)
    , m_verticalData(this, &m_currentPosY)
    , m_animationTimer(this, &ScrollAnimatorNone::animationTimerFired)
{
}

ScrollAnimatorNone::~ScrollAnimatorNone()
{
    stopAnimationTimerIfNeeded();
}

bool ScrollAnimatorNone::scroll(ScrollbarOrientation orientation, ScrollGranularity granularity, float step, float multiplier)
{
    if (!m_scrollableArea->scrollAnimatorEnabled())
        return ScrollAnimator::scroll(orientation, granularity, step, multiplier);

    // FIXME: get the type passed in. MouseWheel could also be by line, but should still have different
    // animation parameters than the keyboard.
    Parameters parameters;
    switch (granularity) {
    case ScrollByDocument:
        break;
    case ScrollByLine:
        parameters = Parameters(true, 10 * kTickTime, 7 * kTickTime, Quadratic, 3 * kTickTime, Quadratic, 3 * kTickTime);
        break;
    case ScrollByPage:
        parameters = Parameters(true, 15 * kTickTime, 10 * kTickTime, Quadratic, 5 * kTickTime, Quadratic, 5 * kTickTime);
        break;
    case ScrollByPixel:
        parameters = Parameters(true, 11 * kTickTime, 2 * kTickTime, Quadratic, 3 * kTickTime, Quadratic, 3 * kTickTime);
        break;
    default:
        break;
    }

    // If the individual input setting is disabled, bail.
    if (!parameters.m_isEnabled)
        return ScrollAnimator::scroll(orientation, granularity, step, multiplier);

    // This is an animatable scroll. Set the animation in motion using the appropriate parameters.
    float scrollableSize = static_cast<float>(m_scrollableArea->scrollSize(orientation));

    PerAxisData& data = (orientation == VerticalScrollbar) ? m_verticalData : m_horizontalData;
    bool needToScroll = data.updateDataFromParameters(orientation, step, multiplier, scrollableSize, WTF::monotonicallyIncreasingTime(), &parameters);
    if (needToScroll && !m_animationTimer.isActive()) {
        m_startTime = data.m_startTime;
        animationTimerFired(&m_animationTimer);
    }
    return needToScroll;
}

void ScrollAnimatorNone::scrollToOffsetWithoutAnimation(const FloatPoint& offset)
{
    stopAnimationTimerIfNeeded();

    m_horizontalData.reset();
    *m_horizontalData.m_currentPosition = offset.x();
    m_horizontalData.m_desiredPosition = offset.x();

    m_verticalData.reset();
    *m_verticalData.m_currentPosition = offset.y();
    m_verticalData.m_desiredPosition = offset.y();

    notifyPositionChanged();
}

void ScrollAnimatorNone::animationTimerFired(Timer<ScrollAnimatorNone>* timer)
{
    double currentTime = WTF::monotonicallyIncreasingTime();
    double deltaToNextFrame = ceil((currentTime - m_startTime) * kFrameRate) / kFrameRate - (currentTime - m_startTime);

    bool continueAnimation = false;
    if (m_horizontalData.m_startTime && m_horizontalData.animateScroll(currentTime + deltaToNextFrame))
        continueAnimation = true;
    if (m_verticalData.m_startTime && m_verticalData.animateScroll(currentTime + deltaToNextFrame))
        continueAnimation = true;
    if (continueAnimation) {
        double nextTimerInterval = max(kMinimumTimerInterval, deltaToNextFrame);
        timer->startOneShot(nextTimerInterval);
    }
    notifyPositionChanged();
}

void ScrollAnimatorNone::stopAnimationTimerIfNeeded()
{
    if (m_animationTimer.isActive())
        m_animationTimer.stop();
}

} // namespace WebCore

#endif // ENABLE(SMOOTH_SCROLLING)
