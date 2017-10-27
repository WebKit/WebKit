/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006, 2008, 2010, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WheelEvent.h"

#include "DataTransfer.h"
#include "EventNames.h"
#include <wtf/MathExtras.h>

namespace WebCore {

inline static unsigned determineDeltaMode(const PlatformWheelEvent& event)
{
    return event.granularity() == ScrollByPageWheelEvent ? WheelEvent::DOM_DELTA_PAGE : WheelEvent::DOM_DELTA_PIXEL;
}

WheelEvent::WheelEvent()
{
}

WheelEvent::WheelEvent(const AtomicString& type, const Init& initializer, IsTrusted isTrusted)
    : MouseEvent(type, initializer, isTrusted)
    , m_wheelDelta(initializer.wheelDeltaX ? initializer.wheelDeltaX : -initializer.deltaX, initializer.wheelDeltaY ? initializer.wheelDeltaY : -initializer.deltaY)
    , m_deltaX(initializer.deltaX ? initializer.deltaX : -initializer.wheelDeltaX)
    , m_deltaY(initializer.deltaY ? initializer.deltaY : -initializer.wheelDeltaY)
    , m_deltaZ(initializer.deltaZ)
    , m_deltaMode(initializer.deltaMode)
{
}

WheelEvent::WheelEvent(const PlatformWheelEvent& event, DOMWindow* view)
    : MouseEvent(eventNames().wheelEvent, true, true, event.timestamp().approximateMonotonicTime(), view, 0, event.globalPosition(), event.position()
#if ENABLE(POINTER_LOCK)
        , { }
#endif
        , event.ctrlKey(), event.altKey(), event.shiftKey(), event.metaKey(), 0, 0, nullptr, 0, 0, nullptr, false)
    , m_wheelDelta(event.wheelTicksX() * TickMultiplier, event.wheelTicksY() * TickMultiplier)
    , m_deltaX(-event.deltaX())
    , m_deltaY(-event.deltaY())
    , m_deltaMode(determineDeltaMode(event))
    , m_wheelEvent(event)
    , m_initializedWithPlatformWheelEvent(true)
{
}

void WheelEvent::initWheelEvent(int rawDeltaX, int rawDeltaY, DOMWindow* view, int screenX, int screenY, int pageX, int pageY, bool ctrlKey, bool altKey, bool shiftKey, bool metaKey)
{
    if (isBeingDispatched())
        return;
    
    initMouseEvent(eventNames().wheelEvent, true, true, view, 0, screenX, screenY, pageX, pageY, ctrlKey, altKey, shiftKey, metaKey, 0, nullptr);

    // Normalize to 120 multiple for compatibility with IE.
    m_wheelDelta = { rawDeltaX * TickMultiplier, rawDeltaY * TickMultiplier };
    m_deltaX = -rawDeltaX;
    m_deltaY = -rawDeltaY;

    m_deltaMode = DOM_DELTA_PIXEL;

    m_initializedWithPlatformWheelEvent = false;
    m_wheelEvent = { };
}

EventInterface WheelEvent::eventInterface() const
{
    return WheelEventInterfaceType;
}

bool WheelEvent::isWheelEvent() const
{
    return true;
}

} // namespace WebCore
