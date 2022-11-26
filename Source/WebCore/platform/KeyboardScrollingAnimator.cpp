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
#include "FrameView.h"
#include "PlatformKeyboardEvent.h"
#include "ScrollAnimator.h"
#include "ScrollTypes.h"
#include <wtf/SortedArrayMap.h>

namespace WebCore {

KeyboardScrollingAnimator::KeyboardScrollingAnimator(ScrollableArea& scrollableArea)
    : m_scrollableArea(scrollableArea)
{
}

RectEdges<bool> KeyboardScrollingAnimator::scrollableDirectionsFromPosition(FloatPoint position) const
{
    auto minimumScrollPosition = m_scrollableArea.minimumScrollPosition();
    auto maximumScrollPosition = m_scrollableArea.maximumScrollPosition();

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

const std::optional<KeyboardScrollingKey> keyboardScrollingKeyForKeyboardEvent(const KeyboardEvent& event)
{
    auto* platformEvent = event.underlyingPlatformEvent();
    if (!platformEvent)
        return { };

    // PlatformEvent::Char is a "keypress" event.
    if (!(platformEvent->type() == PlatformEvent::RawKeyDown || platformEvent->type() == PlatformEvent::Char))
        return { };

    static constexpr std::pair<PackedASCIILiteral<uint64_t>, KeyboardScrollingKey> mappings[] = {
        { "Down", KeyboardScrollingKey::DownArrow },
        { "End", KeyboardScrollingKey::End },
        { "Home", KeyboardScrollingKey::Home },
        { "Left", KeyboardScrollingKey::LeftArrow },
        { "PageDown", KeyboardScrollingKey::PageDown },
        { "PageUp", KeyboardScrollingKey::PageUp },
        { "Right", KeyboardScrollingKey::RightArrow },
        { "Up", KeyboardScrollingKey::UpArrow },
    };
    static constexpr SortedArrayMap map { mappings };

    auto identifier = platformEvent->keyIdentifier();
    if (auto* result = map.tryGet(identifier))
        return *result;

    if (platformEvent->text().characterStartingAt(0) == ' ')
        return KeyboardScrollingKey::Space;

    return { };
}

const std::optional<ScrollDirection> scrollDirectionForKeyboardEvent(const KeyboardEvent& event)
{
    auto key = keyboardScrollingKeyForKeyboardEvent(event);
    if (!key)
        return { };

    // FIXME (bug 227459): This logic does not account for writing-mode.
    auto direction = [&] {
        switch (key.value()) {
        case KeyboardScrollingKey::LeftArrow:
            return ScrollDirection::ScrollLeft;
        case KeyboardScrollingKey::RightArrow:
            return ScrollDirection::ScrollRight;
        case KeyboardScrollingKey::UpArrow:
        case KeyboardScrollingKey::PageUp:
        case KeyboardScrollingKey::Home:
            return ScrollDirection::ScrollUp;
        case KeyboardScrollingKey::DownArrow:
        case KeyboardScrollingKey::PageDown:
        case KeyboardScrollingKey::End:
            return ScrollDirection::ScrollDown;
        case KeyboardScrollingKey::Space:
            return event.shiftKey() ? ScrollDirection::ScrollUp : ScrollDirection::ScrollDown;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }();

    return direction;
}

const std::optional<ScrollGranularity> scrollGranularityForKeyboardEvent(const KeyboardEvent& event)
{
    auto key = keyboardScrollingKeyForKeyboardEvent(event);
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
        case KeyboardScrollingKey::Home:
        case KeyboardScrollingKey::End:
            return ScrollGranularity::Document;
        };
        RELEASE_ASSERT_NOT_REACHED();
    }();

    return granularity;
}

float KeyboardScrollingAnimator::scrollDistance(ScrollDirection direction, ScrollGranularity granularity) const
{
    auto scrollbar = m_scrollableArea.scrollbarForDirection(direction);
    if (!scrollbar)
        return false;

    float step = 0;
    switch (granularity) {
    case ScrollGranularity::Line:
        step = scrollbar->lineStep();
        break;
    case ScrollGranularity::Page:
        step = scrollbar->pageStep();
        break;
    case ScrollGranularity::Document:
        step = scrollbar->totalSize();
        break;
    case ScrollGranularity::Pixel:
        step = scrollbar->pixelStep();
        break;
    }

    auto axis = axisFromDirection(direction);
    if (granularity == ScrollGranularity::Page && axis == ScrollEventAxis::Vertical)
        step = m_scrollableArea.adjustVerticalPageScrollStepForFixedContent(step);

    return step;
}

std::optional<KeyboardScroll> KeyboardScrollingAnimator::makeKeyboardScroll(ScrollDirection direction, ScrollGranularity granularity) const
{
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

bool KeyboardScrollingAnimator::beginKeyboardScrollGesture(ScrollDirection direction, ScrollGranularity granularity)
{
    auto scroll = makeKeyboardScroll(direction, granularity);
    if (!scroll)
        return false;

    auto scrollableDirections = scrollableDirectionsFromPosition(m_scrollableArea.scrollAnimator().currentPosition());
    if (!scrollableDirections.at(boxSideForDirection(direction))) {
        stopScrollingImmediately();
        return false;
    }

    if (m_scrollTriggeringKeyIsPressed)
        return true;

    if (granularity == ScrollGranularity::Document) {
        m_scrollableArea.endKeyboardScroll(false);
        auto newPosition = IntPoint(m_scrollableArea.scrollAnimator().currentPosition() + scroll->offset);
        m_scrollableArea.scrollAnimator().scrollToPositionWithAnimation(newPosition);
        return true;
    }

    m_scrollTriggeringKeyIsPressed = true;

    m_scrollableArea.beginKeyboardScroll(*scroll);

    return true;
}

void KeyboardScrollingAnimator::handleKeyUpEvent()
{
    if (!m_scrollTriggeringKeyIsPressed)
        return;

    m_scrollTriggeringKeyIsPressed = false;

    m_scrollableArea.endKeyboardScroll(false);
}

void KeyboardScrollingAnimator::stopScrollingImmediately()
{
    m_scrollTriggeringKeyIsPressed = false;
    m_scrollableArea.endKeyboardScroll(true);
}

} // namespace WebCore
