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
#include "Node.h"
#include "PlatformMouseEvent.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/MathExtras.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WheelEvent);

inline static unsigned determineDeltaMode(const PlatformWheelEvent& event)
{
    return event.granularity() == ScrollByPageWheelEvent ? WheelEvent::DOM_DELTA_PAGE : WheelEvent::DOM_DELTA_PIXEL;
}

static double wheelDeltaToDelta(int wheelDelta)
{
    // Avoid returning negative zero.
    if (!wheelDelta)
        return 0;
    return -static_cast<double>(wheelDelta);
}

inline WheelEvent::WheelEvent() = default;

inline WheelEvent::WheelEvent(const AtomString& type, const Init& initializer)
    : MouseEvent(type, initializer)
    , m_wheelDelta(initializer.wheelDeltaX ? initializer.wheelDeltaX : clampTo<int>(-initializer.deltaX), initializer.wheelDeltaY ? initializer.wheelDeltaY : clampTo<int>(-initializer.deltaY))
    , m_deltaX(initializer.deltaX ? initializer.deltaX : wheelDeltaToDelta(initializer.wheelDeltaX))
    , m_deltaY(initializer.deltaY ? initializer.deltaY : wheelDeltaToDelta(initializer.wheelDeltaY))
    , m_deltaZ(initializer.deltaZ)
    , m_deltaMode(initializer.deltaMode)
{
}

inline WheelEvent::WheelEvent(const PlatformWheelEvent& event, RefPtr<WindowProxy>&& view, IsCancelable isCancelable)
    : MouseEvent(eventNames().wheelEvent, CanBubble::Yes, isCancelable, IsComposed::Yes, event.timestamp().approximateMonotonicTime(), WTFMove(view), 0,
        event.globalPosition(), event.position() , 0, 0, event.modifiers(), MouseButton::Left, 0, nullptr, 0, SyntheticClickType::NoTap, IsSimulated::No, IsTrusted::Yes)
    , m_wheelDelta(event.wheelTicksX() * TickMultiplier, event.wheelTicksY() * TickMultiplier)
    , m_deltaX(-event.deltaX())
    , m_deltaY(-event.deltaY())
    , m_deltaMode(determineDeltaMode(event))
    , m_underlyingPlatformEvent(event)
{
}

Ref<WheelEvent> WheelEvent::create(const PlatformWheelEvent& event, RefPtr<WindowProxy>&& view, IsCancelable isCancelable)
{
    return adoptRef(*new WheelEvent(event, WTFMove(view), isCancelable));
}

Ref<WheelEvent> WheelEvent::createForBindings()
{
    return adoptRef(*new WheelEvent);
}

Ref<WheelEvent> WheelEvent::create(const AtomString& type, const Init& initializer)
{
    return adoptRef(*new WheelEvent(type, initializer));
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
