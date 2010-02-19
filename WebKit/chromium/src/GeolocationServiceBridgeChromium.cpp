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

#include "GeolocationServiceBridgeChromium.h"

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
using WebCore::String;

namespace WebKit {

class GeolocationServiceBridgeImpl : public GeolocationServiceBridge, public WebGeolocationServiceBridge {
public:
    explicit GeolocationServiceBridgeImpl(GeolocationServiceChromium*);
    virtual ~GeolocationServiceBridgeImpl();

    // GeolocationServiceBridge
    virtual bool startUpdating(PositionOptions*);
    virtual void stopUpdating();
    virtual void suspend();
    virtual void resume();
    virtual int getBridgeId() const;

    // WebGeolocationServiceBridge
    virtual void setIsAllowed(bool allowed);
    virtual void setLastPosition(double latitude, double longitude, bool providesAltitude, double altitude, double accuracy, bool providesAltitudeAccuracy, double altitudeAccuracy, bool providesHeading, double heading, bool providesSpeed, double speed, long long timestamp);
    virtual void setLastError(int errorCode, const WebString& message);

private:
    WebViewClient* getWebViewClient();

    // GeolocationServiceChromium owns us, we only have a pointer back to it.
    GeolocationServiceChromium* m_GeolocationServiceChromium;
    int m_bridgeId;
};

GeolocationServiceBridge* createGeolocationServiceBridgeImpl(GeolocationServiceChromium* geolocationServiceChromium)
{
    return new GeolocationServiceBridgeImpl(geolocationServiceChromium);
}

GeolocationServiceBridgeImpl::GeolocationServiceBridgeImpl(GeolocationServiceChromium* geolocationServiceChromium)
    : m_GeolocationServiceChromium(geolocationServiceChromium)
{
    // We need to attach ourselves here: Geolocation calls requestPermissionForFrame()
    // directly, and we need to be attached so that the embedder can call
    // our setIsAllowed().
    m_bridgeId = getWebViewClient()->getGeolocationService()->attachBridge(this);
    ASSERT(m_bridgeId);
}

GeolocationServiceBridgeImpl::~GeolocationServiceBridgeImpl()
{
    WebKit::WebViewClient* webViewClient = getWebViewClient();
    // Geolocation has an OwnPtr to us, and it's destroyed after the frame has
    // been potentially disconnected. In this case, it calls stopUpdating()
    // has been called and we have already dettached ourselves.
    if (!webViewClient) {
        ASSERT(!m_bridgeId);
    } else if (m_bridgeId)
        webViewClient->getGeolocationService()->dettachBridge(m_bridgeId);
}

bool GeolocationServiceBridgeImpl::startUpdating(PositionOptions* positionOptions)
{
    if (!m_bridgeId)
        m_bridgeId = getWebViewClient()->getGeolocationService()->attachBridge(this);
    getWebViewClient()->getGeolocationService()->startUpdating(m_bridgeId, positionOptions->enableHighAccuracy());
    //// FIXME: this will trigger a permission request regardless.
    //// Is it correct? confirm with andreip.
    // positionChanged();
    return true;
}

void GeolocationServiceBridgeImpl::stopUpdating()
{
    if (m_bridgeId) {
        WebGeolocationServiceInterface* geolocationService = getWebViewClient()->getGeolocationService();
        geolocationService->stopUpdating(m_bridgeId);
        geolocationService->dettachBridge(m_bridgeId);
        m_bridgeId = 0;
    }
}

void GeolocationServiceBridgeImpl::suspend()
{
    getWebViewClient()->getGeolocationService()->suspend(m_bridgeId);
}

void GeolocationServiceBridgeImpl::resume()
{
    getWebViewClient()->getGeolocationService()->resume(m_bridgeId);
}

int GeolocationServiceBridgeImpl::getBridgeId() const
{
    return m_bridgeId;
}

void GeolocationServiceBridgeImpl::setIsAllowed(bool allowed)
{
    m_GeolocationServiceChromium->setIsAllowed(allowed);
}

void GeolocationServiceBridgeImpl::setLastPosition(double latitude, double longitude, bool providesAltitude, double altitude, double accuracy, bool providesAltitudeAccuracy, double altitudeAccuracy, bool providesHeading, double heading, bool providesSpeed, double speed, long long timestamp)
{
    m_GeolocationServiceChromium->setLastPosition(latitude, longitude, providesAltitude, altitude, accuracy, providesAltitudeAccuracy, altitudeAccuracy, providesHeading, heading, providesSpeed, speed, timestamp);
}

void GeolocationServiceBridgeImpl::setLastError(int errorCode, const WebString& message)
{
    m_GeolocationServiceChromium->setLastError(errorCode, message);
}

WebViewClient* GeolocationServiceBridgeImpl::getWebViewClient()
{
    Frame* frame = m_GeolocationServiceChromium->frame();
    if (!frame || !frame->page())
        return 0;
    WebKit::ChromeClientImpl* chromeClientImpl = static_cast<WebKit::ChromeClientImpl*>(frame->page()->chrome()->client());
    WebKit::WebViewClient* webViewClient = chromeClientImpl->webView()->client();
    return webViewClient;
}

} // namespace WebKit

#endif // ENABLE(GEOLOCATION)
