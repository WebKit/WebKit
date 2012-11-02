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

#include <BlackBerryPlatformString.h>

using namespace WebCore;

static const BlackBerry::Platform::String frameOrigin(Frame* frame)
{
    DOMWindow* window = frame->document()->domWindow();
    SecurityOrigin* origin = window->document()->securityOrigin();
    return BlackBerry::Platform::String(origin->toString().utf8().data());
}

GeolocationClientBlackBerry::GeolocationClientBlackBerry(BlackBerry::WebKit::WebPagePrivate* webPagePrivate)
    : m_webPagePrivate(webPagePrivate)
    , m_accuracy(false)
    , m_isActive(false)
{
}

void GeolocationClientBlackBerry::geolocationDestroyed()
{
    delete this;
}

void GeolocationClientBlackBerry::startUpdating()
{
    if (!m_isActive)
        BlackBerry::Platform::GeolocationHandler::instance()->addListener(this, false);
    m_isActive = true;
}

void GeolocationClientBlackBerry::stopUpdating()
{
    if (m_isActive)
        BlackBerry::Platform::GeolocationHandler::instance()->removeListener(this);
    m_isActive = false;
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

    // FIXME: The geolocation setting for WebSettings is always true. Nothing ever toggles that setting.
    if (!m_webPagePrivate->m_webSettings->isGeolocationEnabled()) {
        location->setIsAllowed(false);
        return;
    }

    DOMWindow* window = frame->document()->domWindow();
    if (!window)
        return;

    const BlackBerry::Platform::String origin = frameOrigin(frame);
    m_pendingPermissionGeolocation = location;

    // Check global location setting, if it is off then we request an infobar that invokes a location settings card.
    // If it's on, then we request an infobar that allows the site to have permission to use geolocation.
    if (!BlackBerry::Platform::GeolocationHandler::instance()->isGlobalServiceActive()) {
        // We only want to ask them once per session. If we get here again, automatically fail the request.
        if (!BlackBerry::Platform::GeolocationHandler::instance()->didAskUserForGlobalPermission()) {
            m_webPagePrivate->m_client->requestGlobalLocalServicePermission(this, origin);
            BlackBerry::Platform::GeolocationHandler::instance()->setAskedUserForGlobalPermission();
        } else
            onPermission(false);
        return;
    }

    m_webPagePrivate->m_client->requestGeolocationPermission(this, origin);
}

void GeolocationClientBlackBerry::cancelPermissionRequest(Geolocation* location)
{
    m_webPagePrivate->m_client->cancelGeolocationPermission();
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

void GeolocationClientBlackBerry::onPermission(bool isAllowed)
{
    if (m_pendingPermissionGeolocation)
        m_pendingPermissionGeolocation->setIsAllowed(isAllowed);
}

void GeolocationClientBlackBerry::setEnableHighAccuracy(bool newAccuracy)
{
    // FIXME: we have to implement high accuracy on our side too
    m_accuracy = newAccuracy;
}

