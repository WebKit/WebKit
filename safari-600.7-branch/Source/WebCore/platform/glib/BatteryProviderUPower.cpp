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

#include "config.h"
#include "BatteryProviderUPower.h"

#if ENABLE(BATTERY_STATUS)

#include "BatteryProviderUPowerClient.h"
#include <cmath>
#include <limits>
#include <wtf/gobject/GUniquePtr.h>

using namespace WebCore;

static void powerDeviceAlterationCallback(UpClient* upowerClient, UpDevice* upowerDevice, BatteryProviderUPower* provider)
{
    UpDeviceKind deviceKind;
    g_object_get(upowerDevice, "kind", &deviceKind, nullptr);
    if (deviceKind != UP_DEVICE_KIND_BATTERY)
        return;

    provider->updateBatteryStatus();
}

BatteryProviderUPower::BatteryProviderUPower(BatteryProviderUPowerClient* client)
    : m_client(client)
    , m_isUpdating(false)
{
    ASSERT(m_client);
}

void BatteryProviderUPower::startUpdating()
{
    ASSERT(!m_upowerClient);
    m_upowerClient = adoptGRef(up_client_new());

    GUniqueOutPtr<GError> error;
    if (!up_client_enumerate_devices_sync(m_upowerClient.get(), 0, &error.outPtr())) {
        m_client->updateBatteryStatus(NotAvailable);
        return;
    }

    g_signal_connect(m_upowerClient.get(), "device-changed", G_CALLBACK(powerDeviceAlterationCallback), this);
    g_signal_connect(m_upowerClient.get(), "device-added", G_CALLBACK(powerDeviceAlterationCallback), this);
    g_signal_connect(m_upowerClient.get(), "device-removed", G_CALLBACK(powerDeviceAlterationCallback), this);

    m_isUpdating = true;
    updateBatteryStatus();
}

void BatteryProviderUPower::stopUpdating()
{
    m_upowerClient.clear();
    m_isUpdating = false;
}

void BatteryProviderUPower::updateBatteryStatus()
{
    if (!m_isUpdating)
        return;

    GPtrArray* devices = up_client_get_devices(m_upowerClient.get());
    if (!devices) {
        m_client->updateBatteryStatus(NotAvailable);
        return;
    }

    unsigned numOfBatteryDevices = 0;
    double combinedEnergyCapacityCurrent = 0, combinedEnergyCapacityFull = 0, combinedEnergyRate = 0;

    for (unsigned i = 0; i < devices->len; i++) {
        UpDevice* device = static_cast<UpDevice*>(g_ptr_array_index(devices, i));
        UpDeviceKind deviceKind;
        UpDeviceState deviceState;
        bool isPresent;
        double energyCapacityCurrent = 0, energyCapacityEmpty = 0, energyCapacityFull = 0, energyRate = 0;

        g_object_get(device,
            "energy", &energyCapacityCurrent,
            "energy-empty", &energyCapacityEmpty,
            "energy-full", &energyCapacityFull,
            "energy-rate", &energyRate,
            "is-present", &isPresent,
            "kind", &deviceKind,
            "state", &deviceState,
            nullptr);

        if (deviceKind != UP_DEVICE_KIND_BATTERY || !isPresent)
            continue;

        numOfBatteryDevices++;
        combinedEnergyCapacityCurrent += energyCapacityCurrent - energyCapacityEmpty;
        combinedEnergyCapacityFull += energyCapacityFull;
        // Added energy rate should be signed according to the charging/discharging state.
        combinedEnergyRate += deviceState == UP_DEVICE_STATE_DISCHARGING ? -energyRate : energyRate;
    }

    g_ptr_array_unref(devices);

    if (!numOfBatteryDevices) {
        m_client->updateBatteryStatus(NotAvailable);
        return;
    }

    double level = 0;
    if (combinedEnergyCapacityFull > 0)
        level = combinedEnergyCapacityCurrent / combinedEnergyCapacityFull;

    if (combinedEnergyRate >= 0) {
        double chargingTime = std::numeric_limits<double>::infinity();
        if (combinedEnergyRate)
            chargingTime = 3600 * (combinedEnergyCapacityFull - combinedEnergyCapacityCurrent) / combinedEnergyRate;
        m_client->updateBatteryStatus(Charging, chargingTime, level);
    } else {
        double dischargingTime = 3600 * combinedEnergyCapacityCurrent / std::abs(combinedEnergyRate);
        m_client->updateBatteryStatus(Discharging, dischargingTime, level);
    }
}

#endif // ENABLE(BATTERY_STATUS)
