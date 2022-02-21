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
#include "PlatformKeyboardEvent.h"
#include "ScrollTypes.h"
#include "ScrollableArea.h"
#include "WritingMode.h"

namespace WebCore {

enum class KeyboardScrollingKey : uint8_t {
    LeftArrow,
    RightArrow,
    UpArrow,
    DownArrow,
    Space,
    PageUp,
    PageDown
};

KeyboardScrollingAnimator::KeyboardScrollingAnimator(ScrollAnimator& scrollAnimator, ScrollingEffectsController& scrollController)
    : m_scrollAnimator(scrollAnimator)
    , m_scrollController(scrollController)
{
}

RectEdges<bool> KeyboardScrollingAnimator::scrollableDirectionsFromPosition(FloatPoint position) const
{
    auto minimumScrollPosition = m_scrollAnimator.scrollableArea().minimumScrollPosition();
    auto maximumScrollPosition = m_scrollAnimator.scrollableArea().maximumScrollPosition();

    RectEdges<bool> edges;

    edges.setTop(position.y() > minimumScrollPosition.y());
    edges.setBottom(position.y() < maximumScrollPosition.y());
    edges.setLeft(position.x() > minimumScrollPosition.x());
    edges.setRight(position.x() < maximumScrollPosition.x());

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
        auto scrollableDirections = scrollableDirectionsFromPosition(m_scrollAnimator.currentPosition());
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

    ScrollPosition idealPosition = m_scrollAnimator.scrollableArea().constrainedScrollPosition(IntPoint(m_currentKeyboardScroll ? m_scrollAnimator.currentPosition() : m_idealPosition));
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
        m_scrollController.didStopKeyboardScrolling();
        m_velocity = { };
    }
}

float KeyboardScrollingAnimator::scrollDistance(ScrollDirection direction, ScrollGranularity granularity) const
{
    auto scrollbar = m_scrollAnimator.scrollableArea().scrollbarForDirection(direction);
    if (scrollbar) {
        switch (granularity) {
        case ScrollGranularity::Line:
            return scrollbar->lineStep();
        case ScrollGranularity::Page:
            return scrollbar->pageStep();
        case ScrollGranularity::Document:
            return scrollbar->totalSize();
        case ScrollGranularity::Pixel:
            return scrollbar->pixelStep();
        }
    }

    return 0;
}

static std::optional<KeyboardScrollingKey> keyboardScrollingKeyFromEvent(const PlatformKeyboardEvent& event)
{
    auto identifier = event.keyIdentifier();
    if (identifier == "Left")
        return KeyboardScrollingKey::LeftArrow;
    if (identifier == "Right")
        return KeyboardScrollingKey::RightArrow;
    if (identifier == "Up")
        return KeyboardScrollingKey::UpArrow;
    if (identifier == "Down")
        return KeyboardScrollingKey::DownArrow;
    if (identifier == "PageUp")
        return KeyboardScrollingKey::PageUp;
    if (identifier == "PageDown")
        return KeyboardScrollingKey::PageDown;

    if (event.text().characterStartingAt(0) == ' ')
        return KeyboardScrollingKey::Space;

    return { };
}

std::optional<KeyboardScroll> KeyboardScrollingAnimator::keyboardScrollForKeyboardEvent(const PlatformKeyboardEvent& event) const
{
    auto key = keyboardScrollingKeyFromEvent(event);
    if (!key)
        return { };

    // FIXME (bug 227459): This logic does not account for writing-mode.
    auto granularity = [&] {
        switch (key.value()) {
        case KeyboardScrollingKey::LeftArrow:
        case KeyboardScrollingKey::RightArrow:
            return event.altKey() ? ScrollGranularity::Page : ScrollGranularity::Line;
        case KeyboardScrollingKey::UpArrow:
        case KeyboardScrollingKey::DownArrow:
            if (event.metaKey())
                return ScrollGranularity::Document;
            if (event.altKey())
                return ScrollGranularity::Page;
            return ScrollGranularity::Line;
        case KeyboardScrollingKey::Space:
        case KeyboardScrollingKey::PageUp:
        case KeyboardScrollingKey::PageDown:
            return ScrollGranularity::Page;
        };
        RELEASE_ASSERT_NOT_REACHED();
    }();

    auto direction = [&] {
        switch (key.value()) {
        case KeyboardScrollingKey::LeftArrow:
            return ScrollDirection::ScrollLeft;
        case KeyboardScrollingKey::RightArrow:
            return ScrollDirection::ScrollRight;
        case KeyboardScrollingKey::UpArrow:
        case KeyboardScrollingKey::PageUp:
            return ScrollDirection::ScrollUp;
        case KeyboardScrollingKey::DownArrow:
        case KeyboardScrollingKey::PageDown:
            return ScrollDirection::ScrollDown;
        case KeyboardScrollingKey::Space:
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

bool KeyboardScrollingAnimator::beginKeyboardScrollGesture(const PlatformKeyboardEvent& event)
{
    auto scroll = keyboardScrollForKeyboardEvent(event);
    if (!scroll)
        return false;

    m_currentKeyboardScroll = scroll;

    // PlatformEvent::Char is a "keypress" event.
    if (!(event.type() == PlatformEvent::RawKeyDown || event.type() == PlatformEvent::Char))
        return false;

    if (m_scrollTriggeringKeyIsPressed)
        return false;

    if (m_currentKeyboardScroll->granularity == ScrollGranularity::Document) {
        m_velocity = { };
        stopKeyboardScrollAnimation();
        auto newPosition = IntPoint(m_scrollAnimator.currentPosition() + m_currentKeyboardScroll->offset);
        m_scrollAnimator.scrollToPositionWithAnimation(newPosition);
        return true;
    }

    m_timeAtLastFrame = MonotonicTime::now();
    m_scrollTriggeringKeyIsPressed = true;

    m_idealPositionForMinimumTravel = m_scrollAnimator.currentPosition() + m_currentKeyboardScroll->offset;
    m_scrollController.willBeginKeyboardScrolling();

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
    m_idealPosition = m_scrollAnimator.scrollableArea().constrainedScrollPosition(farthestPoint);

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
