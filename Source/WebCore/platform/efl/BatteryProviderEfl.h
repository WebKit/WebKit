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

#ifndef BatteryProviderEfl_h
#define BatteryProviderEfl_h

#if ENABLE(BATTERY_STATUS)

#include "BatteryClient.h"
#include "BatteryStatus.h"
#include <Eldbus.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class BatteryProviderEflClient;

class BatteryProviderEfl {
public:
    explicit BatteryProviderEfl(BatteryProviderEflClient*);
    ~BatteryProviderEfl();

    virtual void startUpdating();
    virtual void stopUpdating();

    void setBatteryStatus(const AtomicString& eventType, PassRefPtr<BatteryStatus>);
    BatteryStatus* batteryStatus() const;

private:
    static void enumerateDevices(void* data, const Eldbus_Message*, Eldbus_Pending*);
    static void deviceTypeCallback(void* data, const Eldbus_Message*, Eldbus_Pending*);

    Eldbus_Connection* connection() { return m_connection; }
    void setSignalHandler(Eldbus_Signal_Handler* signalHandler) { m_signalHandler = signalHandler; }

    BatteryProviderEflClient* m_client;
    RefPtr<BatteryStatus> m_batteryStatus;

    Eldbus_Connection* m_connection;
    Eldbus_Object* m_object;
    Eldbus_Proxy* m_proxy;
    Eldbus_Signal_Handler* m_signalHandler;
};

}

#endif // ENABLE(BATTERY_STATUS)
#endif // BatteryProviderEfl_h

