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
#include "WebGeolocationServiceBridge.h"
#include "WebString.h"
#include <wtf/CurrentTime.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

#if ENABLE(GEOLOCATION)

#if ENABLE(CLIENT_BASED_GEOLOCATION)
// FIXME: Implement mock bindings for client-based geolocation. Ultimately
// move to another class and remove WebGeolocationService*.

namespace WebKit {

class WebGeolocationServiceMockClientBasedImpl : public WebGeolocationServiceMock {
};

WebGeolocationServiceMock* WebGeolocationServiceMock::createWebGeolocationServiceMock()
{
    return new WebGeolocationServiceMockClientBasedImpl;
}

void WebGeolocationServiceMock::setMockGeolocationPermission(bool allowed)
{
    // FIXME: Implement mock binding
}

void WebGeolocationServiceMock::setMockGeolocationPosition(double latitude, double longitude, double accuracy)
{
    // FIXME: Implement mock binding
}

void WebGeolocationServiceMock::setMockGeolocationError(int errorCode, const WebString& message)
{
    // FIXME: Implement mock binding
}

} // namespace WebKit

#else
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
using WTF::Vector;

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
    WebGeolocationServiceMockImpl();
    virtual ~WebGeolocationServiceMockImpl();
    static void setMockGeolocationPermission(bool allowed);

    // WebGeolocationService
    virtual void requestPermissionForFrame(int bridgeId, const WebURL& url);
    virtual int attachBridge(WebGeolocationServiceBridge*);
    virtual void detachBridge(int bridgeId);

private:
    void notifyPendingPermissions();

    typedef HashMap<int, WebGeolocationServiceBridge*> IdToBridgeMap;
    IdToBridgeMap m_idToBridgeMap;
    Vector<int> m_pendingPermissionRequests;

    // In addition to the singleton instance pointer, we need to keep the setMockGeolocationPermission() state
    // as a static (not object members) as this call may come in before the service has been created.
    static enum PermissionState {
        PermissionStateUnset,
        PermissionStateAllowed,
        PermissionStateDenied,
    } s_permissionState;
    static WebGeolocationServiceMockImpl* s_instance;
};

WebGeolocationServiceMockImpl::PermissionState WebGeolocationServiceMockImpl::s_permissionState = WebGeolocationServiceMockImpl::PermissionStateUnset;
WebGeolocationServiceMockImpl* WebGeolocationServiceMockImpl::s_instance = 0;

WebGeolocationServiceMock* WebGeolocationServiceMock::createWebGeolocationServiceMock()
{
    return new WebGeolocationServiceMockImpl;
}

void WebGeolocationServiceMock::setMockGeolocationPermission(bool allowed)
{
    WebGeolocationServiceMockImpl::setMockGeolocationPermission(allowed);
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

WebGeolocationServiceMockImpl::WebGeolocationServiceMockImpl()
{
    ASSERT(!s_instance);
    s_instance = this;
}

WebGeolocationServiceMockImpl::~WebGeolocationServiceMockImpl()
{
    ASSERT(this == s_instance);
    s_instance = 0;
    // Reset the permission state, so any future service instance (e.g. running
    // multiple tests in a single DRT run) will see a clean call sequence.
    s_permissionState = PermissionStateUnset;
    for (IdToBridgeMap::iterator it = m_idToBridgeMap.begin(); it != m_idToBridgeMap.end(); ++it)
        it->second->didDestroyGeolocationService();
}

void WebGeolocationServiceMockImpl::setMockGeolocationPermission(bool allowed)
{
    s_permissionState = allowed ? PermissionStateAllowed : PermissionStateDenied;
    if (s_instance)
        s_instance->notifyPendingPermissions();
}

void WebGeolocationServiceMockImpl::requestPermissionForFrame(int bridgeId, const WebURL& url)
{
    m_pendingPermissionRequests.append(bridgeId);
    if (s_permissionState != PermissionStateUnset)
        notifyPendingPermissions();
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

void WebGeolocationServiceMockImpl::notifyPendingPermissions()
{
    ASSERT(s_permissionState == PermissionStateAllowed || s_permissionState ==  PermissionStateDenied);
    Vector<int> pendingPermissionRequests;
    pendingPermissionRequests.swap(m_pendingPermissionRequests);
    for (Vector<int>::const_iterator it = pendingPermissionRequests.begin(); it != pendingPermissionRequests.end(); ++it) {
        ASSERT(*it > 0);
        IdToBridgeMap::iterator iter = m_idToBridgeMap.find(*it);
        if (iter != m_idToBridgeMap.end())
            iter->second->setIsAllowed(s_permissionState == PermissionStateAllowed);
    }
}

} // namespace WebKit

#endif // ENABLE(CLIENT_BASED_GEOLOCATION)
#endif // ENABLE(GEOLOCATION)
