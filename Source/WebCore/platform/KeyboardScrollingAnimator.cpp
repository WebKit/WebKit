/*
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
#include "KeyboardScrollingAnimator.h"

#include "EventNames.h"
#include "ScrollTypes.h"
#include "ScrollableArea.h"
#include "WritingMode.h"

namespace WebCore {

KeyboardScrollingAnimator::KeyboardScrollingAnimator(ScrollAnimator& scrollAnimator, ScrollController& scrollController)
    : m_scrollAnimator(scrollAnimator)
    , m_scrollController(scrollController)
{
}

RectEdges<bool> KeyboardScrollingAnimator::scrollableDirectionsFromOffset(FloatPoint offset) const
{
    auto minimumScrollPosition = m_scrollAnimator.scrollableArea().minimumScrollPosition();
    auto maximumScrollPosition = m_scrollAnimator.scrollableArea().maximumScrollPosition();

    RectEdges<bool> edges;

    edges.setTop(offset.y() > minimumScrollPosition.y());
    edges.setBottom(offset.y() < maximumScrollPosition.y());
    edges.setLeft(offset.x() > minimumScrollPosition.x());
    edges.setRight(offset.x() < maximumScrollPosition.x());

    return edges;
}

static BoxSide boxSideForDirection(ScrollDirection direction)
{
    switch (direction) {
    case ScrollDirection::ScrollUp:
        return BoxSide::Top;
    case ScrollDirection::ScrollDown:
        return BoxSide::Bottom;
    case ScrollDirection::ScrollLeft:
        return BoxSide::Left;
    case ScrollDirection::ScrollRight:
        return BoxSide::Right;
    }
    ASSERT_NOT_REACHED();
    return BoxSide::Top;
}

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

void KeyboardScrollingAnimator::updateKeyboardScrollPosition(MonotonicTime currentTime)
{
    auto force = FloatSize { };
    auto axesToApplySpring = FloatSize { 1, 1 };
    KeyboardScrollParameters params = KeyboardScrollParameters::parameters();

    if (m_currentKeyboardScroll) {
        auto scrollableDirections = scrollableDirectionsFromOffset(m_scrollAnimator.currentPosition());
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

    ScrollPosition idealPosition = m_scrollAnimator.scrollableArea().constrainScrollPosition(IntPoint(m_currentKeyboardScroll ? m_scrollAnimator.currentPosition() : m_idealPosition));
    FloatSize displacement = m_scrollAnimator.currentPosition() - idealPosition;

    auto springForce = -displacement.scaled(params.springStiffness) - m_velocity.scaled(params.springDamping);
    force += springForce * axesToApplySpring;

    float frameDuration = (currentTime - m_timeAtLastFrame).value();
    m_timeAtLastFrame = currentTime;

    FloatSize acceleration = force.scaled(1. / params.springMass);
    m_velocity += acceleration.scaled(frameDuration);
    FloatPoint newPosition = m_scrollAnimator.currentPosition() + m_velocity.scaled(frameDuration);

    m_scrollAnimator.scrollToPositionWithoutAnimation(newPosition);

    if (!m_scrollTriggeringKeyIsPressed && m_velocity.diagonalLengthSquared() < 1) {
        m_scrollController.stopKeyboardScrolling();
        m_velocity = { };
    }
}

float KeyboardScrollingAnimator::scrollDistance(ScrollDirection direction, ScrollGranularity granularity) const
{
    auto scrollbar = [&] {
        if (direction == ScrollDirection::ScrollUp || direction == ScrollDirection::ScrollDown)
            return m_scrollAnimator.scrollableArea().verticalScrollbar();
        return m_scrollAnimator.scrollableArea().horizontalScrollbar();
    }();

    if (scrollbar) {
        switch (granularity) {
        case ScrollGranularity::ScrollByLine:
            return scrollbar->lineStep();
        case ScrollGranularity::ScrollByPage:
            return scrollbar->pageStep();
        case ScrollGranularity::ScrollByDocument:
            return scrollbar->totalSize();
        case ScrollGranularity::ScrollByPixel:
            return scrollbar->pixelStep();
        }
    }

    return 0;
}

std::optional<KeyboardScroll> KeyboardScrollingAnimator::keyboardScrollForKeyboardEvent(KeyboardEvent& event) const
{
    // FIXME (bug 227459): This logic does not account for writing-mode.

    enum class Key : uint8_t { LeftArrow, RightArrow, UpArrow, DownArrow, Space, PageUp, PageDown };

    Key key;
    if (event.keyIdentifier() == "Left")
        key = Key::LeftArrow;
    else if (event.keyIdentifier() == "Right")
        key = Key::RightArrow;
    else if (event.keyIdentifier() == "Up")
        key = Key::UpArrow;
    else if (event.keyIdentifier() == "Down")
        key = Key::DownArrow;
    else if (event.charCode() == ' ')
        key = Key::Space;
    else if (event.keyIdentifier() == "PageUp")
        key = Key::PageUp;
    else if (event.keyIdentifier() == "PageDown")
        key = Key::PageDown;
    else
        return std::nullopt;

    auto granularity = [&] {
        switch (key) {
        case Key::LeftArrow:
        case Key::RightArrow:
            return event.altKey() ? ScrollGranularity::ScrollByPage : ScrollGranularity::ScrollByLine;
        case Key::UpArrow:
        case Key::DownArrow:
            if (event.metaKey())
                return ScrollGranularity::ScrollByDocument;
            if (event.altKey())
                return ScrollGranularity::ScrollByPage;
            return ScrollGranularity::ScrollByLine;
        case Key::Space:
        case Key::PageUp:
        case Key::PageDown:
            return ScrollGranularity::ScrollByPage;
        };
        RELEASE_ASSERT_NOT_REACHED();
    }();

    auto direction = [&] {
        switch (key) {
        case Key::LeftArrow:
            return ScrollDirection::ScrollLeft;
        case Key::RightArrow:
            return ScrollDirection::ScrollRight;
        case Key::UpArrow:
        case Key::PageUp:
            return ScrollDirection::ScrollUp;
        case Key::DownArrow:
        case Key::PageDown:
            return ScrollDirection::ScrollDown;
        case Key::Space:
            return event.shiftKey() ? ScrollDirection::ScrollUp : ScrollDirection::ScrollDown;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }();

    float distance = scrollDistance(direction, granularity);

    if (!distance)
        return std::nullopt;

    KeyboardScroll scroll;

    scroll.offset = unitVectorForScrollDirection(direction).scaled(distance);
    scroll.granularity = granularity;
    scroll.direction = direction;
    scroll.maximumVelocity = scroll.offset.scaled(KeyboardScrollParameters::parameters().maximumVelocityMultiplier);
    scroll.force = scroll.maximumVelocity.scaled(KeyboardScrollParameters::parameters().springMass / KeyboardScrollParameters::parameters().timeToMaximumVelocity);

    return scroll;
}

bool KeyboardScrollingAnimator::beginKeyboardScrollGesture(KeyboardEvent& event)
{
    auto scroll = keyboardScrollForKeyboardEvent(event);

    if (!scroll)
        return false;

    m_currentKeyboardScroll = scroll;

    if (!(event.type() == eventNames().keydownEvent || event.type() == eventNames().keypressEvent))
        return false;

    if (m_scrollTriggeringKeyIsPressed)
        return false;

    if (m_currentKeyboardScroll->granularity == ScrollGranularity::ScrollByDocument) {
        m_velocity = { };
        stopKeyboardScrollAnimation();
        auto newPosition = IntPoint(m_scrollAnimator.currentPosition() + m_currentKeyboardScroll->offset);
        m_scrollAnimator.scrollToPositionWithAnimation(newPosition);
        return true;
    }

    m_timeAtLastFrame = MonotonicTime::now();
    m_scrollTriggeringKeyIsPressed = true;

    m_idealPositionForMinimumTravel = m_scrollAnimator.currentPosition() + m_currentKeyboardScroll->offset;
    m_scrollController.beginKeyboardScrolling();

    return true;
}

static ScrollPosition farthestPointInDirection(FloatPoint a, FloatPoint b, ScrollDirection direction)
{
    switch (direction) {
    case ScrollDirection::ScrollUp:
        return ScrollPosition(a.x(), std::min(a.y(), b.y()));
    case ScrollDirection::ScrollDown:
        return ScrollPosition(a.x(), std::max(a.y(), b.y()));
    case ScrollDirection::ScrollLeft:
        return ScrollPosition(std::min(a.x(), b.x()), a.y());
    case ScrollDirection::ScrollRight:
        return ScrollPosition(std::max(a.x(), b.x()), a.y());
    }

    ASSERT_NOT_REACHED();
    return { };
}

void KeyboardScrollingAnimator::stopKeyboardScrollAnimation()
{
    if (!m_currentKeyboardScroll)
        return;

    auto params = KeyboardScrollParameters::parameters();

    // Determine the settling position of the spring, conserving the system's current energy.
    // Kinetic = elastic potential
    // 1/2 * m * v^2 = 1/2 * k * x^2
    // x = sqrt(v^2 * m / k)
    auto displacementMagnitudeSquared = (m_velocity * m_velocity).scaled(params.springMass / params.springStiffness);
    FloatSize displacement = {
        std::copysign(sqrt(displacementMagnitudeSquared.width()), m_velocity.width()),
        std::copysign(sqrt(displacementMagnitudeSquared.height()), m_velocity.height())
    };

    // If the spring would settle before the minimum travel distance
    // for an instantaneous tap, move the settling position of the spring
    // out to that point.
    ScrollPosition farthestPoint = farthestPointInDirection(m_scrollAnimator.currentPosition() + displacement, m_idealPositionForMinimumTravel, m_currentKeyboardScroll->direction);
    m_idealPosition = m_scrollAnimator.scrollableArea().constrainScrollPosition(farthestPoint);

    m_currentKeyboardScroll = std::nullopt;
}

void KeyboardScrollingAnimator::handleKeyUpEvent()
{
    if (!m_scrollTriggeringKeyIsPressed)
        return;

    stopKeyboardScrollAnimation();
    m_scrollTriggeringKeyIsPressed = false;
}

} // namespace WebCore
