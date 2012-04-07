/*
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "BatteryClientBlackBerry.h"

#if ENABLE(BATTERY_STATUS)

#include "BatteryController.h"
#include <stdio.h>

namespace WebCore {

BatteryClientBlackBerry::BatteryClientBlackBerry()
    : m_tracker(0)
    , m_controller(0)
{
}

void BatteryClientBlackBerry::setController(BatteryController* controller)
{
    m_controller = controller;
}

void BatteryClientBlackBerry::startUpdating()
{
    if (m_tracker)
        m_tracker->resume();
    else
        m_tracker = BlackBerry::Platform::BatteryStatusTracker::create(this);
}

void BatteryClientBlackBerry::stopUpdating()
{
    if (m_tracker)
        m_tracker->suspend();
}

void BatteryClientBlackBerry::batteryControllerDestroyed()
{
    delete this;
}

void BatteryClientBlackBerry::onLevelChange(bool charging, double chargingTime, double dischargingTime, double level)
{
    if (!m_controller)
        return;

    m_controller->didChangeBatteryStatus("levelchange", BatteryStatus::create(charging, chargingTime, dischargingTime, level));
}

void BatteryClientBlackBerry::onChargingChange(bool charging, double chargingTime, double dischargingTime, double level)
{
    if (!m_controller)
        return;

    m_controller->didChangeBatteryStatus("chargingchange", BatteryStatus::create(charging, chargingTime, dischargingTime, level));
}

void BatteryClientBlackBerry::onChargingTimeChange(bool charging, double chargingTime, double dischargingTime, double level)
{
    if (!m_controller)
        return;

    m_controller->didChangeBatteryStatus("chargingtimechange", BatteryStatus::create(charging, chargingTime, dischargingTime, level));
}

void BatteryClientBlackBerry::onDischargingTimeChange(bool charging, double chargingTime, double dischargingTime, double level)
{
    if (!m_controller)
        return;

    m_controller->didChangeBatteryStatus("dischargingtimechange", BatteryStatus::create(charging, chargingTime, dischargingTime, level));
}

}

#endif // BATTERY_STATUS
