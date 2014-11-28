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
#include "BatteryProviderEfl.h"

#if ENABLE(BATTERY_STATUS)
#include "BatteryProviderEflClient.h"
#include "EventNames.h"
#include <limits>

namespace WebCore {

const char BUS[] = "org.freedesktop.UPower";

BatteryProviderEfl::BatteryProviderEfl(BatteryProviderEflClient* client)
    : m_client(client)
    , m_connection(nullptr)
    , m_object(nullptr)
    , m_proxy(nullptr)
    , m_signalHandler(nullptr)
{
}

BatteryProviderEfl::~BatteryProviderEfl()
{
}

BatteryStatus* BatteryProviderEfl::batteryStatus() const
{
    return m_batteryStatus.get();
}

void BatteryProviderEfl::stopUpdating()
{
    if (m_signalHandler) {
        eldbus_signal_handler_del(m_signalHandler);
        m_signalHandler = nullptr;
    }

    if (m_proxy) {
        eldbus_proxy_unref(m_proxy);
        m_proxy = nullptr;
    }
    if (m_object) {
        eldbus_object_unref(m_object);
        m_object = nullptr;
    }

    if (m_connection) {
        eldbus_connection_unref(m_connection);
        m_connection = nullptr;
    }
}

static void batteryProperties(void* data, const Eldbus_Message* message, Eldbus_Pending*)
{
    if (eldbus_message_error_get(message, nullptr, nullptr))
        return;

    Eldbus_Message_Iter* array;
    if (!eldbus_message_arguments_get(message, "a{sv}", &array))
        return;

    BatteryProviderEfl* client = static_cast<BatteryProviderEfl*>(data);
    BatteryStatus* clientBatteryStatus = client->batteryStatus();
    bool charging = false;
    bool chargingChanged = false;

    bool chargingTimeChanged = false;
    int64_t chargingTime;

    bool dischargingTimeChanged = false;
    int64_t dischargingTime;

    bool levelChanged = false;
    double level;

    Eldbus_Message_Iter* dict;
    while (eldbus_message_iter_get_and_next(array, 'e', &dict)) {
        char* key;
        Eldbus_Message_Iter* variant;
        if (!eldbus_message_iter_arguments_get(dict, "sv", &key, &variant))
            continue;

        if (!strcmp(key, "State")) {
            unsigned state;
            eldbus_message_iter_arguments_get(variant, "u", &state);
            if (state == 1) // Charging
                charging = true;
            if (!clientBatteryStatus || charging != clientBatteryStatus->charging())
                chargingChanged = true;

        } else if (!strcmp(key, "TimeToEmpty")) {
            eldbus_message_iter_arguments_get(variant, "x", &dischargingTime);
            if (!clientBatteryStatus || dischargingTime != clientBatteryStatus->dischargingTime())
                dischargingTimeChanged = true;

        } else if (!strcmp(key, "TimeToFull")) {
            eldbus_message_iter_arguments_get(variant, "x", &chargingTime);
            if (!clientBatteryStatus || chargingTime != clientBatteryStatus->chargingTime())
                chargingTimeChanged = true;

        } else if (!strcmp(key, "Percentage")) {
            eldbus_message_iter_arguments_get(variant, "d", &level);
            if (!clientBatteryStatus || level != clientBatteryStatus->level() * 100)
                levelChanged = true;
        }
    }

    RefPtr<BatteryStatus> batteryStatus = BatteryStatus::create(charging, chargingTime, dischargingTime, level / 100);
    if (chargingChanged)
        client->setBatteryStatus(eventNames().chargingchangeEvent, batteryStatus);
    if (chargingTimeChanged)
        client->setBatteryStatus(eventNames().chargingtimechangeEvent, batteryStatus);
    if (dischargingTimeChanged)
        client->setBatteryStatus(eventNames().dischargingtimechangeEvent, batteryStatus);
    if (levelChanged)
        client->setBatteryStatus(eventNames().levelchangeEvent, batteryStatus);
}

static void batteryPropertiesChanged(void* data, const Eldbus_Message*)
{
    Eldbus_Proxy* proxy = static_cast<Eldbus_Proxy*>(data);
    BatteryProviderEfl* provider = static_cast<BatteryProviderEfl*>(eldbus_proxy_data_get(proxy, "_provider"));
    eldbus_proxy_property_get_all(proxy, batteryProperties, provider);
}

void BatteryProviderEfl::deviceTypeCallback(void* data, const Eldbus_Message* message, Eldbus_Pending*)
{
    if (eldbus_message_error_get(message, nullptr, nullptr))
        return;

    Eldbus_Message_Iter* variant;
    if (!eldbus_message_arguments_get(message, "v", &variant))
        return;

    unsigned type = 0;
    eldbus_message_iter_arguments_get(variant, "u", &type);
    if (type != 2)
        return;

    Eldbus_Proxy* proxy = static_cast<Eldbus_Proxy*>(data);
    BatteryProviderEfl* provider = static_cast<BatteryProviderEfl*>(eldbus_proxy_data_get(proxy, "_provider"));
    eldbus_proxy_property_get_all(proxy, batteryProperties, provider);
    provider->setSignalHandler(eldbus_proxy_signal_handler_add(proxy, "Changed", batteryPropertiesChanged, proxy));
}

void BatteryProviderEfl::enumerateDevices(void* data, const Eldbus_Message* message, Eldbus_Pending*)
{
    if (eldbus_message_error_get(message, nullptr, nullptr))
        return;

    Eldbus_Message_Iter* array;
    if (!eldbus_message_arguments_get(message, "ao", &array))
        return;

    char* path;
    while (eldbus_message_iter_get_and_next(array, 'o', &path)) {
        if (!path || !path[0])
            break;

        Eldbus_Object* object = eldbus_object_get(static_cast<BatteryProviderEfl*>(data)->connection(), BUS, path);
        if (!object)
            continue;

        Eldbus_Proxy* proxy = eldbus_proxy_get(object, "org.freedesktop.UPower.Device");
        if (!proxy)
            continue;

        eldbus_proxy_data_set(proxy, "_provider", data);
        eldbus_proxy_property_get(proxy, "Type", deviceTypeCallback, proxy);
    }
}

void BatteryProviderEfl::startUpdating()
{
    if (m_connection)
        return;

    if (!eldbus_init())
        return;

    m_connection = eldbus_connection_get(ELDBUS_CONNECTION_TYPE_SYSTEM);
    EINA_SAFETY_ON_NULL_RETURN(m_connection);

    m_object = eldbus_object_get(m_connection, BUS, "/org/freedesktop/UPower");
    if (!m_object) {
        eldbus_connection_unref(m_connection);
        m_connection = nullptr;
        return;
    }

    m_proxy = eldbus_proxy_get(m_object, "org.freedesktop.UPower");
    if (!m_proxy) {
        eldbus_object_unref(m_object);
        m_object = nullptr;
        eldbus_connection_unref(m_connection);
        m_connection = nullptr;
        return;
    }

    eldbus_proxy_call(m_proxy, "EnumerateDevices", enumerateDevices, this, -1, "");
}

void BatteryProviderEfl::setBatteryStatus(const AtomicString& eventType, PassRefPtr<BatteryStatus> batteryStatus)
{
    m_batteryStatus = batteryStatus;
    m_client->didChangeBatteryStatus(eventType, m_batteryStatus);
}

}

#endif // BATTERY_STATUS

