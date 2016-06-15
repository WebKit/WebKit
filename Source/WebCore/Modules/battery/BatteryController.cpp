/*
 *  Copyright (C) 2012 Samsung Electronics
 *  Copyright (C) 2012 Google Inc.
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

#if ENABLE(BATTERY_STATUS)
#include "BatteryController.h"

#include "BatteryClient.h"
#include "BatteryStatus.h"
#include "Event.h"
#include "EventNames.h"

namespace WebCore {

BatteryController::BatteryController(BatteryClient& client)
    : m_client(client)
{
}

BatteryController::~BatteryController()
{
    for (auto& listener : m_listeners)
        listener->batteryControllerDestroyed();
    m_client.batteryControllerDestroyed();
}

void BatteryController::addListener(BatteryManager* batteryManager)
{
    m_listeners.append(batteryManager);
    m_client.startUpdating();

    if (m_batteryStatus)
        batteryManager->updateBatteryStatus(WTFMove(m_batteryStatus));
}

void BatteryController::removeListener(BatteryManager* batteryManager)
{
    size_t pos = m_listeners.find(batteryManager);
    if (pos == WTF::notFound)
        return;
    m_listeners.remove(pos);
    if (m_listeners.isEmpty())
        m_client.stopUpdating();
}

void BatteryController::updateBatteryStatus(RefPtr<BatteryStatus>&& batteryStatus)
{
    if (m_batteryStatus) {
        if (m_batteryStatus->charging() != batteryStatus->charging())
            didChangeBatteryStatus(WebCore::eventNames().chargingchangeEvent, batteryStatus.get());
        else if (batteryStatus->charging() && m_batteryStatus->chargingTime() != batteryStatus->chargingTime())
            didChangeBatteryStatus(WebCore::eventNames().chargingtimechangeEvent, batteryStatus.get());
        else if (!batteryStatus->charging() && m_batteryStatus->dischargingTime() != batteryStatus->dischargingTime())
            didChangeBatteryStatus(WebCore::eventNames().dischargingtimechangeEvent, batteryStatus.get());

        if (m_batteryStatus->level() != batteryStatus->level())
            didChangeBatteryStatus(WebCore::eventNames().levelchangeEvent, batteryStatus.get());
    } else {
        for (auto& listener : m_listeners)
            listener->updateBatteryStatus(batteryStatus.copyRef());
    }

    m_batteryStatus = WTFMove(batteryStatus);
}

void BatteryController::didChangeBatteryStatus(const AtomicString& eventType, RefPtr<BatteryStatus>&& batteryStatus)
{
    Ref<Event> event = Event::create(eventType, false, false);
    for (auto& listener : m_listeners)
        listener->didChangeBatteryStatus(event, batteryStatus.copyRef());
}

const char* BatteryController::supplementName()
{
    return "BatteryController";
}

void provideBatteryTo(Page& page, BatteryClient& client)
{
    Supplement<Page>::provideTo(&page, BatteryController::supplementName(), std::make_unique<BatteryController>(client));
}

}

#endif // BATTERY_STATUS

