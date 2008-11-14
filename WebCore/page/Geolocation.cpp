/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
#include "Geolocation.h"

#include "Document.h"
#include "Frame.h"
#include "PositionError.h"

namespace WebCore {

Geolocation::GeoNotifier::GeoNotifier(PassRefPtr<PositionCallback> successCallback, PassRefPtr<PositionErrorCallback> errorCallback, PositionOptions* options)
    : m_successCallback(successCallback)
    , m_errorCallback(errorCallback)
    , m_timer(this, &Geolocation::GeoNotifier::timerFired)
{
    if (m_errorCallback && options)
        m_timer.startOneShot(options->timeout() / 1000.0);
}

void Geolocation::GeoNotifier::timerFired(Timer<GeoNotifier>*)
{
    ASSERT(m_errorCallback);
    
    m_timer.stop();

    RefPtr<PositionError> error = PositionError::create(PositionError::TIMEOUT, "Timed out");
    m_errorCallback->handleEvent(error.get());
}

Geolocation::Geolocation(Frame* frame)
    : m_frame(frame)
    , m_service(GeolocationService::create(this))
{
    ASSERT(m_frame->document());
    m_frame->document()->setUsingGeolocation(true);
}

void Geolocation::disconnectFrame()
{
    m_service->stopUpdating();
    if (m_frame->document())
        m_frame->document()->setUsingGeolocation(false);
    m_frame = 0;
}

void Geolocation::getCurrentPosition(PassRefPtr<PositionCallback> successCallback, PassRefPtr<PositionErrorCallback> errorCallback, PositionOptions* options)
{
    RefPtr<GeoNotifier> notifier = GeoNotifier::create(successCallback, errorCallback, options);

    if (!m_service->startUpdating(options)) {
        if (notifier->m_errorCallback) {
            RefPtr<PositionError> error = PositionError::create(PositionError::PERMISSION_DENIED, "Unable to Start");
            notifier->m_errorCallback->handleEvent(error.get());
        }
        return;
    }

    m_oneShots.add(notifier);
}

int Geolocation::watchPosition(PassRefPtr<PositionCallback> successCallback, PassRefPtr<PositionErrorCallback> errorCallback, PositionOptions* options)
{
    RefPtr<GeoNotifier> notifier = GeoNotifier::create(successCallback, errorCallback, options);

    if (!m_service->startUpdating(options)) {
        if (notifier->m_errorCallback) {
            RefPtr<PositionError> error = PositionError::create(PositionError::PERMISSION_DENIED, "Unable to Start");
            notifier->m_errorCallback->handleEvent(error.get());
        }
        return 0;
    }
    
    static int sIdentifier = 0;
    
    m_watchers.set(++sIdentifier, notifier);

    return sIdentifier;
}

void Geolocation::clearWatch(int watchId)
{
    m_watchers.remove(watchId);
    
    if (!hasListeners())
        m_service->stopUpdating();
}

void Geolocation::suspend()
{
    if (hasListeners())
        m_service->suspend();
}

void Geolocation::resume()
{
    if (hasListeners())
        m_service->resume();
}

void Geolocation::sendErrorToOneShots(PositionError* error)
{
    Vector<RefPtr<GeoNotifier> > copy;
    copyToVector(m_oneShots, copy);

    Vector<RefPtr<GeoNotifier> >::const_iterator end = copy.end();
    for (Vector<RefPtr<GeoNotifier> >::const_iterator it = copy.begin(); it != end; ++it) {
        RefPtr<GeoNotifier> notifier = *it;
        
        if (notifier->m_errorCallback)
            notifier->m_errorCallback->handleEvent(error);
    }
}

void Geolocation::sendErrorToWatchers(PositionError* error)
{
    Vector<RefPtr<GeoNotifier> > copy;
    copyValuesToVector(m_watchers, copy);

    Vector<RefPtr<GeoNotifier> >::const_iterator end = copy.end();
    for (Vector<RefPtr<GeoNotifier> >::const_iterator it = copy.begin(); it != end; ++it) {
        RefPtr<GeoNotifier> notifier = *it;
        
        if (notifier->m_errorCallback)
            notifier->m_errorCallback->handleEvent(error);
    }
}

void Geolocation::sendPositionToOneShots(Geoposition* position)
{
    Vector<RefPtr<GeoNotifier> > copy;
    copyToVector(m_oneShots, copy);
    
    Vector<RefPtr<GeoNotifier> >::const_iterator end = copy.end();
    for (Vector<RefPtr<GeoNotifier> >::const_iterator it = copy.begin(); it != end; ++it) {
        RefPtr<GeoNotifier> notifier = *it;
        ASSERT(notifier->m_successCallback);
        
        notifier->m_timer.stop();
        bool shouldCallErrorCallback = false;
        notifier->m_successCallback->handleEvent(position, shouldCallErrorCallback);
        if (shouldCallErrorCallback) {
            RefPtr<PositionError> error = PositionError::create(PositionError::UNKNOWN_ERROR, "An exception was thrown");
            handleError(error.get());
        }
    }
}

void Geolocation::sendPositionToWatchers(Geoposition* position)
{
    Vector<RefPtr<GeoNotifier> > copy;
    copyValuesToVector(m_watchers, copy);
    
    Vector<RefPtr<GeoNotifier> >::const_iterator end = copy.end();
    for (Vector<RefPtr<GeoNotifier> >::const_iterator it = copy.begin(); it != end; ++it) {
        RefPtr<GeoNotifier> notifier = *it;
        ASSERT(notifier->m_successCallback);
        
        notifier->m_timer.stop();
        bool shouldCallErrorCallback = false;
        notifier->m_successCallback->handleEvent(position, shouldCallErrorCallback);
        if (shouldCallErrorCallback) {
            RefPtr<PositionError> error = PositionError::create(PositionError::UNKNOWN_ERROR, "An exception was thrown");
            handleError(error.get());
        }
    }
}

void Geolocation::handleError(PositionError* error)
{
    ASSERT(error);
    
    sendErrorToOneShots(error);    
    sendErrorToWatchers(error);

    m_oneShots.clear();
}

void Geolocation::geolocationServicePositionChanged(GeolocationService* service)
{
    ASSERT(service->lastPosition());
    
    sendPositionToOneShots(service->lastPosition());
    sendPositionToWatchers(service->lastPosition());
        
    m_oneShots.clear();

    if (!hasListeners())
        m_service->stopUpdating();
}

void Geolocation::geolocationServiceErrorOccurred(GeolocationService* service)
{
    ASSERT(service->lastError());
    
    handleError(service->lastError());
}

} // namespace WebCore
