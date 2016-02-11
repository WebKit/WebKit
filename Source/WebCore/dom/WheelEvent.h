/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2010, 2013 Apple Inc. All rights reserved.
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
 *
 */

#ifndef WheelEvent_h
#define WheelEvent_h

#include "FloatPoint.h"
#include "MouseEvent.h"
#include "PlatformWheelEvent.h"

namespace WebCore {

class PlatformWheelEvent;

struct WheelEventInit : public MouseEventInit {
    WheelEventInit();

    double deltaX;
    double deltaY;
    double deltaZ;
    unsigned deltaMode;
    int wheelDeltaX; // Deprecated.
    int wheelDeltaY; // Deprecated.
};

class WheelEvent final : public MouseEvent {
public:
    enum { TickMultiplier = 120 };

    enum DeltaMode {
        DOM_DELTA_PIXEL = 0,
        DOM_DELTA_LINE,
        DOM_DELTA_PAGE
    };

    static Ref<WheelEvent> create(const PlatformWheelEvent& event, AbstractView* view)
    {
        return adoptRef(*new WheelEvent(event, view));
    }

    static Ref<WheelEvent> createForBindings()
    {
        return adoptRef(*new WheelEvent);
    }

    static Ref<WheelEvent> createForBindings(const AtomicString& type, const WheelEventInit& initializer)
    {
        return adoptRef(*new WheelEvent(type, initializer));
    }

    void initWheelEvent(int rawDeltaX, int rawDeltaY, AbstractView*,
        int screenX, int screenY, int pageX, int pageY,
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey);

    void initWebKitWheelEvent(int rawDeltaX, int rawDeltaY, AbstractView*,
        int screenX, int screenY, int pageX, int pageY,
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey);

    const PlatformWheelEvent* wheelEvent() const { return m_initializedWithPlatformWheelEvent ? &m_wheelEvent : nullptr; }
    double deltaX() const { return m_deltaX; } // Positive when scrolling right.
    double deltaY() const { return m_deltaY; } // Positive when scrolling down.
    double deltaZ() const { return m_deltaZ; }
    int wheelDelta() const { return wheelDeltaY() ? wheelDeltaY() : wheelDeltaX(); } // Deprecated.
    int wheelDeltaX() const { return m_wheelDelta.x(); } // Deprecated, negative when scrolling right.
    int wheelDeltaY() const { return m_wheelDelta.y(); } // Deprecated, negative when scrolling down.
    unsigned deltaMode() const { return m_deltaMode; }

    bool webkitDirectionInvertedFromDevice() const { return m_wheelEvent.directionInvertedFromDevice(); }
    // Needed for Objective-C legacy support
    bool isHorizontal() const { return m_wheelDelta.x(); }

    virtual EventInterface eventInterface() const override;

#if PLATFORM(MAC)
    PlatformWheelEventPhase phase() const { return m_wheelEvent.phase(); }
    PlatformWheelEventPhase momentumPhase() const { return m_wheelEvent.momentumPhase(); }
#endif

private:
    WheelEvent();
    WheelEvent(const AtomicString&, const WheelEventInit&);
    WheelEvent(const PlatformWheelEvent&, AbstractView*);

    virtual bool isWheelEvent() const override;

    IntPoint m_wheelDelta;
    double m_deltaX;
    double m_deltaY;
    double m_deltaZ;
    unsigned m_deltaMode;
    PlatformWheelEvent m_wheelEvent;
    bool m_initializedWithPlatformWheelEvent;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_EVENT(WheelEvent)

#endif // WheelEvent_h
