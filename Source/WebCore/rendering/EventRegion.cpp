/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "EventRegion.h"

#include "RenderStyle.h"

namespace WebCore {

EventRegionContext::EventRegionContext(EventRegion& eventRegion)
    : m_eventRegion(eventRegion)
{
}

void EventRegionContext::pushTransform(const AffineTransform& transform)
{
    if (m_transformStack.isEmpty())
        m_transformStack.append(transform);
    else
        m_transformStack.append(m_transformStack.last() * transform);
}

void EventRegionContext::popTransform()
{
    if (m_transformStack.isEmpty()) {
        ASSERT_NOT_REACHED();
        return;
    }
    m_transformStack.removeLast();
}

void EventRegionContext::pushClip(const IntRect& clipRect)
{
    auto transformedClip = m_transformStack.isEmpty() ? clipRect : m_transformStack.last().mapRect(clipRect);

    if (m_clipStack.isEmpty())
        m_clipStack.append(transformedClip);
    else
        m_clipStack.append(intersection(m_clipStack.last(), transformedClip));
}

void EventRegionContext::popClip()
{
    if (m_clipStack.isEmpty()) {
        ASSERT_NOT_REACHED();
        return;
    }
    m_clipStack.removeLast();
}

void EventRegionContext::unite(const Region& region, const RenderStyle& style, bool overrideUserModifyIsEditable)
{
    if (m_transformStack.isEmpty() && m_clipStack.isEmpty()) {
        m_eventRegion.unite(region, style, overrideUserModifyIsEditable);
        return;
    }

    auto transformedAndClippedRegion = m_transformStack.isEmpty() ? region : m_transformStack.last().mapRegion(region);

    if (!m_clipStack.isEmpty())
        transformedAndClippedRegion.intersect(m_clipStack.last());

    m_eventRegion.unite(transformedAndClippedRegion, style, overrideUserModifyIsEditable);
}

bool EventRegionContext::contains(const IntRect& rect) const
{
    if (m_transformStack.isEmpty())
        return m_eventRegion.contains(rect);

    return m_eventRegion.contains(m_transformStack.last().mapRect(rect));
}

EventRegion::EventRegion() = default;

bool EventRegion::operator==(const EventRegion& other) const
{
#if ENABLE(TOUCH_ACTION_REGIONS)
    if (m_touchActionRegions != other.m_touchActionRegions)
        return false;
#endif

#if ENABLE(WHEEL_EVENT_REGIONS)
    if (m_wheelEventListenerRegion != other.m_wheelEventListenerRegion)
        return false;
    if (m_nonPassiveWheelEventListenerRegion != other.m_nonPassiveWheelEventListenerRegion)
        return false;
#endif

#if ENABLE(EDITABLE_REGION)
    if (m_editableRegion != other.m_editableRegion)
        return false;
#endif
    return m_region == other.m_region;
}

void EventRegion::unite(const Region& region, const RenderStyle& style, bool overrideUserModifyIsEditable)
{
    m_region.unite(region);

#if ENABLE(TOUCH_ACTION_REGIONS)
    uniteTouchActions(region, style.effectiveTouchActions());
#endif

#if ENABLE(WHEEL_EVENT_REGIONS)
    uniteEventListeners(region, style.eventListenerRegionTypes());
#endif

#if ENABLE(EDITABLE_REGION)
    if (overrideUserModifyIsEditable || style.userModify() != UserModify::ReadOnly)
        m_editableRegion.unite(region);
#else
    UNUSED_PARAM(overrideUserModifyIsEditable);
#endif

#if !ENABLE(TOUCH_ACTION_REGIONS) && !ENABLE(WHEEL_EVENT_REGIONS) && !ENABLE(EDITABLE_REGION)
    UNUSED_PARAM(style);
#endif
}

void EventRegion::translate(const IntSize& offset)
{
    m_region.translate(offset);

#if ENABLE(TOUCH_ACTION_REGIONS)
    for (auto& touchActionRegion : m_touchActionRegions)
        touchActionRegion.translate(offset);
#endif

#if ENABLE(WHEEL_EVENT_REGIONS)
    m_wheelEventListenerRegion.translate(offset);
    m_nonPassiveWheelEventListenerRegion.translate(offset);
#endif

#if ENABLE(EDITABLE_REGION)
    m_editableRegion.translate(offset);
#endif
}

static inline unsigned toIndex(TouchAction touchAction)
{
    switch (touchAction) {
    case TouchAction::None:
        return 0;
    case TouchAction::Manipulation:
        return 1;
    case TouchAction::PanX:
        return 2;
    case TouchAction::PanY:
        return 3;
    case TouchAction::PinchZoom:
        return 4;
    case TouchAction::Auto:
        break;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static inline TouchAction toTouchAction(unsigned index)
{
    switch (index) {
    case 0:
        return TouchAction::None;
    case 1:
        return TouchAction::Manipulation;
    case 2:
        return TouchAction::PanX;
    case 3:
        return TouchAction::PanY;
    case 4:
        return TouchAction::PinchZoom;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return TouchAction::Auto;
}

#if ENABLE(TOUCH_ACTION_REGIONS)
void EventRegion::uniteTouchActions(const Region& touchRegion, OptionSet<TouchAction> touchActions)
{
    for (auto touchAction : touchActions) {
        if (touchAction == TouchAction::Auto)
            break;
        auto index = toIndex(touchAction);
        if (m_touchActionRegions.size() < index + 1)
            m_touchActionRegions.grow(index + 1);
    }

    for (unsigned i = 0; i < m_touchActionRegions.size(); ++i) {
        auto regionTouchAction = toTouchAction(i);
        if (touchActions.contains(regionTouchAction))
            m_touchActionRegions[i].unite(touchRegion);
        else
            m_touchActionRegions[i].subtract(touchRegion);
    }
}

const Region* EventRegion::regionForTouchAction(TouchAction action) const
{
    unsigned actionIndex = toIndex(action);
    if (actionIndex >= m_touchActionRegions.size())
        return nullptr;

    return &m_touchActionRegions[actionIndex];
}

OptionSet<TouchAction> EventRegion::touchActionsForPoint(const IntPoint& point) const
{
    OptionSet<TouchAction> actions;

    for (unsigned i = 0; i < m_touchActionRegions.size(); ++i) {
        if (m_touchActionRegions[i].contains(point)) {
            auto action = toTouchAction(i);
            actions.add(action);
            if (action == TouchAction::None || action == TouchAction::Manipulation)
                break;
        }
    }

    if (actions.isEmpty())
        return { TouchAction::Auto };

    return actions;
}
#endif

#if ENABLE(WHEEL_EVENT_REGIONS)
void EventRegion::uniteEventListeners(const Region& region, OptionSet<EventListenerRegionType> eventListenerRegionTypes)
{
    if (eventListenerRegionTypes.contains(EventListenerRegionType::Wheel))
        m_wheelEventListenerRegion.unite(region);
    if (eventListenerRegionTypes.contains(EventListenerRegionType::NonPassiveWheel))
        m_nonPassiveWheelEventListenerRegion.unite(region);
}

OptionSet<EventListenerRegionType> EventRegion::eventListenerRegionTypesForPoint(const IntPoint& point) const
{
    OptionSet<EventListenerRegionType> regionTypes;
    if (m_wheelEventListenerRegion.contains(point))
        regionTypes.add(EventListenerRegionType::Wheel);
    if (m_nonPassiveWheelEventListenerRegion.contains(point))
        regionTypes.add(EventListenerRegionType::NonPassiveWheel);

    return regionTypes;
}

const Region& EventRegion::eventListenerRegionForType(EventListenerRegionType type) const
{
    switch (type) {
    case EventListenerRegionType::Wheel:
        return m_wheelEventListenerRegion;
    case EventListenerRegionType::NonPassiveWheel:
        return m_nonPassiveWheelEventListenerRegion;
    }
    ASSERT_NOT_REACHED();
    return m_wheelEventListenerRegion;
}
#endif // ENABLE(WHEEL_EVENT_REGIONS)

#if ENABLE(EDITABLE_REGION)
bool EventRegion::containsEditableElementsInRect(const IntRect& rect) const
{
    return m_editableRegion.intersects(rect);
}
#endif

void EventRegion::dump(TextStream& ts) const
{
    ts << m_region;

#if ENABLE(TOUCH_ACTION_REGIONS)
    if (!m_touchActionRegions.isEmpty()) {
        TextStream::IndentScope indentScope(ts);
        ts << indent << "(touch-action\n";
        for (unsigned i = 0; i < m_touchActionRegions.size(); ++i) {
            if (m_touchActionRegions[i].isEmpty())
                continue;
            TextStream::IndentScope indentScope(ts);
            ts << indent << "(" << toTouchAction(i);
            ts << indent << m_touchActionRegions[i];
            ts << indent << ")\n";
        }
        ts << indent << ")\n";
    }
#endif

#if ENABLE(WHEEL_EVENT_REGIONS)
    if (!m_wheelEventListenerRegion.isEmpty()) {
        ts << indent << "(wheel event listener region" << m_wheelEventListenerRegion;
        if (!m_nonPassiveWheelEventListenerRegion.isEmpty()) {
            TextStream::IndentScope indentScope(ts);
            ts << indent << "(non-passive" << m_nonPassiveWheelEventListenerRegion;
            ts << indent << ")\n";
        }
        ts << indent << ")\n";
    }
#endif

#if ENABLE(EDITABLE_REGION)
    if (!m_editableRegion.isEmpty()) {
        ts << indent << "(editable region" << m_editableRegion;
        ts << indent << ")\n";
    }
#endif
}

TextStream& operator<<(TextStream& ts, TouchAction touchAction)
{
    switch (touchAction) {
    case TouchAction::None:
        return ts << "none";
    case TouchAction::Manipulation:
        return ts << "manipulation";
    case TouchAction::PanX:
        return ts << "pan-x";
    case TouchAction::PanY:
        return ts << "pan-y";
    case TouchAction::PinchZoom:
        return ts << "pinch-zoom";
    case TouchAction::Auto:
        return ts << "auto";
    }
    ASSERT_NOT_REACHED();
    return ts;
}

TextStream& operator<<(TextStream& ts, const EventRegion& eventRegion)
{
    eventRegion.dump(ts);
    return ts;
}

}
