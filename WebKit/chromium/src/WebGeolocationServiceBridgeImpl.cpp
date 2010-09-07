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
    bool isAttached() const;
    // Pointer back to the WebKit geolocation client. We obtain this via the frame's page, but need to cache it
    // as it may still be alive after the page has detached from the frame.
    WebGeolocationService* m_webGeolocationService;
    // GeolocationServiceChromium owns us, we only have a pointer back to it.
    GeolocationServiceChromium* m_geolocationServiceChromium;
    int m_bridgeId;
};

GeolocationServiceBridge* createGeolocationServiceBridgeImpl(GeolocationServiceChromium* geolocationServiceChromium)
{
    return new WebGeolocationServiceBridgeImpl(geolocationServiceChromium);
}

WebGeolocationServiceBridgeImpl::WebGeolocationServiceBridgeImpl(GeolocationServiceChromium* geolocationServiceChromium)
    : m_webGeolocationService(0)
    , m_geolocationServiceChromium(geolocationServiceChromium)
    , m_bridgeId(0)
{
}

WebGeolocationServiceBridgeImpl::~WebGeolocationServiceBridgeImpl()
{
    if (isAttached())
        m_webGeolocationService->detachBridge(m_bridgeId);
}

bool WebGeolocationServiceBridgeImpl::startUpdating(PositionOptions* positionOptions)
{
    attachBridgeIfNeeded();
    if (!isAttached())
        return false;
    m_webGeolocationService->startUpdating(m_bridgeId, m_geolocationServiceChromium->frame()->document()->url(), positionOptions->enableHighAccuracy());
    return true;
}

void WebGeolocationServiceBridgeImpl::stopUpdating()
{
    if (isAttached()) {
        m_webGeolocationService->stopUpdating(m_bridgeId);
        m_webGeolocationService->detachBridge(m_bridgeId);
        m_bridgeId = 0;
        m_webGeolocationService = 0;
    }
}

void WebGeolocationServiceBridgeImpl::suspend()
{
    if (isAttached())
        m_webGeolocationService->suspend(m_bridgeId);
}

void WebGeolocationServiceBridgeImpl::resume()
{
    if (isAttached())
        m_webGeolocationService->resume(m_bridgeId);
}

int WebGeolocationServiceBridgeImpl::getBridgeId() const
{
    return m_bridgeId;
}

void WebGeolocationServiceBridgeImpl::attachBridgeIfNeeded()
{
    if (isAttached())
        return;
    // Lazy attach to the geolocation service of the associated page if there is one.
    Frame* frame = m_geolocationServiceChromium->frame();
    if (!frame || !frame->page())
        return;
    WebKit::ChromeClientImpl* chromeClientImpl = static_cast<WebKit::ChromeClientImpl*>(frame->page()->chrome()->client());
    WebKit::WebViewClient* webViewClient = chromeClientImpl->webView()->client();
    m_webGeolocationService = webViewClient->geolocationService();
    ASSERT(m_webGeolocationService);
    m_bridgeId = m_webGeolocationService->attachBridge(this);
    if (!m_bridgeId) {
        // Attach failed. Release association with this service.
        m_webGeolocationService = 0;
    }
}

void WebGeolocationServiceBridgeImpl::setIsAllowed(bool allowed)
{
    m_geolocationServiceChromium->setIsAllowed(allowed);
}

void WebGeolocationServiceBridgeImpl::setLastPosition(double latitude, double longitude, bool providesAltitude, double altitude, double accuracy, bool providesAltitudeAccuracy, double altitudeAccuracy, bool providesHeading, double heading, bool providesSpeed, double speed, long long timestamp)
{
    RefPtr<Geoposition> geoposition = Geoposition::create(Coordinates::create(latitude, longitude, providesAltitude, altitude, accuracy, providesAltitudeAccuracy, altitudeAccuracy, providesHeading, heading, providesSpeed, speed), timestamp);
    m_geolocationServiceChromium->setLastPosition(geoposition);
}

void WebGeolocationServiceBridgeImpl::setLastError(int errorCode, const WebString& message)
{
    m_geolocationServiceChromium->setLastError(errorCode, message);
}

void WebGeolocationServiceBridgeImpl::onWebGeolocationServiceDestroyed()
{
    m_bridgeId = 0;
    m_webGeolocationService = 0;
}

bool WebGeolocationServiceBridgeImpl::isAttached() const
{
    // Test the class invariant.
    if (m_webGeolocationService)
        ASSERT(m_bridgeId);
    else     
        ASSERT(!m_bridgeId);

    return m_webGeolocationService;
}

} // namespace WebKit

#endif // ENABLE(GEOLOCATION)
