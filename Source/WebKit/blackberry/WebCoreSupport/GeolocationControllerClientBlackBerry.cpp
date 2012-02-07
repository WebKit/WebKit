/*
 * Copyright (C) 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "GeolocationControllerClientBlackBerry.h"

#include "Chrome.h"
#include "Geolocation.h"
#include "GeolocationController.h"
#include "GeolocationError.h"
#include "Page.h"
#include "WebPage_p.h"

using namespace WebCore;

GeolocationControllerClientBlackBerry::GeolocationControllerClientBlackBerry(BlackBerry::WebKit::WebPagePrivate* webPagePrivate)
    : m_webPagePrivate(webPagePrivate)
    , m_tracker(0)
    , m_accuracy(false)
{
}

void GeolocationControllerClientBlackBerry::geolocationDestroyed()
{
    if (m_tracker)
        m_tracker->destroy();
    delete this;
}

void GeolocationControllerClientBlackBerry::startUpdating()
{
    if (m_tracker)
        m_tracker->resume();
    else
        m_tracker = BlackBerry::Platform::GeoTracker::create(this, 0, m_accuracy, -1, -1);
}

void GeolocationControllerClientBlackBerry::stopUpdating()
{
    if (m_tracker)
        m_tracker->suspend();
}

GeolocationPosition* GeolocationControllerClientBlackBerry::lastPosition()
{
    return m_lastPosition.get();
}

void GeolocationControllerClientBlackBerry::requestPermission(Geolocation* location)
{
    Frame* frame = location->frame();
    if (!frame)
        return;
    m_webPagePrivate->m_page->chrome()->requestGeolocationPermissionForFrame(frame, location);
}

void GeolocationControllerClientBlackBerry::cancelPermissionRequest(Geolocation* location)
{
    Frame* frame = location->frame();
    if (!frame)
        return;
    m_webPagePrivate->m_page->chrome()->cancelGeolocationPermissionRequestForFrame(frame, location);
}

void GeolocationControllerClientBlackBerry::onLocationUpdate(double timestamp, double latitude, double longitude, double accuracy, double altitude, bool altitudeValid,
                                                             double altitudeAccuracy, bool altitudeAccuracyValid, double speed, bool speedValid, double heading, bool headingValid)
{
    m_lastPosition = GeolocationPosition::create(timestamp, latitude, longitude, accuracy, altitudeValid, altitude, altitudeAccuracyValid,
                                                 altitudeAccuracy, headingValid, heading, speedValid, speed);
    m_webPagePrivate->m_page->geolocationController()->positionChanged(m_lastPosition.get());
}

void GeolocationControllerClientBlackBerry::onLocationError(const char* errorStr)
{
    RefPtr<GeolocationError> error = GeolocationError::create(GeolocationError::PositionUnavailable, String::fromUTF8(errorStr));
    m_webPagePrivate->m_page->geolocationController()->errorOccurred(error.get());
}

void GeolocationControllerClientBlackBerry::onPermission(void* context, bool isAllowed)
{
    Geolocation* position = static_cast<Geolocation*>(context);
    position->setIsAllowed(isAllowed);
}

void GeolocationControllerClientBlackBerry::setEnableHighAccuracy(bool newAccuracy)
{
    if (m_accuracy == newAccuracy)
        return;
    if (m_tracker) {
        m_tracker->destroy();
        m_tracker = BlackBerry::Platform::GeoTracker::create(this, 0, newAccuracy, -1, -1);
    }
}

