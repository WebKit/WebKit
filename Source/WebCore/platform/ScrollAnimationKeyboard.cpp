/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "ScrollAnimationKeyboard.h"

#include "FloatPoint.h"
#include "GeometryUtilities.h"
#include "ScrollExtents.h"
#include "ScrollableArea.h"
#include "TimingFunction.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

static FloatSize perpendicularAbsoluteUnitVector(ScrollDirection direction)
{
    switch (direction) {
    case ScrollDirection::ScrollUp:
    case ScrollDirection::ScrollDown:
        return { 1, 0 };
    case ScrollDirection::ScrollLeft:
    case ScrollDirection::ScrollRight:
        return { 0, 1 };
    }
    ASSERT_NOT_REACHED();
    return { };
}

static FloatPoint farthestPointInDirection(FloatPoint a, FloatPoint b, ScrollDirection direction)
{
    switch (direction) {
    case ScrollDirection::ScrollUp:
        return { a.x(), std::min(a.y(), b.y()) };
    case ScrollDirection::ScrollDown:
        return { a.x(), std::max(a.y(), b.y()) };
    case ScrollDirection::ScrollLeft:
        return { std::min(a.x(), b.x()), a.y() };
    case ScrollDirection::ScrollRight:
        return { std::max(a.x(), b.x()), a.y() };
    }

    ASSERT_NOT_REACHED();
    return { };
}

RectEdges<bool> ScrollAnimationKeyboard::scrollableDirectionsFromPosition(FloatPoint position)
{
    auto extents = m_client.scrollExtentsForAnimation(*this);

    auto minimumScrollPosition = extents.minimumScrollOffset();
    auto maximumScrollPosition = extents.maximumScrollOffset();

    RectEdges<bool> edges;

    edges.setTop(position.y() > minimumScrollPosition.y());
    edges.setBottom(position.y() < maximumScrollPosition.y());
    edges.setLeft(position.x() > minimumScrollPosition.x());
    edges.setRight(position.x() < maximumScrollPosition.x());

    return edges;
}

ScrollAnimationKeyboard::ScrollAnimationKeyboard(ScrollAnimationClient& client)
    : ScrollAnimation(Type::Keyboard, client)
{
}

ScrollAnimationKeyboard::~ScrollAnimationKeyboard() = default;

bool ScrollAnimationKeyboard::retargetActiveAnimation(const FloatPoint&)
{
    if (!isActive())
        return false;

    return true;
}

void ScrollAnimationKeyboard::serviceAnimation(MonotonicTime currentTime)
{
    animateScroll(currentTime);
}

bool ScrollAnimationKeyboard::startKeyboardScroll(const KeyboardScroll& scrollData)
{
    m_timeAtLastFrame = MonotonicTime::now();
    m_currentKeyboardScroll = scrollData;
    m_scrollTriggeringKeyIsPressed = true;
    m_currentOffset = m_client.scrollOffset(*this);
    m_idealPositionForMinimumTravel = {
        (m_currentOffset + m_currentKeyboardScroll->offset).x(),
        (m_currentOffset + m_currentKeyboardScroll->offset).y() };

    if (!isActive())
        didStart(MonotonicTime::now());

    return true;
}

void ScrollAnimationKeyboard::stopKeyboardScrollAnimation()
{
    if (!m_currentKeyboardScroll)
        return;

    auto params = KeyboardScrollParameters::parameters();

    // Determine the settling position of the spring, conserving the system's current energy.
    // Kinetic = elastic potential
    // 1/2 * m * v^2 = 1/2 * k * x^2
    // x = sqrt(v^2 * m / k)
    auto displacementMagnitudeSquared = (m_velocity * m_velocity).scaled(params.springMass / params.springStiffness);
    FloatPoint displacement = {
        std::copysign(sqrt(displacementMagnitudeSquared.width()), m_velocity.width()),
        std::copysign(sqrt(displacementMagnitudeSquared.height()), m_velocity.height())
    };

    // If the spring would settle before the minimum travel distance
    // for an instantaneous tap, move the settling position of the spring
    // out to that point.
    FloatPoint farthestPoint = farthestPointInDirection(m_currentOffset + displacement, { m_idealPositionForMinimumTravel.width(), m_idealPositionForMinimumTravel.height() }, m_currentKeyboardScroll->direction);

    auto extents = m_client.scrollExtentsForAnimation(*this);

    m_idealPosition = farthestPoint.constrainedBetween(extents.minimumScrollOffset(), extents.maximumScrollOffset());

    m_currentKeyboardScroll = std::nullopt;
}

void ScrollAnimationKeyboard::finishKeyboardScroll(bool immediate)
{
    if (!m_scrollTriggeringKeyIsPressed && !immediate)
        return;

    if (!immediate)
        stopKeyboardScrollAnimation();
    
    m_scrollTriggeringKeyIsPressed = false;

    if (immediate) {
        didEnd();
        m_velocity = { };
    }
}

bool ScrollAnimationKeyboard::animateScroll(MonotonicTime currentTime)
{
    auto force = FloatSize { };
    auto axesToApplySpring = FloatSize { 1, 1 };
    KeyboardScrollParameters params = KeyboardScrollParameters::parameters();

    if (m_currentKeyboardScroll) {
        auto scrollableDirections = scrollableDirectionsFromPosition(m_currentOffset);
        auto direction = m_currentKeyboardScroll->direction;

        if (scrollableDirections.at(boxSideForDirection(direction))) {
            // Apply the scrolling force. Only apply the spring in the perpendicular axis,
            // otherwise it drags against the direction of motion.
            axesToApplySpring = perpendicularAbsoluteUnitVector(direction);
            force = m_currentKeyboardScroll->force;
        } else {
            // The scroll view cannot scroll in this direction, and is rubber-banding.
            // Apply a constant and significant force; otherwise, the force for a
            // single-line increment is not strong enough to rubber-band perceptibly.
            force = unitVectorForScrollDirection(direction).scaled(params.rubberBandForce);
        }

        if (fabs(m_velocity.width()) >= fabs(m_currentKeyboardScroll->maximumVelocity.width()))
            force.setWidth(0);

        if (fabs(m_velocity.height()) >= fabs(m_currentKeyboardScroll->maximumVelocity.height()))
            force.setHeight(0);
    }

    auto extents = m_client.scrollExtentsForAnimation(*this);

    FloatPoint idealPosition = (IntPoint(m_currentKeyboardScroll ? m_currentOffset : m_idealPosition).constrainedBetween(IntPoint(extents.minimumScrollOffset()), IntPoint(extents.maximumScrollOffset())));
    FloatSize displacement = m_currentOffset - idealPosition;

    auto springForce = -displacement.scaled(params.springStiffness) - m_velocity.scaled(params.springDamping);
    force += springForce * axesToApplySpring;

    float frameDuration = (currentTime - m_timeAtLastFrame).value();
    m_timeAtLastFrame = currentTime;

    FloatSize acceleration = force.scaled(1. / params.springMass);
    m_velocity += acceleration.scaled(frameDuration);
    m_currentOffset = m_currentOffset + m_velocity.scaled(frameDuration);

    m_client.scrollAnimationDidUpdate(*this, m_currentOffset);

    // Stop the spring if it reaches the ideal position.
    FloatSize newDisplacement = m_currentOffset - idealPosition;

    if (axesToApplySpring.width() && displacement.width() * newDisplacement.width() < 0)
        m_velocity.setWidth(0);
    if (axesToApplySpring.height() && displacement.height() * newDisplacement.height() < 0)
        m_velocity.setHeight(0);

    if (!m_scrollTriggeringKeyIsPressed && m_velocity.diagonalLengthSquared() < 1) {
        didEnd();
        m_velocity = { };
    }

    return true;
}

String ScrollAnimationKeyboard::debugDescription() const
{
    TextStream textStream;
    textStream << "ScrollAnimationKeyboard " << this << " active " << isActive() << " current offset " << currentOffset() << " current velocity " << m_velocity;
    return textStream.release();
}

} // namespace WebCore
