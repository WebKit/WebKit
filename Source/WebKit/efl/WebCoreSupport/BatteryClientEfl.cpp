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
#include "EventNames.h"
#include <limits>

namespace WebCore {

BatteryClientEfl::BatteryClientEfl()
    : m_controller(0)
    , m_timer(this, &BatteryClientEfl::timerFired)
    , m_batteryStatusRefreshInterval(1.0)
{
}

void BatteryClientEfl::setController(BatteryController* controller)
{
    m_controller = controller;
}

void BatteryClientEfl::startUpdating()
{
    if (m_timer.isActive())
        return;

    if (!e_dbus_init())
        return;

    if (!e_ukit_init()) {
        e_dbus_shutdown();
        return;
    }

    m_timer.startRepeating(m_batteryStatusRefreshInterval);
}

void BatteryClientEfl::stopUpdating()
{
    m_timer.stop();
    e_ukit_shutdown();
    e_dbus_shutdown();
}

void BatteryClientEfl::batteryControllerDestroyed()
{
    delete this;
}

void BatteryClientEfl::setBatteryStatus(const AtomicString& eventType, PassRefPtr<BatteryStatus> batteryStatus)
{
    m_batteryStatus = batteryStatus;
    m_controller->didChangeBatteryStatus(eventType, m_batteryStatus);
}

void BatteryClientEfl::timerFired(Timer<BatteryClientEfl>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_timer);
    E_DBus_Connection* edbusConnection = e_dbus_bus_get(DBUS_BUS_SYSTEM);
    if (edbusConnection)
        e_upower_get_all_devices(edbusConnection, getBatteryStatus, static_cast<void*>(this));
}

void BatteryClientEfl::getBatteryStatus(void* data, void* replyData, DBusError* dBusError)
{
    E_Ukit_Get_All_Devices_Return* eukitDeviceNames = static_cast<E_Ukit_Get_All_Devices_Return*>(replyData);
    if (!eukitDeviceNames || !eukitDeviceNames->strings || dbus_error_is_set(dBusError)) {
         dbus_error_free(dBusError);
         return;
    }

    E_DBus_Connection* edbusConnection = e_dbus_bus_get(DBUS_BUS_SYSTEM);
    Eina_List* list;
    void* deviceName;
    EINA_LIST_FOREACH(eukitDeviceNames->strings, list, deviceName)
        e_upower_get_all_properties(edbusConnection, static_cast<char*>(deviceName), setBatteryClient, data);
}

void BatteryClientEfl::setBatteryClient(void* data, void* replyData, DBusError* dBusError)
{
    E_Ukit_Get_All_Properties_Return* eukitPropertyNames = static_cast<E_Ukit_Get_All_Properties_Return*>(replyData);

    if (!eukitPropertyNames || dbus_error_is_set(dBusError)) {
        dbus_error_free(dBusError);
        return;
    }

    if (!eukitPropertyNames->properties)
        return;

    E_Ukit_Property* property = static_cast<E_Ukit_Property*>(eina_hash_find(eukitPropertyNames->properties, "Type"));
    if (!property || property->val.u != E_UPOWER_SOURCE_BATTERY)
        return;

    BatteryClientEfl* client = static_cast<BatteryClientEfl*>(data);
    BatteryStatus* clientBatteryStatus = client->batteryStatus();
    bool charging = false;
    bool chargingChanged = false;
    static unsigned chargingState = 0;

    property = static_cast<E_Ukit_Property*>(eina_hash_find(eukitPropertyNames->properties, "State"));
    if (!property)
        return;
    if (!clientBatteryStatus || chargingState != property->val.u) {
        chargingChanged = true;
        chargingState = property->val.u;
        (chargingState == E_UPOWER_STATE_FULL || chargingState == E_UPOWER_STATE_CHARGING) ? charging = true : charging = false;
    } else
        charging = clientBatteryStatus->charging();

    bool chargingTimeChanged = false;
    bool dischargingTimeChanged = false;
    double chargingTime = std::numeric_limits<double>::infinity();
    double dischargingTime = std::numeric_limits<double>::infinity();

    if (charging) {
        if (!clientBatteryStatus || clientBatteryStatus->dischargingTime() != std::numeric_limits<double>::infinity())
            dischargingTimeChanged = true;
        dischargingTime = std::numeric_limits<double>::infinity();
        property = static_cast<E_Ukit_Property*>(eina_hash_find(eukitPropertyNames->properties, "TimeToFull"));
        if (!property)
            return;
        if (!clientBatteryStatus || clientBatteryStatus->chargingTime() != property->val.x)
            chargingTimeChanged = true;
        chargingTime = property->val.x;
    } else {
        if (!clientBatteryStatus || clientBatteryStatus->chargingTime() != std::numeric_limits<double>::infinity())
            chargingTimeChanged = true;
        chargingTime = std::numeric_limits<double>::infinity();
        property = static_cast<E_Ukit_Property*>(eina_hash_find(eukitPropertyNames->properties, "TimeToEmpty"));
        if (!property)
            return;
        if (!clientBatteryStatus || clientBatteryStatus->dischargingTime() != property->val.x)
            dischargingTimeChanged = true;
        dischargingTime = property->val.x;
    }

    double level = 0;
    bool levelChanged = false;

    property = static_cast<E_Ukit_Property*>(eina_hash_find(eukitPropertyNames->properties, "Percentage"));
    if (!property)
        return;
    if (!clientBatteryStatus || clientBatteryStatus->level() != property->val.d)
        levelChanged = true;
    level = property->val.d;

    WTF::RefPtr<BatteryStatus> batteryStatus = BatteryStatus::create(charging, chargingTime, dischargingTime, level);
    if (chargingChanged)
        client->setBatteryStatus(eventNames().chargingchangeEvent, batteryStatus);
    if (chargingTimeChanged)
        client->setBatteryStatus(eventNames().chargingtimechangeEvent, batteryStatus);
    if (dischargingTimeChanged)
        client->setBatteryStatus(eventNames().dischargingtimechangeEvent, batteryStatus);
    if (levelChanged)
        client->setBatteryStatus(eventNames().levelchangeEvent, batteryStatus);
}

}

#endif // BATTERY_STATUS

