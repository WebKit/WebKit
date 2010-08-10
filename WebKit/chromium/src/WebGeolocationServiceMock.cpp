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
#include "WebGeolocationServiceMock.h"

#include "GeolocationService.h"
#include "GeolocationServiceChromium.h"
#include "GeolocationServiceMock.h"
#include "WebString.h"
#include <wtf/CurrentTime.h>
#include <wtf/HashMap.h>

#if ENABLE(GEOLOCATION)

using WebCore::Coordinates;
using WebCore::Frame;
using WebCore::Geolocation;
using WebCore::GeolocationServiceBridge;
using WebCore::GeolocationServiceChromium;
using WebCore::GeolocationServiceClient;
using WebCore::GeolocationServiceMock;
using WebCore::Geoposition;
using WebCore::PositionError;
using WebCore::PositionOptions;
using WTF::String;

namespace WebCore {
class GeolocationServiceChromiumMock : public GeolocationServiceChromium, public GeolocationServiceClient {
public:
    static GeolocationService* create(GeolocationServiceClient*);
    virtual bool startUpdating(PositionOptions*);
    virtual void stopUpdating();
    virtual Geoposition* lastPosition() const;
    virtual PositionError* lastError() const;

    virtual void geolocationServicePositionChanged(GeolocationService*);
    virtual void geolocationServiceErrorOccurred(GeolocationService*);

private:
    explicit GeolocationServiceChromiumMock(GeolocationServiceClient*);

    GeolocationServiceClient* m_geolocationServiceClient;
    OwnPtr<GeolocationService> m_geolocationServiceMock;
};

GeolocationService* GeolocationServiceChromiumMock::create(GeolocationServiceClient* geolocationServiceClient)
{
    return new GeolocationServiceChromiumMock(geolocationServiceClient);
}

GeolocationServiceChromiumMock::GeolocationServiceChromiumMock(GeolocationServiceClient* geolocationServiceClient)
    : GeolocationServiceChromium(geolocationServiceClient),
      m_geolocationServiceClient(geolocationServiceClient)
{
      m_geolocationServiceMock.set(GeolocationServiceMock::create(this));
}

bool GeolocationServiceChromiumMock::startUpdating(PositionOptions* positionOptions)
{
    GeolocationServiceChromium::startUpdating(positionOptions);
    return m_geolocationServiceMock->startUpdating(positionOptions);
}

void GeolocationServiceChromiumMock::stopUpdating()
{
    GeolocationServiceChromium::stopUpdating();
    m_geolocationServiceMock->stopUpdating();
}

Geoposition* GeolocationServiceChromiumMock::lastPosition() const
{
    return m_geolocationServiceMock->lastPosition();
}

PositionError* GeolocationServiceChromiumMock::lastError() const
{
    return m_geolocationServiceMock->lastError();
}

void GeolocationServiceChromiumMock::geolocationServicePositionChanged(GeolocationService* geolocationService)
{
    ASSERT_UNUSED(geolocationService, geolocationService == m_geolocationServiceMock);
    m_geolocationServiceClient->geolocationServicePositionChanged(this);

}

void GeolocationServiceChromiumMock::geolocationServiceErrorOccurred(GeolocationService* geolocationService)
{
    ASSERT_UNUSED(geolocationService, geolocationService == m_geolocationServiceMock);
    m_geolocationServiceClient->geolocationServiceErrorOccurred(this);
}

} // namespace WebCore

namespace WebKit {

class WebGeolocationServiceMockImpl : public WebGeolocationServiceMock {
public:
    virtual ~WebGeolocationServiceMockImpl() { }
    virtual void requestPermissionForFrame(int bridgeId, const WebURL& url);
    virtual int attachBridge(WebGeolocationServiceBridge*);
    virtual void detachBridge(int bridgeId);

private:
    typedef HashMap<int, WebGeolocationServiceBridge*> IdToBridgeMap;
    IdToBridgeMap m_idToBridgeMap;
};

bool WebGeolocationServiceMock::s_mockGeolocationPermission = false;

WebGeolocationServiceMock* WebGeolocationServiceMock::createWebGeolocationServiceMock()
{
    return new WebGeolocationServiceMockImpl;
}

void WebGeolocationServiceMock::setMockGeolocationPermission(bool allowed)
{
    s_mockGeolocationPermission = allowed;
}

void WebGeolocationServiceMock::setMockGeolocationPosition(double latitude, double longitude, double accuracy)
{
    WebCore::GeolocationService::setCustomMockFactory(&WebCore::GeolocationServiceChromiumMock::create);
    RefPtr<Geoposition> geoposition = Geoposition::create(Coordinates::create(latitude, longitude, false, 0, accuracy, true, 0, false, 0, false, 0), currentTime() * 1000.0);
    GeolocationServiceMock::setPosition(geoposition);
}

void WebGeolocationServiceMock::setMockGeolocationError(int errorCode, const WebString& message)
{
    WebCore::GeolocationService::setCustomMockFactory(&WebCore::GeolocationServiceChromiumMock::create);
    RefPtr<PositionError> positionError = PositionError::create(static_cast<PositionError::ErrorCode>(errorCode), message);
    GeolocationServiceMock::setError(positionError);
}

void WebGeolocationServiceMockImpl::requestPermissionForFrame(int bridgeId, const WebURL& url)
{
    IdToBridgeMap::iterator iter = m_idToBridgeMap.find(bridgeId);
    if (iter == m_idToBridgeMap.end())
        return;
    iter->second->setIsAllowed(s_mockGeolocationPermission);
}

int WebGeolocationServiceMockImpl::attachBridge(WebGeolocationServiceBridge* bridge)
{
    static int nextAvailableWatchId = 1;
    // In case of overflow, make sure the ID remains positive, but reuse the ID values.
    if (nextAvailableWatchId < 1)
        nextAvailableWatchId = 1;
    m_idToBridgeMap.set(nextAvailableWatchId, bridge);
    return nextAvailableWatchId++;
}

void WebGeolocationServiceMockImpl::detachBridge(int bridgeId)
{
    m_idToBridgeMap.remove(bridgeId);
}

} // namespace WebKit

#endif // ENABLE(GEOLOCATION)
