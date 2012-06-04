/*
 * Copyright (C) 2012 Igalia S.L.
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
#include "WebKitGeolocationProvider.h"

#if ENABLE(GEOLOCATION)

static inline WebKitGeolocationProvider* toGeolocationProvider(const void* clientInfo)
{
    return static_cast<WebKitGeolocationProvider*>(const_cast<void*>(clientInfo));
}

static void startUpdatingCallback(WKGeolocationManagerRef geolocationManager, const void* clientInfo)
{
    toGeolocationProvider(clientInfo)->startUpdating();
}

static void stopUpdatingCallback(WKGeolocationManagerRef geolocationManager, const void* clientInfo)
{
    toGeolocationProvider(clientInfo)->stopUpdating();
}

WebKitGeolocationProvider::~WebKitGeolocationProvider()
{
    m_provider.stopUpdating();
}

PassRefPtr<WebKitGeolocationProvider> WebKitGeolocationProvider::create(WKGeolocationManagerRef wkGeolocationManager)
{
    return adoptRef(new WebKitGeolocationProvider(wkGeolocationManager));
}

WebKitGeolocationProvider::WebKitGeolocationProvider(WKGeolocationManagerRef wkGeolocationManager)
    : m_wkGeolocationManager(wkGeolocationManager)
    , m_provider(this)
{
    ASSERT(wkGeolocationManager);

    WKGeolocationProvider wkGeolocationProvider = {
        kWKGeolocationProviderCurrentVersion,
        this, // clientInfo
        startUpdatingCallback,
        stopUpdatingCallback
    };
    WKGeolocationManagerSetProvider(m_wkGeolocationManager.get(), &wkGeolocationProvider);
}

void WebKitGeolocationProvider::startUpdating()
{
    m_provider.startUpdating();
}

void WebKitGeolocationProvider::stopUpdating()
{
    m_provider.stopUpdating();
}

void WebKitGeolocationProvider::notifyPositionChanged(int timestamp, double latitude, double longitude, double altitude, double accuracy, double altitudeAccuracy)
{
    WKRetainPtr<WKGeolocationPositionRef> wkGeolocationPosition(AdoptWK, WKGeolocationPositionCreate(timestamp, latitude, longitude, accuracy));
    WKGeolocationManagerProviderDidChangePosition(m_wkGeolocationManager.get(), wkGeolocationPosition.get());
}

void WebKitGeolocationProvider::notifyErrorOccurred(const char* message)
{
    WKGeolocationManagerProviderDidFailToDeterminePosition(m_wkGeolocationManager.get());
}

#endif // ENABLE(GEOLOCATION)
