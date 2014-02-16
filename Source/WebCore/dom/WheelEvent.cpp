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

#include "Clipboard.h"
#include "EventNames.h"
#include "PlatformWheelEvent.h"

#include <wtf/MathExtras.h>

namespace WebCore {

WheelEventInit::WheelEventInit()
    : deltaX(0)
    , deltaY(0)
    , deltaZ(0)
    , deltaMode(WheelEvent::DOM_DELTA_PIXEL)
    , wheelDeltaX(0)
    , wheelDeltaY(0)
{
}

WheelEvent::WheelEvent()
    : m_deltaX(0)
    , m_deltaY(0)
    , m_deltaZ(0)
    , m_deltaMode(DOM_DELTA_PIXEL)
    , m_directionInvertedFromDevice(false)
{
}

WheelEvent::WheelEvent(const AtomicString& type, const WheelEventInit& initializer)
    : MouseEvent(type, initializer)
    , m_wheelDelta(initializer.wheelDeltaX ? initializer.wheelDeltaX : -initializer.deltaX, initializer.wheelDeltaY ? initializer.wheelDeltaY : -initializer.deltaY)
    , m_deltaX(initializer.deltaX ? initializer.deltaX : -initializer.wheelDeltaX)
    , m_deltaY(initializer.deltaY ? initializer.deltaY : -initializer.wheelDeltaY)
    , m_deltaZ(initializer.deltaZ)
    , m_deltaMode(initializer.deltaMode)
{
}

WheelEvent::WheelEvent(const FloatPoint& wheelTicks, const FloatPoint& rawDelta, unsigned deltaMode,
    PassRefPtr<AbstractView> view, const IntPoint& screenLocation, const IntPoint& pageLocation,
    bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, bool directionInvertedFromDevice, double timestamp)
    : MouseEvent(eventNames().wheelEvent,
                 true, true, timestamp, view, 0, screenLocation.x(), screenLocation.y(),
                 pageLocation.x(), pageLocation.y(),
#if ENABLE(POINTER_LOCK)
                 0, 0,
#endif
                 ctrlKey, altKey, shiftKey, metaKey, 0, 0, 0, false)
    , m_wheelDelta(wheelTicks.x() * TickMultiplier, wheelTicks.y() * TickMultiplier)
    , m_deltaX(-rawDelta.x())
    , m_deltaY(-rawDelta.y())
    , m_deltaZ(0)
    , m_deltaMode(deltaMode)
    , m_directionInvertedFromDevice(directionInvertedFromDevice)
{
}

void WheelEvent::initWheelEvent(int rawDeltaX, int rawDeltaY, PassRefPtr<AbstractView> view,
                                int screenX, int screenY, int pageX, int pageY,
                                bool ctrlKey, bool altKey, bool shiftKey, bool metaKey)
{
    if (dispatched())
        return;
    
    initUIEvent(eventNames().wheelEvent, true, true, view, 0);
    
    m_screenLocation = IntPoint(screenX, screenY);
    m_ctrlKey = ctrlKey;
    m_altKey = altKey;
    m_shiftKey = shiftKey;
    m_metaKey = metaKey;

    // Normalize to 120 multiple for compatibility with IE.
    m_wheelDelta = IntPoint(rawDeltaX * TickMultiplier, rawDeltaY * TickMultiplier);
    m_deltaX = -rawDeltaX;
    m_deltaY = -rawDeltaY;

    m_deltaMode = DOM_DELTA_PIXEL;
    m_directionInvertedFromDevice = false;

    initCoordinates(IntPoint(pageX, pageY));
}

void WheelEvent::initWebKitWheelEvent(int rawDeltaX, int rawDeltaY, PassRefPtr<AbstractView> view,
                                      int screenX, int screenY, int pageX, int pageY,
                                      bool ctrlKey, bool altKey, bool shiftKey, bool metaKey)
{
    initWheelEvent(rawDeltaX, rawDeltaY, view, screenX, screenY, pageX, pageY,
                   ctrlKey, altKey, shiftKey, metaKey);
}

EventInterface WheelEvent::eventInterface() const
{
    return WheelEventInterfaceType;
}

bool WheelEvent::isMouseEvent() const
{
    return false;
}

bool WheelEvent::isWheelEvent() const
{
    return true;
}

} // namespace WebCore
