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
#include "LocalFrameView.h"
#include "PlatformKeyboardEvent.h"
#include "ScrollAnimator.h"
#include "ScrollTypes.h"
#include <wtf/SortedArrayMap.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(KeyboardScrollingAnimator);

KeyboardScrollingAnimator::KeyboardScrollingAnimator(ScrollableArea& scrollableArea)
    : m_scrollableArea(scrollableArea)
{
}

const std::optional<KeyboardScrollingKey> keyboardScrollingKeyForKeyboardEvent(const KeyboardEvent& event)
{
    auto* platformEvent = event.underlyingPlatformEvent();
    if (!platformEvent)
        return { };

    // PlatformEvent::Type::Char is a "keypress" event.
    if (!(platformEvent->type() == PlatformEvent::Type::RawKeyDown || platformEvent->type() == PlatformEvent::Type::Char))
        return { };

    static constexpr std::pair<PackedASCIILiteral<uint64_t>, KeyboardScrollingKey> mappings[] = {
        { "Down"_s, KeyboardScrollingKey::DownArrow },
        { "End"_s, KeyboardScrollingKey::End },
        { "Home"_s, KeyboardScrollingKey::Home },
        { "Left"_s, KeyboardScrollingKey::LeftArrow },
        { "PageDown"_s, KeyboardScrollingKey::PageDown },
        { "PageUp"_s, KeyboardScrollingKey::PageUp },
        { "Right"_s, KeyboardScrollingKey::RightArrow },
        { "Up"_s, KeyboardScrollingKey::UpArrow },
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
    switch (key.value()) {
    case KeyboardScrollingKey::LeftArrow:
    case KeyboardScrollingKey::RightArrow:
        if (event.shiftKey() || event.metaKey())
            return { };
        if (event.altKey())
            return ScrollGranularity::Page;
        return ScrollGranularity::Line;
    case KeyboardScrollingKey::UpArrow:
    case KeyboardScrollingKey::DownArrow:
        if (event.shiftKey())
            return { };
        if (event.metaKey()) {
            if (event.modifierKeys().hasExactlyOneBitSet())
                return ScrollGranularity::Document;
            return { };
        }
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
    return { };
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

RectEdges<bool> KeyboardScrollingAnimator::scrollingDirections() const
{
    RectEdges<bool> edges;

    edges.setTop(m_scrollableArea.allowsVerticalScrolling());
    edges.setBottom(edges.top());
    edges.setLeft(m_scrollableArea.allowsHorizontalScrolling());
    edges.setRight(edges.left());

    return edges;
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

bool KeyboardScrollingAnimator::beginKeyboardScrollGesture(ScrollDirection direction, ScrollGranularity granularity, bool isKeyRepeat)
{
    auto scroll = makeKeyboardScroll(direction, granularity);
    if (!scroll)
        return false;

    if (m_scrollableArea.isUserScrollInProgress()) {
        m_scrollTriggeringKeyIsPressed = false;
        m_scrollableArea.endKeyboardScroll(true);
        return true;
    }

    if (m_scrollTriggeringKeyIsPressed)
        return true;

    if (!scrollingDirections().at(boxSideForDirection(direction)))
        return false;

    if (granularity == ScrollGranularity::Document || (!isKeyRepeat && granularity == ScrollGranularity::Page)) {
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
