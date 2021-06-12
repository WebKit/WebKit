/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Apple Inc. All Rights Reserved.
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
#include "GeolocationClientMock.h"

#if ENABLE(GEOLOCATION)

#include "GeolocationController.h"
#include "GeolocationError.h"
#include "GeolocationPosition.h"

namespace WebCore {

GeolocationClientMock::GeolocationClientMock()
    : m_controller(0)
    , m_hasError(false)
    , m_controllerTimer(*this, &GeolocationClientMock::controllerTimerFired)
    , m_permissionTimer(*this, &GeolocationClientMock::permissionTimerFired)
    , m_isActive(false)
    , m_permissionState(PermissionStateUnset)
{
}

GeolocationClientMock::~GeolocationClientMock()
{
    ASSERT(!m_isActive);
}

void GeolocationClientMock::setController(GeolocationController *controller)
{
    ASSERT(controller && !m_controller);
    m_controller = controller;
}

void GeolocationClientMock::setPosition(GeolocationPositionData&& position)
{
    m_lastPosition = WTFMove(position);
    clearError();
    asyncUpdateController();
}

void GeolocationClientMock::setPositionUnavailableError(const String& errorMessage)
{
    m_hasError = true;
    m_errorMessage = errorMessage;
    m_lastPosition = std::nullopt;
    asyncUpdateController();
}

void GeolocationClientMock::setPermission(bool allowed)
{
    m_permissionState = allowed ? PermissionStateAllowed : PermissionStateDenied;
    asyncUpdatePermission();
}

int GeolocationClientMock::numberOfPendingPermissionRequests() const
{
    return m_pendingPermission.size();
}

void GeolocationClientMock::requestPermission(Geolocation& geolocation)
{
    m_pendingPermission.add(&geolocation);
    if (m_permissionState != PermissionStateUnset)
        asyncUpdatePermission();
}

void GeolocationClientMock::cancelPermissionRequest(Geolocation& geolocation)
{
    // Called from Geolocation::disconnectFrame() in response to Frame destruction.
    m_pendingPermission.remove(&geolocation);
    if (m_pendingPermission.isEmpty() && m_permissionTimer.isActive())
        m_permissionTimer.stop();
}

void GeolocationClientMock::asyncUpdatePermission()
{
    ASSERT(m_permissionState != PermissionStateUnset);
    if (!m_permissionTimer.isActive())
        m_permissionTimer.startOneShot(0_s);
}

void GeolocationClientMock::permissionTimerFired()
{
    ASSERT(m_permissionState != PermissionStateUnset);
    bool allowed = m_permissionState == PermissionStateAllowed;
    GeolocationSet::iterator end = m_pendingPermission.end();

    // Once permission has been set (or denied) on a Geolocation object, there can be
    // no further requests for permission to the mock. Consequently the callbacks
    // which fire synchronously from Geolocation::setIsAllowed() cannot reentrantly modify
    // m_pendingPermission.
    for (GeolocationSet::iterator it = m_pendingPermission.begin(); it != end; ++it)
        (*it)->setIsAllowed(allowed, { });
    m_pendingPermission.clear();
}

void GeolocationClientMock::reset()
{
    m_lastPosition = std::nullopt;
    clearError();
    m_permissionState = PermissionStateUnset;
}

void GeolocationClientMock::geolocationDestroyed()
{
    ASSERT(!m_isActive);
}

void GeolocationClientMock::startUpdating(const String& authorizationToken)
{
    ASSERT(!m_isActive);
    UNUSED_PARAM(authorizationToken);
    m_isActive = true;
    asyncUpdateController();
}

void GeolocationClientMock::stopUpdating()
{
    ASSERT(m_isActive);
    m_isActive = false;
    m_controllerTimer.stop();
}

void GeolocationClientMock::setEnableHighAccuracy(bool)
{
    // FIXME: We need to add some tests regarding "high accuracy" mode.
    // See https://bugs.webkit.org/show_bug.cgi?id=49438
}

std::optional<GeolocationPositionData> GeolocationClientMock::lastPosition()
{
    return m_lastPosition;
}

void GeolocationClientMock::asyncUpdateController()
{
    ASSERT(m_controller);
    if (m_isActive && !m_controllerTimer.isActive())
        m_controllerTimer.startOneShot(0_s);
}

void GeolocationClientMock::controllerTimerFired()
{
    ASSERT(m_controller);

    if (m_lastPosition) {
        ASSERT(!m_hasError);
        m_controller->positionChanged(*m_lastPosition);
    } else if (m_hasError) {
        auto geolocatioError = GeolocationError::create(GeolocationError::PositionUnavailable, m_errorMessage);
        m_controller->errorOccurred(geolocatioError.get());
    }
}

void GeolocationClientMock::clearError()
{
    m_hasError = false;
    m_errorMessage = String();
}

} // WebCore

#endif // ENABLE(GEOLOCATION)
