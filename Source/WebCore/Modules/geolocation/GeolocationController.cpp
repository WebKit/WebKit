/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GeolocationController.h"

#if ENABLE(GEOLOCATION)

#include "GeolocationClient.h"
#include "GeolocationError.h"
#include "GeolocationPositionData.h"

namespace WebCore {

GeolocationController::GeolocationController(Page& page, GeolocationClient& client)
    : m_page(page)
    , m_client(client)
{
    m_page.addActivityStateChangeObserver(*this);
}

GeolocationController::~GeolocationController()
{
    ASSERT(m_observers.isEmpty());

    // NOTE: We don't have to remove ourselves from page's ActivityStateChangeObserver set, since
    // we are supplement of the Page, and our destructor getting called means the page is being
    // torn down.

    m_client.geolocationDestroyed();
}

void GeolocationController::didNavigatePage()
{
    while (!m_observers.isEmpty())
        removeObserver(m_observers.begin()->get());
}

void GeolocationController::addObserver(Geolocation& observer, bool enableHighAccuracy)
{
    bool highAccuracyWasRequired = needsHighAccuracy();

    m_observers.add(observer);
    if (enableHighAccuracy)
        m_highAccuracyObservers.add(observer);

    if (m_isUpdating) {
        if (!highAccuracyWasRequired && enableHighAccuracy)
            m_client.setEnableHighAccuracy(true);
    } else
        startUpdatingIfNecessary();
}

void GeolocationController::removeObserver(Geolocation& observer)
{
    if (!m_observers.contains(observer))
        return;

    bool highAccuracyWasRequired = needsHighAccuracy();

    m_observers.remove(observer);
    m_highAccuracyObservers.remove(observer);

    if (!m_isUpdating)
        return;

    if (m_observers.isEmpty())
        stopUpdatingIfNecessary();
    else if (highAccuracyWasRequired && !needsHighAccuracy())
        m_client.setEnableHighAccuracy(false);
}

void GeolocationController::revokeAuthorizationToken(const String& authorizationToken)
{
    m_client.revokeAuthorizationToken(authorizationToken);
}

void GeolocationController::requestPermission(Geolocation& geolocation)
{
    if (!m_page.isVisible()) {
        m_pendingPermissionRequest.add(geolocation);
        return;
    }

    m_client.requestPermission(geolocation);
}

void GeolocationController::cancelPermissionRequest(Geolocation& geolocation)
{
    if (m_pendingPermissionRequest.remove(geolocation))
        return;

    m_client.cancelPermissionRequest(geolocation);
}

void GeolocationController::positionChanged(const std::optional<GeolocationPositionData>& position)
{
    m_lastPosition = position;
    for (auto& observer : copyToVectorOf<Ref<Geolocation>>(m_observers))
        observer->positionChanged();
}

void GeolocationController::errorOccurred(GeolocationError& error)
{
    for (auto& observer : copyToVectorOf<Ref<Geolocation>>(m_observers))
        observer->setError(error);
}

std::optional<GeolocationPositionData> GeolocationController::lastPosition()
{
    if (m_lastPosition)
        return m_lastPosition.value();

    return m_client.lastPosition();
}

void GeolocationController::activityStateDidChange(OptionSet<ActivityState> oldActivityState, OptionSet<ActivityState> newActivityState)
{
    // Toggle GPS based on page visibility to save battery.
    auto changed = oldActivityState ^ newActivityState;
    if (changed & ActivityState::IsVisible && !m_observers.isEmpty()) {
        if (newActivityState & ActivityState::IsVisible)
            startUpdatingIfNecessary();
        else
            stopUpdatingIfNecessary();
    }

    if (!m_page.isVisible())
        return;

    auto pendedPermissionRequests = WTFMove(m_pendingPermissionRequest);
    for (auto& permissionRequest : pendedPermissionRequests)
        m_client.requestPermission(permissionRequest.get());
}

void GeolocationController::startUpdatingIfNecessary()
{
    if (m_isUpdating || !m_page.isVisible() || m_observers.isEmpty())
        return;

    m_client.startUpdating((*m_observers.random())->authorizationToken(), needsHighAccuracy());
    m_isUpdating = true;
}

void GeolocationController::stopUpdatingIfNecessary()
{
    if (!m_isUpdating)
        return;

    m_client.stopUpdating();
    m_isUpdating = false;
}

ASCIILiteral GeolocationController::supplementName()
{
    return "GeolocationController"_s;
}

void provideGeolocationTo(Page* page, GeolocationClient& client)
{
    ASSERT(page);
    Supplement<Page>::provideTo(page, GeolocationController::supplementName(), makeUnique<GeolocationController>(*page, client));
}
    
} // namespace WebCore

#endif // ENABLE(GEOLOCATION)
