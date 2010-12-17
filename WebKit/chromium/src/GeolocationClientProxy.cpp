/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GeolocationClientProxy.h"

#include "Geolocation.h"
#include "GeolocationPosition.h"
#include "WebGeolocationClient.h"
#include "WebGeolocationController.h"
#include "WebGeolocationPermissionRequest.h"
#include "WebGeolocationPosition.h"

namespace WebKit {

GeolocationClientProxy::GeolocationClientProxy(WebGeolocationClient* client)
    : m_client(client)
{
}

GeolocationClientProxy::~GeolocationClientProxy()
{
}

void GeolocationClientProxy::setController(WebCore::GeolocationController* controller)
{
    // We support there not being a client, provided we don't do any Geolocation.
    if (m_client) {
        // Ownership of the WebGeolocationController is transferred to the client.
        m_client->setController(new WebGeolocationController(controller));
    }
}

void GeolocationClientProxy::geolocationDestroyed()
{
    if (m_client)
        m_client->geolocationDestroyed();
}

void GeolocationClientProxy::startUpdating()
{
    m_client->startUpdating();
}

void GeolocationClientProxy::stopUpdating()
{
    m_client->stopUpdating();
}

void GeolocationClientProxy::setEnableHighAccuracy(bool highAccuracy)
{
    m_client->setEnableHighAccuracy(highAccuracy);
}

WebCore::GeolocationPosition* GeolocationClientProxy::lastPosition()
{
    WebGeolocationPosition webPosition;
    if (m_client->lastPosition(webPosition))
        m_lastPosition = webPosition;
    else
        m_lastPosition.clear();

    return m_lastPosition.get();
}

void GeolocationClientProxy::requestPermission(WebCore::Geolocation* geolocation)
{
    m_client->requestPermission(WebGeolocationPermissionRequest(geolocation));
}

void GeolocationClientProxy::cancelPermissionRequest(WebCore::Geolocation* geolocation)
{
    m_client->cancelPermissionRequest(WebGeolocationPermissionRequest(geolocation));
}

}
