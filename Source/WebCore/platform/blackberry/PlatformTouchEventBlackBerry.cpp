/*
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2010, 2011 Research In Motion Limited. All rights reserved.
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
 *
 */

#include "config.h"
#include "PlatformTouchEvent.h"

#include <BlackBerryPlatformTouchEvent.h>

#if ENABLE(TOUCH_EVENTS)

namespace WebCore {

PlatformTouchEvent::PlatformTouchEvent(BlackBerry::Platform::TouchEvent* event)
    : m_ctrlKey(0)
    , m_altKey(event->m_altKey)
    , m_shiftKey(event->m_shiftKey)
    , m_metaKey(0)
    , m_rotation(0)
    , m_scale(1)
    , m_doubleTap(false)
    , m_touchHold(false)
{
    switch (event->m_type) {
    case BlackBerry::Platform::TouchEvent::TouchStart:
        m_type = TouchStart;
        break;
    case BlackBerry::Platform::TouchEvent::TouchMove:
        m_type = TouchMove;
        break;
    case BlackBerry::Platform::TouchEvent::TouchEnd:
        m_type = TouchEnd;
        break;
    case BlackBerry::Platform::TouchEvent::TouchCancel:
        m_type = TouchCancel;
        break;
    }

    for (unsigned i = 0; i < event->m_points.size(); ++i)
        m_touchPoints.append(PlatformTouchPoint(event->m_points[i]));

    if (event->m_gestures.empty())
        return;

    BlackBerry::Platform::Gesture pinch;
    if (event->hasGesture(BlackBerry::Platform::Gesture::Pinch, &pinch)) {
        BlackBerry::Platform::PinchGestureData* data = static_cast<BlackBerry::Platform::PinchGestureData*>(pinch.m_data);
        if (data) {
            m_rotation = data->m_angle * 180 / M_PI;
            m_scale = data->m_scale;
        }
    } else if (event->hasGesture(BlackBerry::Platform::Gesture::DoubleTap))
        m_doubleTap = true;
    else if (event->hasGesture(BlackBerry::Platform::Gesture::TouchHold))
        m_touchHold = true;
}

} // namespace WebCore

#endif // ENABLE(TOUCH_EVENTS)
