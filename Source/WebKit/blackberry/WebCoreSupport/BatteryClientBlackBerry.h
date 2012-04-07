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

#ifndef BatteryClientBlackBerry_h
#define BatteryClientBlackBerry_h

#if ENABLE(BATTERY_STATUS)

#include "BatteryClient.h"

#include <BlackBerryPlatformBatteryStatusTracker.h>
#include <BlackBerryPlatformBatteryStatusTrackerListener.h>

namespace WebCore {

class BatteryController;
class BatteryStatus;

class BatteryClientBlackBerry : public BatteryClient, public BlackBerry::Platform::BatteryStatusTrackerListener {
public:
    BatteryClientBlackBerry();
    ~BatteryClientBlackBerry() { }

    virtual void setController(BatteryController*);
    virtual void startUpdating();
    virtual void stopUpdating();
    virtual void batteryControllerDestroyed();

    void onLevelChange(bool charging, double chargingTime, double dischargingTime, double level);
    void onChargingChange(bool charging, double chargingTime, double dischargingTime, double level);
    void onChargingTimeChange(bool charging, double chargingTime, double dischargingTime, double level);
    void onDischargingTimeChange(bool charging, double chargingTime, double dischargingTime, double level);

private:
    BlackBerry::Platform::BatteryStatusTracker* m_tracker;
    BatteryController* m_controller;
};

}

#endif // BATTERY_STATUS
#endif // BatteryClientBlackBerry_h
