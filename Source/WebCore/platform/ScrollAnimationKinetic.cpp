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

#include "config.h"
#include "ScrollAnimationKinetic.h"

#include "PlatformWheelEvent.h"
#include "ScrollExtents.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

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
static constexpr double velocityAccumulationFloor = 0.33;
static constexpr double velocityAccumulationCeil = 1.0;
static constexpr double velocityAccumulationMax = 6.0;
static constexpr Seconds scrollCaptureThreshold { 150_ms };

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollAnimationKinetic);

ScrollAnimationKinetic::PerAxisData::PerAxisData(double lower, double upper, double initialOffset, double initialVelocity)
    : m_lower(lower)
    , m_upper(upper)
    , m_coef1(initialVelocity / decelFriction + initialOffset)
    , m_coef2(-initialVelocity / decelFriction)
    , m_offset(clampTo(initialOffset, lower, upper))
    , m_velocity(initialOffset < lower || initialOffset > upper ? 0 : initialVelocity)
{
}

bool ScrollAnimationKinetic::PerAxisData::animateScroll(Seconds elapsedTime)
{
    auto lastOffset = m_offset;
    auto lastTime = m_elapsedTime;
    m_elapsedTime = elapsedTime;

    double exponentialPart = exp(-decelFriction * m_elapsedTime.value());
    m_offset = m_coef1 + m_coef2 * exponentialPart;
    m_velocity = -decelFriction * m_coef2 * exponentialPart;

    if (m_offset < m_lower) {
        m_velocity = m_lower - m_offset;
        m_offset = m_lower;
    } else if (m_offset > m_upper) {
        m_velocity = m_upper - m_offset;
        m_offset = m_upper;
    }

    if (std::abs(m_velocity) < 1 || (lastTime > 0_s && std::abs(m_offset - lastOffset) < 1)) {
        m_offset = round(m_offset);
        m_velocity = 0;
    }

    return m_velocity;
}

ScrollAnimationKinetic::ScrollAnimationKinetic(ScrollAnimationClient& client)
    : ScrollAnimation(Type::Kinetic, client)
{
}

ScrollAnimationKinetic::~ScrollAnimationKinetic() = default;

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

FloatSize ScrollAnimationKinetic::computeVelocity()
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

    // FIXME: It's odd that computeVelocity() has the side effect of clearing history.
    m_scrollHistory.clear();

    return FloatSize(accumDelta.x() * -1 / (last - first).value(), accumDelta.y() * -1 / (last - first).value());
}

bool ScrollAnimationKinetic::startAnimatedScrollWithInitialVelocity(const FloatPoint& initialOffset, const FloatSize& velocity, const FloatSize& previousVelocity, bool mayHScroll, bool mayVScroll)
{
    m_initialOffset = initialOffset;
    m_initialVelocity = velocity;

    stop();

    if (velocity.isZero()) {
        m_horizontalData = std::nullopt;
        m_verticalData = std::nullopt;
        return false;
    }

    auto extents = m_client.scrollExtentsForAnimation(*this);

    auto accumulateVelocity = [&](double velocity, double previousVelocity) -> double {
        if ((std::signbit(previousVelocity) == std::signbit(velocity))
            && (std::abs(velocity) >= std::abs(previousVelocity * velocityAccumulationFloor))) {
            double minVelocity = previousVelocity * velocityAccumulationFloor;
            double maxVelocity = previousVelocity * velocityAccumulationCeil;
            double accumulationMultiplier = (velocity - minVelocity) / (maxVelocity - minVelocity);
            velocity += previousVelocity * std::min(accumulationMultiplier, velocityAccumulationMax);
        }

        return velocity;
    };

    if (mayHScroll) {
        m_horizontalData = PerAxisData(extents.minimumScrollOffset().x(),
            extents.maximumScrollOffset().x(),
            initialOffset.x(), accumulateVelocity(velocity.width(), previousVelocity.width()));
    } else
        m_horizontalData = std::nullopt;

    if (mayVScroll) {
        m_verticalData = PerAxisData(extents.minimumScrollOffset().y(),
            extents.maximumScrollOffset().y(),
            initialOffset.y(), accumulateVelocity(velocity.height(), previousVelocity.height()));
    } else
        m_verticalData = std::nullopt;

    m_currentOffset = initialOffset;
    didStart(MonotonicTime::now());
    return true;
}

bool ScrollAnimationKinetic::retargetActiveAnimation(const FloatPoint&)
{
    if (!isActive())
        return false;
    
    // FIXME: Implement retargeting.
    return false;
}

void ScrollAnimationKinetic::serviceAnimation(MonotonicTime currentTime)
{
    auto elapsedTime = timeSinceStart(currentTime);

    if (m_horizontalData && !m_horizontalData.value().animateScroll(elapsedTime))
        m_horizontalData = std::nullopt;

    if (m_verticalData && !m_verticalData.value().animateScroll(elapsedTime))
        m_verticalData = std::nullopt;

    double x = m_horizontalData ? m_horizontalData.value().offset() : m_currentOffset.x();
    double y = m_verticalData ? m_verticalData.value().offset() : m_currentOffset.y();
    m_currentOffset = FloatPoint(x, y);
    m_client.scrollAnimationDidUpdate(*this, m_currentOffset);

    if (!m_horizontalData && !m_verticalData)
        didEnd();
}

String ScrollAnimationKinetic::debugDescription() const
{
    TextStream textStream;
    textStream << "ScrollAnimationKinetic " << this << " active " << isActive() << " current offset " << currentOffset();
    return textStream.release();
}

FloatSize ScrollAnimationKinetic::accumulateVelocityFromPreviousGesture(const MonotonicTime lastStartTime, const FloatPoint& lastInitialOffset, const FloatSize& lastInitialVelocity)
{
    auto elapsedTime = MonotonicTime::now() - lastStartTime;
    auto extents = m_client.scrollExtentsForAnimation(*this);

    auto horizontalData = PerAxisData(extents.minimumScrollOffset().x(),
        extents.maximumScrollOffset().x(),
        lastInitialOffset.x(), lastInitialVelocity.width());

    auto verticalData = PerAxisData(extents.minimumScrollOffset().y(),
        extents.maximumScrollOffset().y(),
        lastInitialOffset.y(), lastInitialVelocity.height());

    horizontalData.animateScroll(elapsedTime);
    verticalData.animateScroll(elapsedTime);

    return FloatSize(horizontalData.velocity(), verticalData.velocity());
}

} // namespace WebCore
