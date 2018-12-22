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

#include "APIGeolocationProvider.h"
#include "WebGeolocationManagerProxy.h"
#include "WebGeolocationPosition.h"

namespace WebKit {

class GeolocationProvider : public API::GeolocationProvider {
public:
    explicit GeolocationProvider(WebKitGeolocationProvider& provider)
        : m_provider(provider)
    {
    }

private:
    void startUpdating(WebGeolocationManagerProxy&) override
    {
        m_provider.startUpdating();
    }

    void stopUpdating(WebGeolocationManagerProxy&) override
    {
        m_provider.stopUpdating();
    }

    WebKitGeolocationProvider& m_provider;
};

WebKitGeolocationProvider::~WebKitGeolocationProvider()
{
    m_provider.stopUpdating();
    m_geolocationManager->setProvider(nullptr);
}

WebKitGeolocationProvider::WebKitGeolocationProvider(WebGeolocationManagerProxy* geolocationManager)
    : m_geolocationManager(geolocationManager)
    , m_provider(this)
{
    ASSERT(geolocationManager);
    geolocationManager->setProvider(std::make_unique<GeolocationProvider>(*this));
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
    WebCore::GeolocationPosition corePosition { static_cast<double>(timestamp), latitude, longitude, accuracy };
    corePosition.altitude = altitude;
    corePosition.altitudeAccuracy = altitudeAccuracy;
    auto position = WebGeolocationPosition::create(WTFMove(corePosition));
    m_geolocationManager->providerDidChangePosition(position.ptr());
}

void WebKitGeolocationProvider::notifyErrorOccurred(const char* /* message */)
{
    m_geolocationManager->providerDidFailToDeterminePosition();
}

} // namespace WebKit

#endif // ENABLE(GEOLOCATION)
