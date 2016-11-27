/*
 *  Copyright (C) 2012 Samsung Electronics
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#pragma once

#if ENABLE(PROXIMITY_EVENTS)

#include "Event.h"

namespace WebCore {

class DeviceProximityEvent : public Event {
public:
    ~DeviceProximityEvent() { }

    static Ref<DeviceProximityEvent> create()
    {
        return adoptRef(*new DeviceProximityEvent());
    }

    static Ref<DeviceProximityEvent> create(const AtomicString& eventType, const double value, const double min, const double max)
    {
        return adoptRef(*new DeviceProximityEvent(eventType, value, min, max));
    }

    struct Init : EventInit {
        std::optional<double> value;
        std::optional<double> min;
        std::optional<double> max;
    };

    static Ref<DeviceProximityEvent> create(const AtomicString& type, const Init& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new DeviceProximityEvent(type, initializer, isTrusted));
    }

    double value() { return m_value; }
    double min() { return m_min; }
    double max() { return m_max; }

    virtual EventInterface eventInterface() const { return DeviceProximityEventInterfaceType; }

private:
    DeviceProximityEvent();
    DeviceProximityEvent(const AtomicString& eventType, const double value, const double min, const double max);
    DeviceProximityEvent(const AtomicString& eventType, const Init&, IsTrusted);

    double m_value;
    double m_min;
    double m_max;
};

} // namespace WebCore

#endif // DeviceProximityEvent_h
