/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "WebGeolocationServiceBridgeImpl.h"

#include "Chrome.h"
#include "ChromeClientImpl.h"
#include "Frame.h"
#include "Geolocation.h"
#include "GeolocationServiceChromium.h"
#include "Geoposition.h"
#include "Page.h"
#include "PositionError.h"
#include "PositionOptions.h"
#include "WebFrame.h"
#include "WebFrameImpl.h"
#include "WebGeolocationService.h"
#include "WebGeolocationServiceBridge.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"

#if ENABLE(GEOLOCATION)

using WebCore::Coordinates;
using WebCore::Frame;
using WebCore::Geolocation;
using WebCore::GeolocationServiceBridge;
using WebCore::GeolocationServiceChromium;
using WebCore::GeolocationServiceClient;
using WebCore::Geoposition;
using WebCore::PositionError;
using WebCore::PositionOptions;
using WTF::String;

namespace WebKit {

class WebGeolocationServiceBridgeImpl : public GeolocationServiceBridge, public WebGeolocationServiceBridge {
public:
    explicit WebGeolocationServiceBridgeImpl(GeolocationServiceChromium*);
    virtual ~WebGeolocationServiceBridgeImpl();

    // GeolocationServiceBridge
    virtual bool startUpdating(PositionOptions*);
    virtual void stopUpdating();
    virtual void suspend();
    virtual void resume();
    virtual int getBridgeId() const;
    virtual void attachBridgeIfNeeded();

    // WebGeolocationServiceBridge
    virtual void setIsAllowed(bool allowed);
    virtual void setLastPosition(double latitude, double longitude, bool providesAltitude, double altitude, double accuracy, bool providesAltitudeAccuracy, double altitudeAccuracy, bool providesHeading, double heading, bool providesSpeed, double speed, long long timestamp);
    virtual void setLastError(int errorCode, const WebString& message);
    virtual void onWebGeolocationServiceDestroyed();

private:
    WebViewClient* getWebViewClient();

    // GeolocationServiceChromium owns us, we only have a pointer back to it.
    GeolocationServiceChromium* m_GeolocationServiceChromium;
    int m_bridgeId;
};

GeolocationServiceBridge* createGeolocationServiceBridgeImpl(GeolocationServiceChromium* geolocationServiceChromium)
{
    return new WebGeolocationServiceBridgeImpl(geolocationServiceChromium);
}

WebGeolocationServiceBridgeImpl::WebGeolocationServiceBridgeImpl(GeolocationServiceChromium* geolocationServiceChromium)
    : m_GeolocationServiceChromium(geolocationServiceChromium)
    , m_bridgeId(0)
{
}

WebGeolocationServiceBridgeImpl::~WebGeolocationServiceBridgeImpl()
{
    WebKit::WebViewClient* webViewClient = getWebViewClient();
    // Geolocation has an OwnPtr to us, and it's destroyed after the frame has
    // been potentially disconnected. In this case, it calls stopUpdating()
    // has been called and we have already detached ourselves.
    if (!webViewClient)
        ASSERT(!m_bridgeId);
    else if (m_bridgeId)
        webViewClient->geolocationService()->detachBridge(m_bridgeId);
}

bool WebGeolocationServiceBridgeImpl::startUpdating(PositionOptions* positionOptions)
{
    attachBridgeIfNeeded();
    getWebViewClient()->geolocationService()->startUpdating(m_bridgeId, m_GeolocationServiceChromium->frame()->document()->url(), positionOptions->enableHighAccuracy());
    return true;
}

void WebGeolocationServiceBridgeImpl::stopUpdating()
{
    WebViewClient* webViewClient = getWebViewClient();
    if (m_bridgeId && webViewClient) {
        WebGeolocationService* geolocationService = webViewClient->geolocationService();
        geolocationService->stopUpdating(m_bridgeId);
        geolocationService->detachBridge(m_bridgeId);
    }
    m_bridgeId = 0;
}

void WebGeolocationServiceBridgeImpl::suspend()
{
    getWebViewClient()->geolocationService()->suspend(m_bridgeId);
}

void WebGeolocationServiceBridgeImpl::resume()
{
    getWebViewClient()->geolocationService()->resume(m_bridgeId);
}

int WebGeolocationServiceBridgeImpl::getBridgeId() const
{
    return m_bridgeId;
}

void WebGeolocationServiceBridgeImpl::attachBridgeIfNeeded()
{
    if (!m_bridgeId)
        m_bridgeId = getWebViewClient()->geolocationService()->attachBridge(this);
}

void WebGeolocationServiceBridgeImpl::setIsAllowed(bool allowed)
{
    m_GeolocationServiceChromium->setIsAllowed(allowed);
}

void WebGeolocationServiceBridgeImpl::setLastPosition(double latitude, double longitude, bool providesAltitude, double altitude, double accuracy, bool providesAltitudeAccuracy, double altitudeAccuracy, bool providesHeading, double heading, bool providesSpeed, double speed, long long timestamp)
{
    RefPtr<Geoposition> geoposition = Geoposition::create(Coordinates::create(latitude, longitude, providesAltitude, altitude, accuracy, providesAltitudeAccuracy, altitudeAccuracy, providesHeading, heading, providesSpeed, speed), timestamp);
    m_GeolocationServiceChromium->setLastPosition(geoposition);
}

void WebGeolocationServiceBridgeImpl::setLastError(int errorCode, const WebString& message)
{
    m_GeolocationServiceChromium->setLastError(errorCode, message);
}

WebViewClient* WebGeolocationServiceBridgeImpl::getWebViewClient()
{
    Frame* frame = m_GeolocationServiceChromium->frame();
    if (!frame || !frame->page())
        return 0;
    WebKit::ChromeClientImpl* chromeClientImpl = static_cast<WebKit::ChromeClientImpl*>(frame->page()->chrome()->client());
    WebKit::WebViewClient* webViewClient = chromeClientImpl->webView()->client();
    return webViewClient;
}

void WebGeolocationServiceBridgeImpl::onWebGeolocationServiceDestroyed()
{
}

} // namespace WebKit

#endif // ENABLE(GEOLOCATION)
