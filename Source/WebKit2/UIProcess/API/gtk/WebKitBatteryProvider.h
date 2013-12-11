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

#ifndef WebKitBatteryProvider_h
#define WebKitBatteryProvider_h

#if ENABLE(BATTERY_STATUS)

#include "WebKitPrivate.h"
#include <WebCore/BatteryProviderUPowerClient.h>
#include <WebCore/BatteryProviderUPower.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebKit {

class WebKitBatteryProvider : public RefCounted<WebKitBatteryProvider>, public WebCore::BatteryProviderUPowerClient {
public:
    static PassRefPtr<WebKitBatteryProvider> create(WebBatteryManagerProxy*);
    virtual ~WebKitBatteryProvider();

    void startUpdating();
    void stopUpdating();

private:
    WebKitBatteryProvider(WebBatteryManagerProxy*);

    // WebCore::BatteryProviderUPowerClient
    virtual void updateBatteryStatus(WebCore::BatteryProviderUPowerStatus, double secondsRemaining, double batteryLevel);

    RefPtr<WebBatteryManagerProxy> m_batteryManager;
    WebCore::BatteryProviderUPower m_provider;
};

}

#endif // ENABLE(BATTERY_STATUS)

#endif // WebKitBatteryProvider_h
