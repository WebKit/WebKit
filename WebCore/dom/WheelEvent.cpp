/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#include "EventNames.h"
#include <wtf/MathExtras.h>

namespace WebCore {

WheelEvent::WheelEvent()
    : m_wheelDeltaX(0)
    , m_wheelDeltaY(0)
{
}

WheelEvent::WheelEvent(float wheelTicksX, float wheelTicksY, PassRefPtr<AbstractView> view,
                       int screenX, int screenY, int pageX, int pageY,
                       bool ctrlKey, bool altKey, bool shiftKey, bool metaKey)
    : MouseRelatedEvent(eventNames().mousewheelEvent,
                        true, true, view, 0, screenX, screenY, pageX, pageY, 
                        ctrlKey, altKey, shiftKey, metaKey)
    , m_wheelDeltaX(lroundf(wheelTicksX * 120))
    , m_wheelDeltaY(lroundf(wheelTicksY * 120)) // Normalize to the Windows 120 multiple
{
}

void WheelEvent::initWheelEvent(int wheelDeltaX, int wheelDeltaY, PassRefPtr<AbstractView> view,
                                int screenX, int screenY, int pageX, int pageY,
                                bool ctrlKey, bool altKey, bool shiftKey, bool metaKey)
{
    if (dispatched())
        return;
    
    initUIEvent(eventNames().mousewheelEvent, true, true, view, 0);
    
    m_screenX = screenX;
    m_screenY = screenY;
    m_ctrlKey = ctrlKey;
    m_altKey = altKey;
    m_shiftKey = shiftKey;
    m_metaKey = metaKey;
    m_wheelDeltaX = wheelDeltaX;
    m_wheelDeltaY = wheelDeltaY;
    
    initCoordinates(pageX, pageY);
}


bool WheelEvent::isWheelEvent() const
{
    return true;
}

} // namespace WebCore
