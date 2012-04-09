/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "BatteryClientImpl.h"

#if ENABLE(BATTERY_STATUS)

#include "BatteryController.h"
#include "BatteryStatus.h"
#include "EventNames.h"
#include "WebBatteryStatusClient.h"
#include <wtf/RefPtr.h>

namespace WebKit {

BatteryClientImpl::BatteryClientImpl(WebBatteryStatusClient* client)
    : m_client(client)
    , m_controller(0)
{
}

void BatteryClientImpl::updateBatteryStatus(const WebBatteryStatus& batteryStatus)
{
    if (m_controller) {
        RefPtr<WebCore::BatteryStatus> status = WebCore::BatteryStatus::create(batteryStatus.charging, batteryStatus.chargingTime, batteryStatus.dischargingTime, batteryStatus.level);
        m_controller->updateBatteryStatus(status);
    }
}

void BatteryClientImpl::setController(WebCore::BatteryController* controller)
{
    m_controller = controller;
}

void BatteryClientImpl::startUpdating()
{
    if (m_client)
        m_client->startUpdating();
}

void BatteryClientImpl::stopUpdating()
{
    if (m_client)
        m_client->stopUpdating();
}

void BatteryClientImpl::batteryControllerDestroyed()
{
    m_controller = 0;
}

} // namespace WebKit

#endif // ENABLE(BATTERY_STATUS)
