/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
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
#include "GeolocationServiceChromium.h"

#include "ChromiumBridge.h"

namespace WebCore {

GeolocationServiceBridge::~GeolocationServiceBridge()
{
}

GeolocationServiceChromium::GeolocationServiceChromium(GeolocationServiceClient* c)
        : GeolocationService(c),
          m_geolocation(static_cast<Geolocation*>(c)),
          m_geolocationServiceBridge(ChromiumBridge::createGeolocationServiceBridge(this)),
          m_lastError(PositionError::create(PositionError::POSITION_UNAVAILABLE, ""))
{
}

void GeolocationServiceChromium::setIsAllowed(bool allowed)
{
    m_geolocation->setIsAllowed(allowed);
}

void GeolocationServiceChromium::setLastPosition(PassRefPtr<Geoposition> geoposition)
{
    m_lastPosition = geoposition;
    positionChanged();
}

void GeolocationServiceChromium::setLastError(int errorCode, const String& message)
{
    m_lastError = PositionError::create(static_cast<PositionError::ErrorCode>(errorCode), message);
    errorOccurred();
}

Frame* GeolocationServiceChromium::frame()
{
    return m_geolocation->frame();
}

bool GeolocationServiceChromium::startUpdating(PositionOptions* options)
{
    return m_geolocationServiceBridge->startUpdating(options);
}

void GeolocationServiceChromium::stopUpdating()
{
    return m_geolocationServiceBridge->stopUpdating();
}

void GeolocationServiceChromium::suspend()
{
    return m_geolocationServiceBridge->suspend();
}

void GeolocationServiceChromium::resume()
{
    return m_geolocationServiceBridge->resume();
}

Geoposition* GeolocationServiceChromium::lastPosition() const
{
    return m_lastPosition.get();
}

PositionError* GeolocationServiceChromium::lastError() const
{
    return m_lastError.get();
}

static GeolocationService* createGeolocationServiceChromium(GeolocationServiceClient* c)
{
    return new GeolocationServiceChromium(c);
}

// Sets up the factory function for GeolocationService.
GeolocationService::FactoryFunction* GeolocationService::s_factoryFunction = &createGeolocationServiceChromium;

} // namespace WebCore
