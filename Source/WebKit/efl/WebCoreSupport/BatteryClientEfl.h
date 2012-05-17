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

#ifndef BatteryClientEfl_h
#define BatteryClientEfl_h

#if ENABLE(BATTERY_STATUS)

#include "BatteryClient.h"
#include "BatteryStatus.h"
#include "Timer.h"
#include <E_Ukit.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class BatteryController;
class BatteryStatus;

class BatteryClientEfl : public BatteryClient {
public:
    BatteryClientEfl();
    ~BatteryClientEfl() { };

    virtual void setController(BatteryController*);
    virtual void startUpdating();
    virtual void stopUpdating();
    virtual void batteryControllerDestroyed();

    void setBatteryStatus(const AtomicString& eventType, PassRefPtr<BatteryStatus>);
    BatteryStatus* batteryStatus() { return m_batteryStatus.get(); }

private:
    void timerFired(Timer<BatteryClientEfl>*);
    static void getBatteryStatus(void* data, void* replyData, DBusError*);
    static void setBatteryClient(void* data, void* replyData, DBusError*);

    BatteryController* m_controller;
    Timer<BatteryClientEfl> m_timer;
    RefPtr<BatteryStatus> m_batteryStatus;
    const double m_batteryStatusRefreshInterval;
};

}

#endif // BATTERY_STATUS
#endif // BatteryClientEfl_h

