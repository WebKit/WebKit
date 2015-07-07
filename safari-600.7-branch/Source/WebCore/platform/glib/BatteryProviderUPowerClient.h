/*
 * Copyright (C) 2013 Igalia S.L.
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

#ifndef BatteryProviderUPowerClient_h
#define BatteryProviderUPowerClient_h

#if ENABLE(BATTERY_STATUS)

namespace WebCore {

enum BatteryProviderUPowerStatus {
    NotAvailable = 0,
    Charging,
    Discharging,
};

class BatteryProviderUPowerClient {
public:
    virtual void updateBatteryStatus(BatteryProviderUPowerStatus, double secondsRemaining = 0, double batteryLevel = 0) = 0;
};

}

#endif // ENABLE(BATTERY_STATUS)

#endif // BatteryProviderUPowerClient_h
