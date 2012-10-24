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
#include "GeolocationClientBlackBerry.h"

#include "Chrome.h"
#include "Geolocation.h"
#include "GeolocationController.h"
#include "GeolocationError.h"
#include "Page.h"
#include "WebPage_p.h"

using namespace WebCore;

static CString frameOrigin(Frame* frame)
{
    DOMWindow* window = frame->document()->domWindow();
    SecurityOrigin* origin = window->document()->securityOrigin();
    CString latinOrigin = origin->toString().latin1();
    return latinOrigin;
}

GeolocationClientBlackBerry::GeolocationClientBlackBerry(BlackBerry::WebKit::WebPagePrivate* webPagePrivate)
    : m_webPagePrivate(webPagePrivate)
    , m_tracker(0)
    , m_accuracy(false)
{
}

void GeolocationClientBlackBerry::geolocationDestroyed()
{
    if (m_tracker)
        m_tracker->destroy();
    delete this;
}

void GeolocationClientBlackBerry::startUpdating()
{
    if (m_tracker)
        m_tracker->resume();
    else
        m_tracker = BlackBerry::Platform::GeoTracker::create(this, m_accuracy);
}

void GeolocationClientBlackBerry::stopUpdating()
{
    if (m_tracker)
        m_tracker->suspend();
}

GeolocationPosition* GeolocationClientBlackBerry::lastPosition()
{
    return m_lastPosition.get();
}

void GeolocationClientBlackBerry::requestPermission(Geolocation* location)
{
    Frame* frame = location->frame();
    if (!frame)
        return;
    m_webPagePrivate->m_page->chrome()->client()->requestGeolocationPermissionForFrame(frame, location);
}

void GeolocationClientBlackBerry::cancelPermissionRequest(Geolocation* location)
{
    Frame* frame = location->frame();
    if (!frame)
        return;
    m_webPagePrivate->m_page->chrome()->client()->cancelGeolocationPermissionRequestForFrame(frame, location);
}

void GeolocationClientBlackBerry::onLocationUpdate(double timestamp, double latitude, double longitude, double accuracy, double altitude, bool altitudeValid,
                                                             double altitudeAccuracy, bool altitudeAccuracyValid, double speed, bool speedValid, double heading, bool headingValid)
{
    m_lastPosition = GeolocationPosition::create(timestamp, latitude, longitude, accuracy, altitudeValid, altitude, altitudeAccuracyValid,
                                                 altitudeAccuracy, headingValid, heading, speedValid, speed);
    GeolocationController::from(m_webPagePrivate->m_page)->positionChanged(m_lastPosition.get());
}

void GeolocationClientBlackBerry::onLocationError(const char* errorStr)
{
    RefPtr<GeolocationError> error = GeolocationError::create(GeolocationError::PositionUnavailable, String::fromUTF8(errorStr));
    GeolocationController::from(m_webPagePrivate->m_page)->errorOccurred(error.get());
}

void GeolocationClientBlackBerry::onPermission(void* context, bool isAllowed)
{
    Geolocation* position = static_cast<Geolocation*>(context);
    position->setIsAllowed(isAllowed);
}

void GeolocationClientBlackBerry::setEnableHighAccuracy(bool newAccuracy)
{
    if (m_accuracy == newAccuracy)
        return;

    m_accuracy = newAccuracy;

    if (m_tracker)
        m_tracker->setRequiresHighAccuracy(m_accuracy);
}

