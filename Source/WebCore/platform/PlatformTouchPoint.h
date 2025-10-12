/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef PlatformTouchPoint_h
#define PlatformTouchPoint_h

#include "DoublePoint.h"

#if ENABLE(TOUCH_EVENTS)

namespace WebCore {

class PlatformTouchEvent;

class PlatformTouchPoint {
public:
    enum State {
        TouchReleased,
        TouchPressed,
        TouchMoved,
        TouchStationary,
        TouchCancelled,
        TouchStateEnd // Placeholder: must remain the last item.
    };

    // This is necessary for us to be able to build synthetic events.
    PlatformTouchPoint()
        : m_id(0)
        , m_rotationAngle(0)
        , m_force(0)
    {
    }

#if PLATFORM(WPE)
    // FIXME: since WPE currently does not send touch stationary events, we need to be able to
    // create a PlatformTouchPoint of type TouchCancelled artificially
    PlatformTouchPoint(unsigned id, State state, DoublePoint screenPos, DoublePoint pos)
        : m_id(id)
        , m_state(state)
        , m_screenPos(screenPos)
        , m_pos(pos)
    {
    }
#endif

    unsigned id() const { return m_id; }
    State state() const { return m_state; }
    DoublePoint screenPos() const { return m_screenPos; }
    DoublePoint pos() const { return m_pos; }
    DoubleSize radius() const { return m_radius; }
    float rotationAngle() const { return m_rotationAngle; }
    float twist() const { return m_twist; }
    float force() const { return m_force; }

protected:
    unsigned m_id;
    State m_state;
    DoublePoint m_screenPos;
    DoublePoint m_pos;
    DoubleSize m_radius;
    float m_rotationAngle;
    float m_twist;
    float m_force;
};

}

#endif // ENABLE(TOUCH_EVENTS)

#endif // PlatformTouchPoint_h
