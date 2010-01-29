/*
 * Copyright (C) 2008, 2009 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Torch Mobile, Inc.
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

#include "Chrome.h"
#include "Document.h"
#include "Frame.h"
#include "Page.h"

#if ENABLE(CLIENT_BASED_GEOLOCATION)
#include "Coordinates.h"
#include "GeolocationController.h"
#include "GeolocationError.h"
#include "GeolocationPosition.h"
#include "PositionError.h"
#endif

namespace WebCore {

static const char permissionDeniedErrorMessage[] = "User denied Geolocation";

#if ENABLE(CLIENT_BASED_GEOLOCATION)

static PassRefPtr<Geoposition> createGeoposition(GeolocationPosition* position)
{
    if (!position)
        return 0;
    
    RefPtr<Coordinates> coordinates = Coordinates::create(position->latitude(), position->longitude(), position->canProvideAltitude(), position->altitude(), 
                                                          position->accuracy(), position->canProvideAltitudeAccuracy(), position->altitudeAccuracy(),
                                                          position->canProvideHeading(), position->heading(), position->canProvideSpeed(), position->speed());
    return Geoposition::create(coordinates.release(), position->timestamp());
}

static PassRefPtr<PositionError> createPositionError(GeolocationError* error)
{
    PositionError::ErrorCode code = PositionError::POSITION_UNAVAILABLE;
    switch (error->code()) {
    case GeolocationError::PermissionDenied:
        code = PositionError::PERMISSION_DENIED;
        break;
    case GeolocationError::PositionUnavailable:
        code = PositionError::POSITION_UNAVAILABLE;
        break;
    }

    return PositionError::create(code, error->message());
}
#endif

Geolocation::GeoNotifier::GeoNotifier(Geolocation* geolocation, PassRefPtr<PositionCallback> successCallback, PassRefPtr<PositionErrorCallback> errorCallback, PassRefPtr<PositionOptions> options)
    : m_geolocation(geolocation)
    , m_successCallback(successCallback)
    , m_errorCallback(errorCallback)
    , m_options(options)
    , m_timer(this, &Geolocation::GeoNotifier::timerFired)
{
    ASSERT(m_geolocation);
    ASSERT(m_successCallback);
    // If no options were supplied from JS, we should have created a default set
    // of options in JSGeolocationCustom.cpp.
    ASSERT(m_options);
}

void Geolocation::GeoNotifier::setFatalError(PassRefPtr<PositionError> error)
{
    // This method is called at most once on a given GeoNotifier object.
    ASSERT(!m_fatalError);
    m_fatalError = error;
    m_timer.startOneShot(0);
}

bool Geolocation::GeoNotifier::hasZeroTimeout() const
{
    return m_options->hasTimeout() && m_options->timeout() == 0;
}

void Geolocation::GeoNotifier::startTimerIfNeeded()
{
    if (m_options->hasTimeout())
        m_timer.startOneShot(m_options->timeout() / 1000.0);
}

void Geolocation::GeoNotifier::timerFired(Timer<GeoNotifier>*)
{
    m_timer.stop();

    // Protect this GeoNotifier object, since it
    // could be deleted by a call to clearWatch in a callback.
    RefPtr<GeoNotifier> protect(this);

    if (m_fatalError) {
        if (m_errorCallback)
            m_errorCallback->handleEvent(m_fatalError.get());
        // This will cause this notifier to be deleted.
        m_geolocation->fatalErrorOccurred(this);
        return;
    }

    if (m_errorCallback) {
        RefPtr<PositionError> error = PositionError::create(PositionError::TIMEOUT, "Timeout expired");
        m_errorCallback->handleEvent(error.get());
    }
    m_geolocation->requestTimedOut(this);
}

void Geolocation::Watchers::set(int id, PassRefPtr<GeoNotifier> prpNotifier)
{
    RefPtr<GeoNotifier> notifier = prpNotifier;

    m_idToNotifierMap.set(id, notifier.get());
    m_notifierToIdMap.set(notifier.release(), id);
}

void Geolocation::Watchers::remove(int id)
{
    IdToNotifierMap::iterator iter = m_idToNotifierMap.find(id);
    if (iter == m_idToNotifierMap.end())
        return;
    m_notifierToIdMap.remove(iter->second);
    m_idToNotifierMap.remove(iter);
}

void Geolocation::Watchers::remove(GeoNotifier* notifier)
{
    NotifierToIdMap::iterator iter = m_notifierToIdMap.find(notifier);
    if (iter == m_notifierToIdMap.end())
        return;
    m_idToNotifierMap.remove(iter->second);
    m_notifierToIdMap.remove(iter);
}

void Geolocation::Watchers::clear()
{
    m_idToNotifierMap.clear();
    m_notifierToIdMap.clear();
}

bool Geolocation::Watchers::isEmpty() const
{
    return m_idToNotifierMap.isEmpty();
}

void Geolocation::Watchers::getNotifiersVector(Vector<RefPtr<GeoNotifier> >& copy) const
{
    copyValuesToVector(m_idToNotifierMap, copy);
}

Geolocation::Geolocation(Frame* frame)
    : m_frame(frame)
#if !ENABLE(CLIENT_BASED_GEOLOCATION)
    , m_service(GeolocationService::create(this))
#endif
    , m_allowGeolocation(Unknown)
    , m_shouldClearCache(false)
{
    if (!m_frame)
        return;
    ASSERT(m_frame->document());
    m_frame->document()->setUsingGeolocation(true);
}

Geolocation::~Geolocation()
{
}

void Geolocation::disconnectFrame()
{
    stopUpdating();
    if (m_frame && m_frame->document())
        m_frame->document()->setUsingGeolocation(false);
    m_frame = 0;
}

Geoposition* Geolocation::lastPosition()
{
#if ENABLE(CLIENT_BASED_GEOLOCATION)
    if (!m_frame)
        return 0;

    Page* page = m_frame->page();
    if (!page)
        return 0;

    m_lastPosition = createGeoposition(page->geolocationController()->lastPosition());
#else
    m_lastPosition = m_service->lastPosition();
#endif

    return m_lastPosition.get();
}

void Geolocation::getCurrentPosition(PassRefPtr<PositionCallback> successCallback, PassRefPtr<PositionErrorCallback> errorCallback, PassRefPtr<PositionOptions> options)
{
    RefPtr<GeoNotifier> notifier = startRequest(successCallback, errorCallback, options);
    ASSERT(notifier);

    m_oneShots.add(notifier);
}

int Geolocation::watchPosition(PassRefPtr<PositionCallback> successCallback, PassRefPtr<PositionErrorCallback> errorCallback, PassRefPtr<PositionOptions> options)
{
    RefPtr<GeoNotifier> notifier = startRequest(successCallback, errorCallback, options);
    ASSERT(notifier);

    static int nextAvailableWatchId = 1;
    // In case of overflow, make sure the ID remains positive, but reuse the ID values.
    if (nextAvailableWatchId < 1)
        nextAvailableWatchId = 1;
    m_watchers.set(nextAvailableWatchId, notifier.release());
    return nextAvailableWatchId++;
}

PassRefPtr<Geolocation::GeoNotifier> Geolocation::startRequest(PassRefPtr<PositionCallback> successCallback, PassRefPtr<PositionErrorCallback> errorCallback, PassRefPtr<PositionOptions> options)
{
    RefPtr<GeoNotifier> notifier = GeoNotifier::create(this, successCallback, errorCallback, options);

    // Check whether permissions have already been denied. Note that if this is the case,
    // the permission state can not change again in the lifetime of this page.
    if (isDenied())
        notifier->setFatalError(PositionError::create(PositionError::PERMISSION_DENIED, permissionDeniedErrorMessage));
    else {
        if (notifier->hasZeroTimeout() || startUpdating(notifier.get()))
            notifier->startTimerIfNeeded();
        else
            notifier->setFatalError(PositionError::create(PositionError::POSITION_UNAVAILABLE, "Failed to start Geolocation service"));
    }

    return notifier.release();
}

void Geolocation::fatalErrorOccurred(Geolocation::GeoNotifier* notifier)
{
    // This request has failed fatally. Remove it from our lists.
    m_oneShots.remove(notifier);
    m_watchers.remove(notifier);

    if (!hasListeners())
        stopUpdating();
}

void Geolocation::requestTimedOut(GeoNotifier* notifier)
{
    // If this is a one-shot request, stop it.
    m_oneShots.remove(notifier);

    if (!hasListeners())
        stopUpdating();
}

void Geolocation::clearWatch(int watchId)
{
    m_watchers.remove(watchId);
    
    if (!hasListeners())
        stopUpdating();
}

void Geolocation::suspend()
{
#if !ENABLE(CLIENT_BASED_GEOLOCATION)
    if (hasListeners())
        m_service->suspend();
#endif
}

void Geolocation::resume()
{
#if !ENABLE(CLIENT_BASED_GEOLOCATION)
    if (hasListeners())
        m_service->resume();
#endif
}

void Geolocation::setIsAllowed(bool allowed)
{
    m_allowGeolocation = allowed ? Yes : No;
    
#if ENABLE(CLIENT_BASED_GEOLOCATION)
    if (m_startRequestPermissionNotifier) {
        if (isAllowed()) {
            // Permission request was made during the startUpdating process
            m_startRequestPermissionNotifier = 0;
            if (!m_frame)
                return;
            Page* page = m_frame->page();
            if (!page)
                return;
            page->geolocationController()->addObserver(this);
        } else {
            m_startRequestPermissionNotifier->setFatalError(PositionError::create(PositionError::PERMISSION_DENIED, permissionDeniedErrorMessage));
            m_oneShots.add(m_startRequestPermissionNotifier);
            m_startRequestPermissionNotifier = 0;
        }
        return;
    }
#endif
    
    if (isAllowed())
        makeSuccessCallbacks();
    else {
        RefPtr<PositionError> error = PositionError::create(PositionError::PERMISSION_DENIED, permissionDeniedErrorMessage);
        error->setIsFatal(true);
        handleError(error.get());
    }
}

void Geolocation::sendError(Vector<RefPtr<GeoNotifier> >& notifiers, PositionError* error)
{
     Vector<RefPtr<GeoNotifier> >::const_iterator end = notifiers.end();
     for (Vector<RefPtr<GeoNotifier> >::const_iterator it = notifiers.begin(); it != end; ++it) {
         RefPtr<GeoNotifier> notifier = *it;
         
         if (notifier->m_errorCallback)
             notifier->m_errorCallback->handleEvent(error);
     }
}

void Geolocation::sendPosition(Vector<RefPtr<GeoNotifier> >& notifiers, Geoposition* position)
{
    Vector<RefPtr<GeoNotifier> >::const_iterator end = notifiers.end();
    for (Vector<RefPtr<GeoNotifier> >::const_iterator it = notifiers.begin(); it != end; ++it) {
        RefPtr<GeoNotifier> notifier = *it;
        ASSERT(notifier->m_successCallback);
        
        notifier->m_successCallback->handleEvent(position);
    }
}

void Geolocation::stopTimer(Vector<RefPtr<GeoNotifier> >& notifiers)
{
    Vector<RefPtr<GeoNotifier> >::const_iterator end = notifiers.end();
    for (Vector<RefPtr<GeoNotifier> >::const_iterator it = notifiers.begin(); it != end; ++it) {
        RefPtr<GeoNotifier> notifier = *it;
        notifier->m_timer.stop();
    }
}

void Geolocation::stopTimersForOneShots()
{
    Vector<RefPtr<GeoNotifier> > copy;
    copyToVector(m_oneShots, copy);
    
    stopTimer(copy);
}

void Geolocation::stopTimersForWatchers()
{
    Vector<RefPtr<GeoNotifier> > copy;
    m_watchers.getNotifiersVector(copy);
    
    stopTimer(copy);
}

void Geolocation::stopTimers()
{
    stopTimersForOneShots();
    stopTimersForWatchers();
}

void Geolocation::handleError(PositionError* error)
{
    ASSERT(error);
    
    Vector<RefPtr<GeoNotifier> > oneShotsCopy;
    copyToVector(m_oneShots, oneShotsCopy);

    Vector<RefPtr<GeoNotifier> > watchersCopy;
    m_watchers.getNotifiersVector(watchersCopy);

    // Clear the lists before we make the callbacks, to avoid clearing notifiers
    // added by calls to Geolocation methods from the callbacks, and to prevent
    // further callbacks to these notifiers.
    m_oneShots.clear();
    if (error->isFatal())
        m_watchers.clear();

    sendError(oneShotsCopy, error);
    sendError(watchersCopy, error);

    if (!hasListeners())
        stopUpdating();
}

void Geolocation::requestPermission()
{
    if (m_allowGeolocation > Unknown)
        return;

    if (!m_frame)
        return;
    
    Page* page = m_frame->page();
    if (!page)
        return;
    
    m_allowGeolocation = InProgress;

    // Ask the chrome: it maintains the geolocation challenge policy itself.
    page->chrome()->requestGeolocationPermissionForFrame(m_frame, this);
}

void Geolocation::positionChanged(PassRefPtr<Geoposition> newPosition)
{
    m_currentPosition = newPosition;

    // Stop all currently running timers.
    stopTimers();
    
    if (!isAllowed()) {
        // requestPermission() will ask the chrome for permission. This may be
        // implemented synchronously or asynchronously. In both cases,
        // makeSuccessCallbacks() will be called if permission is granted, so
        // there's nothing more to do here.
        requestPermission();
        return;
    }

    makeSuccessCallbacks();
}

void Geolocation::makeSuccessCallbacks()
{
    ASSERT(m_currentPosition);
    ASSERT(isAllowed());
    
    Vector<RefPtr<GeoNotifier> > oneShotsCopy;
    copyToVector(m_oneShots, oneShotsCopy);
    
    Vector<RefPtr<GeoNotifier> > watchersCopy;
    m_watchers.getNotifiersVector(watchersCopy);
    
    // Clear the lists before we make the callbacks, to avoid clearing notifiers
    // added by calls to Geolocation methods from the callbacks, and to prevent
    // further callbacks to these notifiers.
    m_oneShots.clear();

    sendPosition(oneShotsCopy, m_currentPosition.get());
    sendPosition(watchersCopy, m_currentPosition.get());

    if (!hasListeners())
        stopUpdating();
}

#if ENABLE(CLIENT_BASED_GEOLOCATION)

void Geolocation::setPosition(GeolocationPosition* position)
{
    positionChanged(createGeoposition(position));
}

void Geolocation::setError(GeolocationError* error)
{
    RefPtr<PositionError> positionError = createPositionError(error);
    handleError(positionError.get());
}

#else

void Geolocation::geolocationServicePositionChanged(GeolocationService* service)
{
    ASSERT_UNUSED(service, service == m_service);
    ASSERT(m_service->lastPosition());

    positionChanged(m_service->lastPosition());
}

void Geolocation::geolocationServiceErrorOccurred(GeolocationService* service)
{
    ASSERT(service->lastError());

    handleError(service->lastError());
}

#endif

bool Geolocation::startUpdating(GeoNotifier* notifier)
{
#if ENABLE(CLIENT_BASED_GEOLOCATION)
    // FIXME: Pass options to client.

    if (!isAllowed()) {
        m_startRequestPermissionNotifier = notifier;
        requestPermission();
        return true;
    }
    
    if (!m_frame)
        return false;

    Page* page = m_frame->page();
    if (!page)
        return false;

    page->geolocationController()->addObserver(this);
    return true;
#else
    return m_service->startUpdating(notifier->options);
#endif
}

void Geolocation::stopUpdating()
{
#if ENABLE(CLIENT_BASED_GEOLOCATION)
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    page->geolocationController()->removeObserver(this);
#else
    m_service->stopUpdating();
#endif

}

} // namespace WebCore
