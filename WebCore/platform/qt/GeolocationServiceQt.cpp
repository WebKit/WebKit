/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "GeolocationServiceQt.h"

#include "Geolocation.h"
#include "Geoposition.h"
#include "PositionError.h"
#include "PositionOptions.h"

using namespace QtMobility;

namespace WebCore {

GeolocationService::FactoryFunction* GeolocationService::s_factoryFunction = &GeolocationServiceQt::create;

GeolocationService* GeolocationServiceQt::create(GeolocationServiceClient* client)
{
    return new GeolocationServiceQt(client);
}

GeolocationServiceQt::GeolocationServiceQt(GeolocationServiceClient* client)
    : GeolocationService(client)
    , m_lastPosition(0)
    , m_lastError(0)
{
    m_location = QGeoPositionInfoSource::createDefaultSource(this);

    if (m_location)
        connect(m_location, SIGNAL(positionUpdated(QGeoPositionInfo)), this, SLOT(positionUpdated(QGeoPositionInfo)));
}

GeolocationServiceQt::~GeolocationServiceQt()
{
    delete m_location;
}

void GeolocationServiceQt::positionUpdated(const QGeoPositionInfo &geoPosition)
{
    if (!geoPosition.isValid())
        errorOccurred();

    QGeoCoordinate coord = geoPosition.coordinate();
    double latitude = coord.latitude();
    double longitude = coord.longitude();
    bool providesAltitude = (geoPosition.coordinate().type() == QGeoCoordinate::Coordinate3D);
    double altitude = coord.altitude();

    double accuracy = geoPosition.attribute(QGeoPositionInfo::HorizontalAccuracy);

    bool providesAltitudeAccuracy = geoPosition.hasAttribute(QGeoPositionInfo::VerticalAccuracy);
    double altitudeAccuracy = geoPosition.attribute(QGeoPositionInfo::VerticalAccuracy);

    bool providesHeading =  geoPosition.hasAttribute(QGeoPositionInfo::Direction);
    double heading = geoPosition.attribute(QGeoPositionInfo::Direction);

    bool providesSpeed = geoPosition.hasAttribute(QGeoPositionInfo::GroundSpeed);
    double speed = geoPosition.attribute(QGeoPositionInfo::GroundSpeed);

    RefPtr<Coordinates> coordinates = Coordinates::create(latitude, longitude, providesAltitude, altitude,
                                                          accuracy, providesAltitudeAccuracy, altitudeAccuracy,
                                                          providesHeading, heading, providesSpeed, speed);
    m_lastPosition = Geoposition::create(coordinates.release(), geoPosition.timestamp().toTime_t());
    positionChanged();
}

bool GeolocationServiceQt::startUpdating(PositionOptions*)
{
    m_lastPosition = 0;

    if (!m_location)
        return false;

    // TODO: handle enableHighAccuracy()

    m_location->startUpdates();
    return true;
}

void GeolocationServiceQt::stopUpdating()
{
    if (m_location)
        m_location->stopUpdates();
}

} // namespace WebCore
