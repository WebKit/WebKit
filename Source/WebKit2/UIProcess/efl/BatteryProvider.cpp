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
#include "WKBatteryStatus.h"
#include "WKContext.h"

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

    WKBatteryManagerRef wkBatteryManager = WKContextGetBatteryManager(m_wkContext.get());
    ASSERT(wkBatteryManager);

    WKBatteryManagerSetProvider(wkBatteryManager, 0);
}

PassRefPtr<BatteryProvider> BatteryProvider::create(WKContextRef wkContext)
{
    return adoptRef(new BatteryProvider(wkContext));
}

BatteryProvider::BatteryProvider(WKContextRef wkContext)
    : m_wkContext(wkContext)
    , m_provider(this)
{
    ASSERT(m_wkContext);

    WKBatteryManagerRef wkBatteryManager = WKContextGetBatteryManager(m_wkContext.get());
    ASSERT(wkBatteryManager);

    WKBatteryProvider wkBatteryProvider = {
        kWKBatteryProviderCurrentVersion,
        this, // clientInfo
        startUpdatingCallback,
        stopUpdatingCallback
    };
    WKBatteryManagerSetProvider(wkBatteryManager, &wkBatteryProvider);
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
    WKBatteryManagerRef wkBatteryManager = WKContextGetBatteryManager(m_wkContext.get());
    ASSERT(wkBatteryManager);

    WKRetainPtr<WKBatteryStatusRef> wkBatteryStatus(AdoptWK, WKBatteryStatusCreate(status->charging(), status->chargingTime(), status->dischargingTime(), status->level()));
    WKBatteryManagerProviderDidChangeBatteryStatus(wkBatteryManager, toAPI(eventType.impl()), wkBatteryStatus.get());
}

#endif // ENABLE(BATTERY_STATUS)
