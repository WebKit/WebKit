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

#if ENABLE(CLIENT_BASED_GEOLOCATION)

#include "GeolocationControllerClient.h"

namespace WebCore {

GeolocationController::GeolocationController(Page* page, GeolocationControllerClient* client)
    : m_page(page)
    , m_client(client)
{
}

GeolocationController::~GeolocationController()
{
    if (m_client)
        m_client->geolocationDestroyed();
}

void GeolocationController::addObserver(Geolocation* observer)
{
    ASSERT(!m_observers.contains(observer));

    bool wasEmpty = m_observers.isEmpty();
    m_observers.add(observer);
    if (wasEmpty && m_client)
        m_client->startUpdating();
}

void GeolocationController::removeObserver(Geolocation* observer)
{
    if (!m_observers.contains(observer))
        return;

    m_observers.remove(observer);
    if (m_observers.isEmpty() && m_client)
        m_client->stopUpdating();
}

void GeolocationController::positionChanged(GeolocationPosition* position)
{
    Vector<RefPtr<Geolocation> > observersVector;
    copyToVector(m_observers, observersVector);
    for (size_t i = 0; i < observersVector.size(); ++i)
        observersVector[i]->setPosition(position);
}

void GeolocationController::errorOccurred(GeolocationError* error)
{
    Vector<RefPtr<Geolocation> > observersVector;
    copyToVector(m_observers, observersVector);
    for (size_t i = 0; i < observersVector.size(); ++i)
        observersVector[i]->setError(error);
}

GeolocationPosition* GeolocationController::lastPosition()
{
    if (!m_client)
        return 0;

    return m_client->lastPosition();
}

} // namespace WebCore

#endif // ENABLE(CLIENT_BASED_GEOLOCATION)
