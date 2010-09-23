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

#ifndef PlatformTouchEvent_h
#define PlatformTouchEvent_h

#include "PlatformTouchPoint.h"
#include <wtf/Vector.h>

#if ENABLE(TOUCH_EVENTS)

#if PLATFORM(QT)
QT_BEGIN_NAMESPACE
class QTouchEvent;
QT_END_NAMESPACE
#endif

#if PLATFORM(ANDROID)
#include "IntPoint.h"
#endif

#if PLATFORM(BREWMP)
typedef unsigned short    uint16;
typedef unsigned long int uint32;
#define AEEEvent uint16
#endif

namespace WebCore {

enum TouchEventType {
    TouchStart
    , TouchMove
    , TouchEnd
    , TouchCancel
};

class PlatformTouchEvent {
public:
    PlatformTouchEvent()
        : m_type(TouchStart)
        , m_ctrlKey(false)
        , m_altKey(false)
        , m_shiftKey(false)
        , m_metaKey(false)
    {}
#if PLATFORM(QT)
    PlatformTouchEvent(QTouchEvent*);
#elif PLATFORM(ANDROID)
    PlatformTouchEvent(const IntPoint& windowPos, TouchEventType, PlatformTouchPoint::State, int metaState);
#elif PLATFORM(BREWMP)
    PlatformTouchEvent(AEEEvent, uint16 wParam, uint32 dwParam);
#endif

    TouchEventType type() const { return m_type; }
    const Vector<PlatformTouchPoint>& touchPoints() const { return m_touchPoints; }

    bool ctrlKey() const { return m_ctrlKey; }
    bool altKey() const { return m_altKey; }
    bool shiftKey() const { return m_shiftKey; }
    bool metaKey() const { return m_metaKey; }

protected:
    TouchEventType m_type;
    Vector<PlatformTouchPoint> m_touchPoints;
    bool m_ctrlKey;
    bool m_altKey;
    bool m_shiftKey;
    bool m_metaKey;
};

}

#endif // ENABLE(TOUCH_EVENTS)

#endif // PlatformTouchEvent_h
