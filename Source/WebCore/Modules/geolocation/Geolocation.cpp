/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Torch Mobile, Inc.
 * Copyright 2010, The Android Open Source Project
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

#if ENABLE(GEOLOCATION)

#include "Document.h"
#include "EventLoop.h"
#include "FeaturePolicy.h"
#include "GeoNotifier.h"
#include "GeolocationController.h"
#include "GeolocationCoordinates.h"
#include "GeolocationError.h"
#include "GeolocationPosition.h"
#include "GeolocationPositionData.h"
#include "LocalFrame.h"
#include "Navigator.h"
#include "Page.h"
#include "RuntimeApplicationChecks.h"
#include "SecurityOrigin.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Ref.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static const ASCIILiteral permissionDeniedErrorMessage { "User denied Geolocation"_s };
static const ASCIILiteral failedToStartServiceErrorMessage { "Failed to start Geolocation service"_s };
static const ASCIILiteral framelessDocumentErrorMessage { "Geolocation cannot be used in frameless documents"_s };
static const ASCIILiteral originCannotRequestGeolocationErrorMessage { "Origin does not have permission to use Geolocation service"_s };

WTF_MAKE_ISO_ALLOCATED_IMPL(Geolocation);

static RefPtr<GeolocationPosition> createGeolocationPosition(std::optional<GeolocationPositionData>&& position)
{
    if (!position)
        return nullptr;
    
    EpochTimeStamp timestamp = convertSecondsToEpochTimeStamp(position->timestamp);
    return GeolocationPosition::create(GeolocationCoordinates::create(WTFMove(position.value())), timestamp);
}

static Ref<GeolocationPositionError> createGeolocationPositionError(GeolocationError& error)
{
    auto code = GeolocationPositionError::POSITION_UNAVAILABLE;
    switch (error.code()) {
    case GeolocationError::PermissionDenied:
        code = GeolocationPositionError::PERMISSION_DENIED;
        break;
    case GeolocationError::PositionUnavailable:
        code = GeolocationPositionError::POSITION_UNAVAILABLE;
        break;
    }

    return GeolocationPositionError::create(code, error.message());
}

bool Geolocation::Watchers::add(int id, RefPtr<GeoNotifier>&& notifier)
{
    ASSERT(id > 0);

    if (!m_idToNotifierMap.add(id, notifier.get()).isNewEntry)
        return false;
    m_notifierToIdMap.set(WTFMove(notifier), id);
    return true;
}

GeoNotifier* Geolocation::Watchers::find(int id)
{
    ASSERT(id > 0);
    return m_idToNotifierMap.get(id);
}

void Geolocation::Watchers::remove(int id)
{
    ASSERT(id > 0);
    if (auto notifier = m_idToNotifierMap.take(id))
        m_notifierToIdMap.remove(notifier);
}

void Geolocation::Watchers::remove(GeoNotifier* notifier)
{
    if (auto identifier = m_notifierToIdMap.take(notifier))
        m_idToNotifierMap.remove(identifier);
}

bool Geolocation::Watchers::contains(GeoNotifier* notifier) const
{
    return m_notifierToIdMap.contains(notifier);
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

void Geolocation::Watchers::getNotifiersVector(GeoNotifierVector& copy) const
{
    copy = copyToVector(m_idToNotifierMap.values());
}

Ref<Geolocation> Geolocation::create(Navigator& navigator)
{
    auto geolocation = adoptRef(*new Geolocation(navigator));
    geolocation->suspendIfNeeded();
    return geolocation;
}

Geolocation::Geolocation(Navigator& navigator)
    : ActiveDOMObject(navigator.scriptExecutionContext())
    , m_navigator(navigator)
    , m_resumeTimer(*this, &Geolocation::resumeTimerFired)
{
}

Geolocation::~Geolocation()
{
    ASSERT(m_allowGeolocation != InProgress);
    revokeAuthorizationTokenIfNecessary();
}

SecurityOrigin* Geolocation::securityOrigin() const
{
    return scriptExecutionContext()->securityOrigin();
}

Page* Geolocation::page() const
{
    return document() ? document()->page() : nullptr;
}
    
void Geolocation::suspend(ReasonForSuspension reason)
{
    if (reason == ReasonForSuspension::BackForwardCache) {
        stop();
        m_resetOnResume = true;
    }

    // Suspend GeoNotifier timeout timers.
    if (hasListeners())
        stopTimers();

    m_isSuspended = true;
    m_resumeTimer.stop();
    ActiveDOMObject::suspend(reason);
}

void Geolocation::resume()
{
#if USE(WEB_THREAD)
    ASSERT(WebThreadIsLockedOrDisabled());
#endif
    ActiveDOMObject::resume();

    if (!m_resumeTimer.isActive())
        m_resumeTimer.startOneShot(0_s);
}

void Geolocation::resumeTimerFired()
{
    m_isSuspended = false;

    if (m_resetOnResume) {
        resetAllGeolocationPermission();
        m_resetOnResume = false;
    }

    // Resume GeoNotifier timeout timers.
    if (hasListeners()) {
        for (auto& notifier : m_oneShots)
            notifier->startTimerIfNeeded();
        GeoNotifierVector watcherCopy;
        m_watchers.getNotifiersVector(watcherCopy);
        for (auto& watcher : watcherCopy)
            watcher->startTimerIfNeeded();
    }

    if ((isAllowed() || isDenied()) && !m_pendingForPermissionNotifiers.isEmpty()) {
        // The pending permission was granted while the object was suspended.
        setIsAllowed(isAllowed(), authorizationToken());
        ASSERT(!m_hasChangedPosition);
        ASSERT(!m_errorWaitingForResume);
        return;
    }

    if (isDenied() && hasListeners()) {
        // The permission was revoked while the object was suspended.
        setIsAllowed(false, { });
        return;
    }

    if (m_hasChangedPosition) {
        positionChanged();
        m_hasChangedPosition = false;
    }

    if (m_errorWaitingForResume) {
        handleError(*m_errorWaitingForResume);
        m_errorWaitingForResume = nullptr;
    }
}

void Geolocation::resetAllGeolocationPermission()
{
    if (m_isSuspended) {
        m_resetOnResume = true;
        return;
    }

    if (m_allowGeolocation == InProgress) {
        Page* page = this->page();
        if (page)
            GeolocationController::from(page)->cancelPermissionRequest(*this);

        // This return is not technically correct as GeolocationController::cancelPermissionRequest() should have cleared the active request.
        // Neither iOS nor OS X supports cancelPermissionRequest() (https://bugs.webkit.org/show_bug.cgi?id=89524), so we workaround that and let ongoing requests complete. :(
        return;
    }

    // 1) Reset our own state.
    stopUpdating();
    resetIsAllowed();
    m_hasChangedPosition = false;
    m_errorWaitingForResume = nullptr;

    // 2) Request new permission for the active notifiers.
    stopTimers();

    // Go over the one shot and re-request permission.
    for (auto& notifier : m_oneShots)
        startRequest(notifier.get());
    // Go over the watchers and re-request permission.
    GeoNotifierVector watcherCopy;
    m_watchers.getNotifiersVector(watcherCopy);
    for (auto& watcher : watcherCopy)
        startRequest(watcher.get());
}

void Geolocation::stop()
{
    Page* page = this->page();
    if (page && m_allowGeolocation == InProgress)
        GeolocationController::from(page)->cancelPermissionRequest(*this);
    // The frame may be moving to a new page and we want to get the permissions from the new page's client.
    resetIsAllowed();
    cancelAllRequests();
    stopUpdating();
    m_hasChangedPosition = false;
    m_errorWaitingForResume = nullptr;
    m_pendingForPermissionNotifiers.clear();
}

const char* Geolocation::activeDOMObjectName() const
{
    return "Geolocation";
}

GeolocationPosition* Geolocation::lastPosition()
{
    Page* page = this->page();
    if (!page)
        return nullptr;

    m_lastPosition = createGeolocationPosition(GeolocationController::from(page)->lastPosition());

    return m_lastPosition.get();
}

void Geolocation::getCurrentPosition(Ref<PositionCallback>&& successCallback, RefPtr<PositionErrorCallback>&& errorCallback, PositionOptions&& options)
{
    if (!document() || !document()->isFullyActive()) {
        if (errorCallback && errorCallback->scriptExecutionContext()) {
            errorCallback->scriptExecutionContext()->eventLoop().queueTask(TaskSource::Geolocation, [errorCallback] {
                errorCallback->handleEvent(GeolocationPositionError::create(GeolocationPositionError::POSITION_UNAVAILABLE, "Document is not fully active"_s));
            });
        }
        return;
    }

    auto notifier = GeoNotifier::create(*this, WTFMove(successCallback), WTFMove(errorCallback), WTFMove(options));
    startRequest(notifier.ptr());

    m_oneShots.add(WTFMove(notifier));
}

int Geolocation::watchPosition(Ref<PositionCallback>&& successCallback, RefPtr<PositionErrorCallback>&& errorCallback, PositionOptions&& options)
{
    if (!document() || !document()->isFullyActive()) {
        if (errorCallback && errorCallback->scriptExecutionContext()) {
            errorCallback->scriptExecutionContext()->eventLoop().queueTask(TaskSource::Geolocation, [errorCallback] {
                errorCallback->handleEvent(GeolocationPositionError::create(GeolocationPositionError::POSITION_UNAVAILABLE, "Document is not fully active"_s));
            });
        }
        return 0;
    }

    auto notifier = GeoNotifier::create(*this, WTFMove(successCallback), WTFMove(errorCallback), WTFMove(options));
    startRequest(notifier.ptr());

    int watchID;
    // Keep asking for the next id until we're given one that we don't already have.
    do {
        watchID = scriptExecutionContext()->circularSequentialID();
    } while (!m_watchers.add(watchID, notifier.copyRef()));
    return watchID;
}

static void logError(const String& target, const bool isSecure, const bool isMixedContent, Document* document)
{
    StringBuilder message;
    message.append("[blocked] Access to geolocation was blocked over");
    
    if (!isSecure)
        message.append(" insecure connection to ");
    else if (isMixedContent)
        message.append(" secure connection with mixed content to ");
    else
        return;
    
    message.append(target);
    message.append(".\n");
    document->addConsoleMessage(MessageSource::Security, MessageLevel::Error, message.toString());
}
    
bool Geolocation::shouldBlockGeolocationRequests()
{
    if (!isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type::Geolocation, *document(), LogFeaturePolicyFailure::Yes))
        return true;

    bool isSecure = SecurityOrigin::isSecure(document()->url()) || document()->isSecureContext();
    bool hasMixedContent = !document()->foundMixedContent().isEmpty();
    bool isLocalOrigin = securityOrigin()->isLocal();
    if (document()->canAccessResource(ScriptExecutionContext::ResourceType::Geolocation) != ScriptExecutionContext::HasResourceAccess::No) {
        if (isLocalOrigin || (isSecure && !hasMixedContent))
            return false;
    }

    logError(securityOrigin()->toString(), isSecure, hasMixedContent, document());
    return true;
}

void Geolocation::startRequest(GeoNotifier* notifier)
{
    if (shouldBlockGeolocationRequests()) {
        notifier->setFatalError(GeolocationPositionError::create(GeolocationPositionError::PERMISSION_DENIED, originCannotRequestGeolocationErrorMessage));
        return;
    }
    document()->setGeolocationAccessed();

    // Check whether permissions have already been denied. Note that if this is the case,
    // the permission state can not change again in the lifetime of this page.
    if (isDenied())
        notifier->setFatalError(GeolocationPositionError::create(GeolocationPositionError::PERMISSION_DENIED, permissionDeniedErrorMessage));
    else if (haveSuitableCachedPosition(notifier->options()))
        notifier->setUseCachedPosition();
    else if (notifier->hasZeroTimeout())
        notifier->startTimerIfNeeded();
    else if (!isAllowed()) {
        // if we don't yet have permission, request for permission before calling startUpdating()
        m_pendingForPermissionNotifiers.add(notifier);
        requestPermission();
    } else if (startUpdating(notifier))
        notifier->startTimerIfNeeded();
    else
        notifier->setFatalError(GeolocationPositionError::create(GeolocationPositionError::POSITION_UNAVAILABLE, failedToStartServiceErrorMessage));
}

void Geolocation::fatalErrorOccurred(GeoNotifier* notifier)
{
    // This request has failed fatally. Remove it from our lists.
    m_oneShots.remove(notifier);
    m_watchers.remove(notifier);

    if (!hasListeners())
        stopUpdating();
}

void Geolocation::requestUsesCachedPosition(GeoNotifier* notifier)
{
    // This is called asynchronously, so the permissions could have been denied
    // since we last checked in startRequest.
    if (isDenied()) {
        notifier->setFatalError(GeolocationPositionError::create(GeolocationPositionError::PERMISSION_DENIED, permissionDeniedErrorMessage));
        return;
    }

    m_requestsAwaitingCachedPosition.add(notifier);

    // If permissions are allowed, make the callback
    if (isAllowed()) {
        makeCachedPositionCallbacks();
        return;
    }

    // Request permissions, which may be synchronous or asynchronous.
    requestPermission();
}

void Geolocation::makeCachedPositionCallbacks()
{
    // All modifications to m_requestsAwaitingCachedPosition are done
    // asynchronously, so we don't need to worry about it being modified from
    // the callbacks.
    for (auto& notifier : m_requestsAwaitingCachedPosition) {
        // FIXME: This seems wrong, since makeCachedPositionCallbacks() is called in a branch where
        // lastPosition() is known to be null in Geolocation::setIsAllowed().
        notifier->runSuccessCallback(lastPosition());

        // If this is a one-shot request, stop it. Otherwise, if the watch still
        // exists, start the service to get updates.
        if (!m_oneShots.remove(notifier.get()) && m_watchers.contains(notifier.get())) {
            if (notifier->hasZeroTimeout() || startUpdating(notifier.get()))
                notifier->startTimerIfNeeded();
            else
                notifier->setFatalError(GeolocationPositionError::create(GeolocationPositionError::POSITION_UNAVAILABLE, failedToStartServiceErrorMessage));
        }
    }

    m_requestsAwaitingCachedPosition.clear();

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

bool Geolocation::haveSuitableCachedPosition(const PositionOptions& options)
{
    auto* cachedPosition = lastPosition();
    if (!cachedPosition)
        return false;
    if (!options.maximumAge)
        return false;
    EpochTimeStamp currentTimeMillis = convertSecondsToEpochTimeStamp(WallTime::now().secondsSinceEpoch());
    return cachedPosition->timestamp() > currentTimeMillis - options.maximumAge;
}

void Geolocation::clearWatch(int watchID)
{
    if (watchID <= 0)
        return;

    if (GeoNotifier* notifier = m_watchers.find(watchID))
        m_pendingForPermissionNotifiers.remove(notifier);
    m_watchers.remove(watchID);
    
    if (!hasListeners())
        stopUpdating();
}

void Geolocation::setIsAllowed(bool allowed, const String& authorizationToken)
{
    // Protect the Geolocation object from garbage collection during a callback.
    Ref<Geolocation> protectedThis(*this);

    // This may be due to either a new position from the service, or a cached
    // position.
    m_allowGeolocation = allowed ? Yes : No;
    m_authorizationToken = authorizationToken;
    
    if (m_isSuspended)
        return;

    // Permission request was made during the startRequest process
    if (!m_pendingForPermissionNotifiers.isEmpty()) {
        handlePendingPermissionNotifiers();
        m_pendingForPermissionNotifiers.clear();
        return;
    }

    if (!isAllowed()) {
        auto error = GeolocationPositionError::create(GeolocationPositionError::PERMISSION_DENIED, permissionDeniedErrorMessage);
        error->setIsFatal(true);
        handleError(error);
        m_requestsAwaitingCachedPosition.clear();
        m_hasChangedPosition = false;
        m_errorWaitingForResume = nullptr;
        return;
    }

    // If the service has a last position, use it to call back for all requests.
    // If any of the requests are waiting for permission for a cached position,
    // the position from the service will be at least as fresh.
    if (RefPtr<GeolocationPosition> position = lastPosition())
        makeSuccessCallbacks(*position);
    else
        makeCachedPositionCallbacks();
}

void Geolocation::sendError(GeoNotifierVector& notifiers, GeolocationPositionError& error)
{
    for (auto& notifier : notifiers)
        notifier->runErrorCallback(error);
}

void Geolocation::sendPosition(GeoNotifierVector& notifiers, GeolocationPosition& position)
{
    for (auto& notifier : notifiers)
        notifier->runSuccessCallback(&position);
}

void Geolocation::stopTimer(GeoNotifierVector& notifiers)
{
    for (auto& notifier : notifiers)
        notifier->stopTimer();
}

void Geolocation::stopTimersForOneShots()
{
    auto copy = copyToVector(m_oneShots);
    stopTimer(copy);
}

void Geolocation::stopTimersForWatchers()
{
    GeoNotifierVector copy;
    m_watchers.getNotifiersVector(copy);
    
    stopTimer(copy);
}

void Geolocation::stopTimers()
{
    stopTimersForOneShots();
    stopTimersForWatchers();
}

void Geolocation::cancelRequests(GeoNotifierVector& notifiers)
{
    for (auto& notifier : notifiers)
        notifier->setFatalError(GeolocationPositionError::create(GeolocationPositionError::POSITION_UNAVAILABLE, framelessDocumentErrorMessage));
}

void Geolocation::cancelAllRequests()
{
    auto copy = copyToVector(m_oneShots);
    cancelRequests(copy);
    m_watchers.getNotifiersVector(copy);
    cancelRequests(copy);
}

void Geolocation::extractNotifiersWithCachedPosition(GeoNotifierVector& notifiers, GeoNotifierVector* cached)
{
    GeoNotifierVector nonCached;
    for (auto& notifier : notifiers) {
        if (notifier->useCachedPosition()) {
            if (cached)
                cached->append(notifier.get());
        } else
            nonCached.append(notifier.get());
    }
    notifiers.swap(nonCached);
}

void Geolocation::copyToSet(const GeoNotifierVector& src, GeoNotifierSet& dest)
{
    for (auto& notifier : src)
        dest.add(notifier.get());
}

void Geolocation::handleError(GeolocationPositionError& error)
{
    auto oneShotsCopy = copyToVector(m_oneShots);

    GeoNotifierVector watchersCopy;
    m_watchers.getNotifiersVector(watchersCopy);

    // Clear the lists before we make the callbacks, to avoid clearing notifiers
    // added by calls to Geolocation methods from the callbacks, and to prevent
    // further callbacks to these notifiers.
    GeoNotifierVector oneShotsWithCachedPosition;
    m_oneShots.clear();
    if (error.isFatal())
        m_watchers.clear();
    else {
        // Don't send non-fatal errors to notifiers due to receive a cached position.
        extractNotifiersWithCachedPosition(oneShotsCopy, &oneShotsWithCachedPosition);
        extractNotifiersWithCachedPosition(watchersCopy, 0);
    }

    sendError(oneShotsCopy, error);
    sendError(watchersCopy, error);

    // hasListeners() doesn't distinguish between notifiers due to receive a
    // cached position and those requiring a fresh position. Perform the check
    // before restoring the notifiers below.
    if (!hasListeners())
        stopUpdating();

    // Maintain a reference to the cached notifiers until their timer fires.
    copyToSet(oneShotsWithCachedPosition, m_oneShots);
}

void Geolocation::requestPermission()
{
    if (m_allowGeolocation > Unknown)
        return;

    Page* page = this->page();
    if (!page)
        return;

    m_allowGeolocation = InProgress;

    // Ask the embedder: it maintains the geolocation challenge policy itself.
    GeolocationController::from(page)->requestPermission(*this);
}

void Geolocation::revokeAuthorizationTokenIfNecessary()
{
    if (m_authorizationToken.isNull())
        return;

    Page* page = this->page();
    if (!page)
        return;

    GeolocationController::from(page)->revokeAuthorizationToken(std::exchange(m_authorizationToken, String()));
}

void Geolocation::resetIsAllowed()
{
    m_allowGeolocation = Unknown;
    revokeAuthorizationTokenIfNecessary();
}

void Geolocation::makeSuccessCallbacks(GeolocationPosition& position)
{
    ASSERT(lastPosition());
    ASSERT(isAllowed());
    
    auto oneShotsCopy = copyToVector(m_oneShots);
    
    GeoNotifierVector watchersCopy;
    m_watchers.getNotifiersVector(watchersCopy);
    
    // Clear the lists before we make the callbacks, to avoid clearing notifiers
    // added by calls to Geolocation methods from the callbacks, and to prevent
    // further callbacks to these notifiers.
    m_oneShots.clear();

    sendPosition(oneShotsCopy, position);
    sendPosition(watchersCopy, position);

    if (!hasListeners())
        stopUpdating();
}

void Geolocation::positionChanged()
{
    ASSERT(isAllowed());

    // Stop all currently running timers.
    stopTimers();

    if (m_isSuspended) {
        m_hasChangedPosition = true;
        return;
    }

    if (RefPtr position = lastPosition())
        makeSuccessCallbacks(*position);
}

void Geolocation::setError(GeolocationError& error)
{
    if (m_isSuspended) {
        m_errorWaitingForResume = createGeolocationPositionError(error);
        return;
    }

    auto positionError = createGeolocationPositionError(error);
    handleError(positionError);
}

bool Geolocation::startUpdating(GeoNotifier* notifier)
{
    Page* page = this->page();
    if (!page)
        return false;

    GeolocationController::from(page)->addObserver(*this, notifier->options().enableHighAccuracy);
    return true;
}

void Geolocation::stopUpdating()
{
    Page* page = this->page();
    if (!page)
        return;

    GeolocationController::from(page)->removeObserver(*this);
}

void Geolocation::handlePendingPermissionNotifiers()
{
    // While we iterate through the list, we need not worry about list being modified as the permission 
    // is already set to Yes/No and no new listeners will be added to the pending list
    for (auto& notifier : m_pendingForPermissionNotifiers) {
        if (isAllowed()) {
            // start all pending notification requests as permission granted.
            // The notifier is always ref'ed by m_oneShots or m_watchers.
            if (startUpdating(notifier.get()))
                notifier->startTimerIfNeeded();
            else
                notifier->setFatalError(GeolocationPositionError::create(GeolocationPositionError::POSITION_UNAVAILABLE, failedToStartServiceErrorMessage));
        } else
            notifier->setFatalError(GeolocationPositionError::create(GeolocationPositionError::PERMISSION_DENIED, permissionDeniedErrorMessage));
    }
}

Navigator* Geolocation::navigator()
{
    return m_navigator.get();
}

LocalFrame* Geolocation::frame() const
{
    return m_navigator ? m_navigator->frame() : nullptr;
}

} // namespace WebCore
                                                        
#endif // ENABLE(GEOLOCATION)
