/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "BatteryProvider.h"

#if ENABLE(BATTERY_STATUS)

#include "WKAPICast.h"
#include "WKBatteryManager.h"
#include "WebBatteryManagerProxy.h"
#include "WebBatteryStatus.h"
#include "WebContext.h"

using namespace WebCore;
using namespace WebKit;

static inline BatteryProvider* toBatteryProvider(const void* clientInfo)
{
    return static_cast<BatteryProvider*>(const_cast<void*>(clientInfo));
}

static void startUpdatingCallback(WKBatteryManagerRef, const void* clientInfo)
{
    toBatteryProvider(clientInfo)->startUpdating();
}

static void stopUpdatingCallback(WKBatteryManagerRef, const void* clientInfo)
{
    toBatteryProvider(clientInfo)->stopUpdating();
}

BatteryProvider::~BatteryProvider()
{
    m_provider.stopUpdating();

    ASSERT(m_context->batteryManagerProxy());
    m_context->batteryManagerProxy()->initializeProvider(0);
}

PassRefPtr<BatteryProvider> BatteryProvider::create(PassRefPtr<WebContext> context)
{
    ASSERT(context);
    return adoptRef(new BatteryProvider(context));
}

BatteryProvider::BatteryProvider(PassRefPtr<WebContext> context)
    : m_context(context)
    , m_provider(this)
{
    ASSERT(context);

    WKBatteryProvider wkBatteryProvider = {
        kWKBatteryProviderCurrentVersion,
        this, // clientInfo
        startUpdatingCallback,
        stopUpdatingCallback
    };

    ASSERT(m_context->batteryManagerProxy());
    m_context->batteryManagerProxy()->initializeProvider(&wkBatteryProvider);
}

void BatteryProvider::startUpdating()
{
    m_provider.startUpdating();
}

void BatteryProvider::stopUpdating()
{
    m_provider.stopUpdating();
}

void BatteryProvider::didChangeBatteryStatus(const AtomicString& eventType, PassRefPtr<BatteryStatus> status)
{
    RefPtr<WebBatteryStatus> batteryStatus = WebBatteryStatus::create(status->charging(), status->chargingTime(), status->dischargingTime(), status->level());

    ASSERT(m_context->batteryManagerProxy());
    m_context->batteryManagerProxy()->providerDidChangeBatteryStatus(eventType, batteryStatus.get());
}
#endif // ENABLE(BATTERY_STATUS)
