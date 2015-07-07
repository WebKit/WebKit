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
#include "WebKitBatteryProvider.h"

#if ENABLE(BATTERY_STATUS)

#include "WebBatteryManagerProxy.h"
#include "WebBatteryStatus.h"
#include <limits>

using namespace WebKit;

static inline WebKitBatteryProvider* toBatteryProvider(const void* clientInfo)
{
    return static_cast<WebKitBatteryProvider*>(const_cast<void*>(clientInfo));
}

static void startUpdatingCallback(WKBatteryManagerRef batteryManager, const void* clientInfo)
{
    toBatteryProvider(clientInfo)->startUpdating();
}

static void stopUpdatingCallback(WKBatteryManagerRef batteryManager, const void* clientInfo)
{
    toBatteryProvider(clientInfo)->stopUpdating();
}

PassRefPtr<WebKitBatteryProvider> WebKitBatteryProvider::create(WebBatteryManagerProxy* batteryManager)
{
    return adoptRef(new WebKitBatteryProvider(batteryManager));
}

WebKitBatteryProvider::WebKitBatteryProvider(WebBatteryManagerProxy* batteryManager)
    : m_batteryManager(batteryManager)
    , m_provider(this)
{
    ASSERT(batteryManager);

    WKBatteryProviderV0 wkBatteryProvider = {
        {
            0, // version
            this // clientInfo
        },
        startUpdatingCallback,
        stopUpdatingCallback
    };
    WKBatteryManagerSetProvider(toAPI(batteryManager), &wkBatteryProvider.base);
}

WebKitBatteryProvider::~WebKitBatteryProvider()
{
    m_provider.stopUpdating();
}

void WebKitBatteryProvider::startUpdating()
{
    m_provider.startUpdating();
}

void WebKitBatteryProvider::stopUpdating()
{
    m_provider.stopUpdating();
}

void WebKitBatteryProvider::updateBatteryStatus(WebCore::BatteryProviderUPowerStatus status, double secondsRemaining, double batteryLevel)
{
    RefPtr<WebBatteryStatus> batteryStatus;

    switch (status) {
    case WebCore::NotAvailable:
        // When an implementation cannot report battery status, the default values should be used.
        batteryStatus = WebBatteryStatus::create(true, std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity(), 1.0);
        break;
    case WebCore::Charging:
        batteryStatus = WebBatteryStatus::create(true, secondsRemaining, 0, batteryLevel);
        break;
    case WebCore::Discharging:
        batteryStatus = WebBatteryStatus::create(false, 0, secondsRemaining, batteryLevel);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    m_batteryManager->providerUpdateBatteryStatus(batteryStatus.get());
}

#endif // ENABLE(BATTERY_STATUS)
