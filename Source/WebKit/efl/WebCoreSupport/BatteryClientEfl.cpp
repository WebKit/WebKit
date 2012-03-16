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

#include "config.h"
#include "BatteryClientEfl.h"

#if ENABLE(BATTERY_STATUS)

#include "BatteryController.h"

namespace WebCore {

BatteryClientEfl::BatteryClientEfl()
    : m_controller(0)
{
}

void BatteryClientEfl::setController(BatteryController* controller)
{
    m_controller = controller;
}

void BatteryClientEfl::startUpdating()
{
    // FIXME: Need to implement for getting battery status to the efl port.
}

void BatteryClientEfl::stopUpdating()
{
    // FIXME: Need to implement for getting battery status to the efl port
}

void BatteryClientEfl::batteryControllerDestroyed()
{
    delete this;
}

void BatteryClientEfl::setBatteryStatus(const AtomicString& eventType, PassRefPtr<BatteryStatus> batteryStatus)
{
    m_controller->didChangeBatteryStatus(eventType, batteryStatus);
}

}

#endif // BATTERY_STATUS

