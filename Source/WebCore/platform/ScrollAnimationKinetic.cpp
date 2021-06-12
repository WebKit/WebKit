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

#include "config.h"
#include "ScrollAnimationKinetic.h"

#include "PlatformWheelEvent.h"

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

/*
 * PerAxisData is a port of GtkKineticScrolling as of GTK+ 3.20,
 * mimicking its API and its behavior.
 *
 * All our curves are second degree linear differential equations, and
 * so they can always be written as linear combinations of 2 base
 * solutions. coef1 and coef2 are the coefficients to these two base
 * solutions, and are computed from the initial position and velocity.
 *
 * In the case of simple deceleration, the differential equation is
 *
 *   y'' = -my'
 *
 * With m the resistence factor. For this we use the following 2
 * base solutions:
 *
 *   f1(x) = 1
 *   f2(x) = exp(-mx)
 *
 * In the case of overshoot, the differential equation is
 *
 *   y'' = -my' - ky
 *
 * With m the resistance, and k the spring stiffness constant. We let
 * k = m^2 / 4, so that the system is critically damped (ie, returns to its
 * equilibrium position as quickly as possible, without oscillating), and offset
 * the whole thing, such that the equilibrium position is at 0. This gives the
 * base solutions
 *
 *   f1(x) = exp(-mx / 2)
 *   f2(x) = t exp(-mx / 2)
 */

static constexpr double decelFriction = 4;
static constexpr double frameRate = 60;
static constexpr double velocityAccumulationFloor = 0.33;
static constexpr double velocityAccumulationCeil = 1.0;
static constexpr double velocityAccumulationMax = 6.0;
static constexpr Seconds tickTime = 1_s / frameRate;
static constexpr Seconds minimumTimerInterval { 1_ms };
static constexpr Seconds scrollCaptureThreshold { 150_ms };

namespace WebCore {

ScrollAnimationKinetic::PerAxisData::PerAxisData(double lower, double upper, double initialPosition, double initialVelocity)
    : m_lower(lower)
    , m_upper(upper)
    , m_coef1(initialVelocity / decelFriction + initialPosition)
    , m_coef2(-initialVelocity / decelFriction)
    , m_position(clampTo(initialPosition, lower, upper))
    , m_velocity(initialPosition < lower || initialPosition > upper ? 0 : initialVelocity)
{
}

bool ScrollAnimationKinetic::PerAxisData::animateScroll(Seconds timeDelta)
{
    auto lastPosition = m_position;
    auto lastTime = m_elapsedTime;
    m_elapsedTime += timeDelta;

    double exponentialPart = exp(-decelFriction * m_elapsedTime.value());
    m_position = m_coef1 + m_coef2 * exponentialPart;
    m_velocity = -decelFriction * m_coef2 * exponentialPart;

    if (m_position < m_lower) {
        m_velocity = m_lower - m_position;
        m_position = m_lower;
    } else if (m_position > m_upper) {
        m_velocity = m_upper - m_position;
        m_position = m_upper;
    }

    if (fabs(m_velocity) < 1 || (lastTime && fabs(m_position - lastPosition) < 1)) {
        m_position = round(m_position);
        m_velocity = 0;
    }

    return m_velocity;
}

ScrollAnimationKinetic::ScrollAnimationKinetic(ScrollExtentsCallback&& scrollExtentsFunction, NotifyPositionChangedCallback&& notifyPositionChangedFunction)
    : m_scrollExtentsFunction(WTFMove(scrollExtentsFunction))
    , m_notifyPositionChangedFunction(WTFMove(notifyPositionChangedFunction))
    , m_animationTimer(RunLoop::current(), this, &ScrollAnimationKinetic::animationTimerFired)
{
#if USE(GLIB_EVENT_LOOP)
    m_animationTimer.setPriority(WTF::RunLoopSourcePriority::DisplayRefreshMonitorTimer);
#endif
}

ScrollAnimationKinetic::~ScrollAnimationKinetic() = default;

void ScrollAnimationKinetic::stop()
{
    m_animationTimer.stop();
}

bool ScrollAnimationKinetic::isActive() const
{
    return m_animationTimer.isActive();
}

void ScrollAnimationKinetic::appendToScrollHistory(const PlatformWheelEvent& event)
{
    m_scrollHistory.removeAllMatching([&event] (PlatformWheelEvent& otherEvent) -> bool {
        return (event.timestamp() - otherEvent.timestamp()) > scrollCaptureThreshold;
    });

    m_scrollHistory.append(event);
}

void ScrollAnimationKinetic::clearScrollHistory()
{
    m_scrollHistory.clear();
}

FloatPoint ScrollAnimationKinetic::computeVelocity()
{
    if (m_scrollHistory.isEmpty())
        return { };

    auto first = m_scrollHistory[0].timestamp();
    auto last = m_scrollHistory.rbegin()->timestamp();

    if (last == first)
        return { };

    FloatPoint accumDelta;
    for (const auto& scrollEvent : m_scrollHistory)
        accumDelta += FloatPoint(scrollEvent.deltaX(), scrollEvent.deltaY());

    m_scrollHistory.clear();

    return FloatPoint(accumDelta.x() * -1 / (last - first).value(), accumDelta.y() * -1 / (last - first).value());
}

void ScrollAnimationKinetic::start(const FloatPoint& initialPosition, const FloatPoint& velocity, bool mayHScroll, bool mayVScroll)
{
    stop();

    if (!velocity.x() && !velocity.y()) {
        m_position = initialPosition;
        m_horizontalData = std::nullopt;
        m_verticalData = std::nullopt;
        return;
    }

    auto delta = deltaToNextFrame();
    auto extents = m_scrollExtentsFunction();

    auto accumulateVelocity = [&](double velocity, std::optional<PerAxisData> axisData) -> double {
        if (axisData && axisData.value().animateScroll(delta)) {
            double lastVelocity = axisData.value().velocity();
            if ((std::signbit(lastVelocity) == std::signbit(velocity))
                && (std::abs(velocity) >= std::abs(lastVelocity * velocityAccumulationFloor))) {
                double minVelocity = lastVelocity * velocityAccumulationFloor;
                double maxVelocity = lastVelocity * velocityAccumulationCeil;
                double accumulationMultiplier = (velocity - minVelocity) / (maxVelocity - minVelocity);
                velocity += lastVelocity * std::min(accumulationMultiplier, velocityAccumulationMax);
            }
        }

        return velocity;
    };

    if (mayHScroll) {
        m_horizontalData = PerAxisData(extents.minimumScrollPosition.x(),
            extents.maximumScrollPosition.x(),
            initialPosition.x(), accumulateVelocity(velocity.x(), m_horizontalData));
    } else
        m_horizontalData = std::nullopt;

    if (mayVScroll) {
        m_verticalData = PerAxisData(extents.minimumScrollPosition.y(),
            extents.maximumScrollPosition.y(),
            initialPosition.y(), accumulateVelocity(velocity.y(), m_verticalData));
    } else
        m_verticalData = std::nullopt;

    m_position = initialPosition;
    m_startTime = MonotonicTime::now() - tickTime / 2.;
    animationTimerFired();
}

void ScrollAnimationKinetic::animationTimerFired()
{
    auto delta = deltaToNextFrame();

    if (m_horizontalData && !m_horizontalData.value().animateScroll(delta))
        m_horizontalData = std::nullopt;

    if (m_verticalData && !m_verticalData.value().animateScroll(delta))
        m_verticalData = std::nullopt;

    // If one of the axes didn't finish its animation we must continue it.
    if (m_horizontalData || m_verticalData)
        m_animationTimer.startOneShot(std::max(minimumTimerInterval, delta));

    double x = m_horizontalData ? m_horizontalData.value().position() : m_position.x();
    double y = m_verticalData ? m_verticalData.value().position() : m_position.y();
    m_position = FloatPoint(x, y);
    m_notifyPositionChangedFunction(FloatPoint(m_position));
}

Seconds ScrollAnimationKinetic::deltaToNextFrame()
{
    MonotonicTime currentTime = MonotonicTime::now();
    return 1_s * ceil((currentTime - m_startTime).value() * frameRate) / frameRate - (currentTime - m_startTime);
}

} // namespace WebCore
