/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "ServiceWorkerRegistration.h"

#if ENABLE(SERVICE_WORKER)
#include "DOMWindow.h"
#include "Document.h"
#include "Event.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "JSDOMPromiseDeferred.h"
#include "JSNotification.h"
#include "Logging.h"
#include "NavigationPreloadManager.h"
#include "NotificationClient.h"
#include "NotificationPermission.h"
#include "ServiceWorker.h"
#include "ServiceWorkerContainer.h"
#include "ServiceWorkerTypes.h"
#include "WorkerGlobalScope.h"
#include <wtf/IsoMallocInlines.h>

#define REGISTRATION_RELEASE_LOG(fmt, ...) RELEASE_LOG(ServiceWorker, "%p - ServiceWorkerRegistration::" fmt, this, ##__VA_ARGS__)
#define REGISTRATION_RELEASE_LOG_ERROR(fmt, ...) RELEASE_LOG_ERROR(ServiceWorker, "%p - ServiceWorkerRegistration::" fmt, this, ##__VA_ARGS__)

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ServiceWorkerRegistration);

Ref<ServiceWorkerRegistration> ServiceWorkerRegistration::getOrCreate(ScriptExecutionContext& context, Ref<ServiceWorkerContainer>&& container, ServiceWorkerRegistrationData&& data)
{
    if (auto* registration = container->registration(data.identifier)) {
        ASSERT(!registration->isContextStopped());
        return *registration;
    }

    auto registration = adoptRef(*new ServiceWorkerRegistration(context, WTFMove(container), WTFMove(data)));
    registration->suspendIfNeeded();
    return registration;
}

ServiceWorkerRegistration::ServiceWorkerRegistration(ScriptExecutionContext& context, Ref<ServiceWorkerContainer>&& container, ServiceWorkerRegistrationData&& registrationData)
    : ActiveDOMObject(&context)
    , m_registrationData(WTFMove(registrationData))
    , m_container(WTFMove(container))
{
    LOG(ServiceWorker, "Creating registration %p for registration key %s", this, m_registrationData.key.loggingString().utf8().data());

    if (m_registrationData.installingWorker)
        m_installingWorker = ServiceWorker::getOrCreate(context, WTFMove(*m_registrationData.installingWorker));
    if (m_registrationData.waitingWorker)
        m_waitingWorker = ServiceWorker::getOrCreate(context, WTFMove(*m_registrationData.waitingWorker));
    if (m_registrationData.activeWorker)
        m_activeWorker = ServiceWorker::getOrCreate(context, WTFMove(*m_registrationData.activeWorker));

    REGISTRATION_RELEASE_LOG("ServiceWorkerRegistration: ID %llu, installing=%llu, waiting=%llu, active=%llu", identifier().toUInt64(), m_installingWorker ? m_installingWorker->identifier().toUInt64() : 0, m_waitingWorker ? m_waitingWorker->identifier().toUInt64() : 0, m_activeWorker ? m_activeWorker->identifier().toUInt64() : 0);

    m_container->addRegistration(*this);

    relaxAdoptionRequirement();
}

ServiceWorkerRegistration::~ServiceWorkerRegistration()
{
    LOG(ServiceWorker, "Deleting registration %p for registration key %s", this, m_registrationData.key.loggingString().utf8().data());

    m_container->removeRegistration(*this);
}

ServiceWorker* ServiceWorkerRegistration::installing()
{
    return m_installingWorker.get();
}

ServiceWorker* ServiceWorkerRegistration::waiting()
{
    return m_waitingWorker.get();
}

ServiceWorker* ServiceWorkerRegistration::active()
{
    return m_activeWorker.get();
}

ServiceWorker* ServiceWorkerRegistration::getNewestWorker() const
{
    if (m_installingWorker)
        return m_installingWorker.get();
    if (m_waitingWorker)
        return m_waitingWorker.get();

    return m_activeWorker.get();
}

const String& ServiceWorkerRegistration::scope() const
{
    return m_registrationData.scopeURL.string();
}

ServiceWorkerUpdateViaCache ServiceWorkerRegistration::updateViaCache() const
{
    return m_registrationData.updateViaCache;
}

WallTime ServiceWorkerRegistration::lastUpdateTime() const
{
    return m_registrationData.lastUpdateTime;
}

void ServiceWorkerRegistration::setLastUpdateTime(WallTime lastUpdateTime)
{
    m_registrationData.lastUpdateTime = lastUpdateTime;
}

void ServiceWorkerRegistration::setUpdateViaCache(ServiceWorkerUpdateViaCache updateViaCache)
{
    m_registrationData.updateViaCache = updateViaCache;
}

void ServiceWorkerRegistration::update(Ref<DeferredPromise>&& promise)
{
    if (isContextStopped()) {
        promise->reject(Exception(InvalidStateError));
        return;
    }

    auto* newestWorker = getNewestWorker();
    if (!newestWorker) {
        promise->reject(Exception(InvalidStateError, "newestWorker is null"_s));
        return;
    }

    m_container->updateRegistration(m_registrationData.scopeURL, newestWorker->scriptURL(), newestWorker->workerType(), WTFMove(promise));
}

void ServiceWorkerRegistration::unregister(Ref<DeferredPromise>&& promise)
{
    if (isContextStopped()) {
        promise->reject(Exception(InvalidStateError));
        return;
    }

    m_container->unregisterRegistration(identifier(), WTFMove(promise));
}

void ServiceWorkerRegistration::subscribeToPushService(const Vector<uint8_t>& applicationServerKey, DOMPromiseDeferred<IDLInterface<PushSubscription>>&& promise)
{
    if (isContextStopped()) {
        promise.reject(Exception(InvalidStateError));
        return;
    }

    m_container->subscribeToPushService(*this, applicationServerKey, WTFMove(promise));
}

void ServiceWorkerRegistration::unsubscribeFromPushService(PushSubscriptionIdentifier subscriptionIdentifier, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    if (isContextStopped()) {
        promise.reject(Exception(InvalidStateError));
        return;
    }

    m_container->unsubscribeFromPushService(identifier(), subscriptionIdentifier, WTFMove(promise));
}

void ServiceWorkerRegistration::getPushSubscription(DOMPromiseDeferred<IDLNullable<IDLInterface<PushSubscription>>>&& promise)
{
    if (isContextStopped()) {
        promise.reject(Exception(InvalidStateError));
        return;
    }

    m_container->getPushSubscription(*this, WTFMove(promise));
}

void ServiceWorkerRegistration::getPushPermissionState(DOMPromiseDeferred<IDLEnumeration<PushPermissionState>>&& promise)
{
    if (isContextStopped()) {
        promise.reject(Exception(InvalidStateError));
        return;
    }

    m_container->getPushPermissionState(identifier(), WTFMove(promise));
}

void ServiceWorkerRegistration::updateStateFromServer(ServiceWorkerRegistrationState state, RefPtr<ServiceWorker>&& serviceWorker)
{
    switch (state) {
    case ServiceWorkerRegistrationState::Installing:
        REGISTRATION_RELEASE_LOG("updateStateFromServer: Setting registration %llu installing worker to %llu", identifier().toUInt64(), serviceWorker ? serviceWorker->identifier().toUInt64() : 0);
        m_installingWorker = WTFMove(serviceWorker);
        break;
    case ServiceWorkerRegistrationState::Waiting:
        REGISTRATION_RELEASE_LOG("updateStateFromServer: Setting registration %llu waiting worker to %llu", identifier().toUInt64(), serviceWorker ? serviceWorker->identifier().toUInt64() : 0);
        m_waitingWorker = WTFMove(serviceWorker);
        break;
    case ServiceWorkerRegistrationState::Active:
        REGISTRATION_RELEASE_LOG("updateStateFromServer: Setting registration %llu active worker to %llu", identifier().toUInt64(), serviceWorker ? serviceWorker->identifier().toUInt64() : 0);
        m_activeWorker = WTFMove(serviceWorker);
        break;
    }
}

void ServiceWorkerRegistration::queueTaskToFireUpdateFoundEvent()
{
    if (isContextStopped())
        return;

    REGISTRATION_RELEASE_LOG("fireUpdateFoundEvent: Firing updatefound event for registration %llu", identifier().toUInt64());

    queueTaskToDispatchEvent(*this, TaskSource::DOMManipulation, Event::create(eventNames().updatefoundEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

EventTargetInterface ServiceWorkerRegistration::eventTargetInterface() const
{
    return ServiceWorkerRegistrationEventTargetInterfaceType;
}

ScriptExecutionContext* ServiceWorkerRegistration::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

const char* ServiceWorkerRegistration::activeDOMObjectName() const
{
    return "ServiceWorkerRegistration";
}

void ServiceWorkerRegistration::stop()
{
    removeAllEventListeners();
}

bool ServiceWorkerRegistration::virtualHasPendingActivity() const
{
    return getNewestWorker() && hasEventListeners();
}

NavigationPreloadManager& ServiceWorkerRegistration::navigationPreload()
{
    if (!m_navigationPreload)
        m_navigationPreload = std::unique_ptr<NavigationPreloadManager>(new NavigationPreloadManager(*this));
    return *m_navigationPreload;
}

#if ENABLE(NOTIFICATION_EVENT)
void ServiceWorkerRegistration::showNotification(ScriptExecutionContext& context, const String& title, const NotificationOptions& options, DOMPromiseDeferred<void>&& promise)
{
    if (!m_activeWorker) {
        promise.reject(Exception { TypeError, "Registration does not have an active worker"_s });
        return;
    }

    auto* client = context.notificationClient();
    if (!client) {
        promise.reject(Exception { TypeError, "Registration not active"_s });
        return;
    }

    if (client->checkPermission(&context) != NotificationPermission::Granted) {
        promise.reject(Exception { TypeError, "Registration does not have permission to show notifications"_s });
        return;
    }

    // The Notification is kept alive by virtue of being show()'n soon.
    // FIXME: When implementing getNotifications(), store this Notification in the registration's notification list.
    auto notification = Notification::create(context, title, options);

    context.eventLoop().queueTask(TaskSource::DOMManipulation, [promise = WTFMove(promise)]() mutable {
        promise.resolve();
    });
}

void ServiceWorkerRegistration::getNotifications(ScriptExecutionContext& context, const GetNotificationOptions& filter, DOMPromiseDeferred<IDLSequence<IDLInterface<Notification>>> promise)
{
    auto notifications = filteredNotificationList(filter.tag);
    context.eventLoop().queueTask(TaskSource::DOMManipulation, [promise = WTFMove(promise), notifications = WTFMove(notifications)]() mutable {
        promise.resolve(WTFMove(notifications));
    });
}

void ServiceWorkerRegistration::addNotificationToList(Notification& notification)
{
    // A Notification at this exact address might previously have been in the list but not successfully removed.
    // The WeakHashSet captures that possibility here to help us maintain consistency.
    auto iter = m_notificationList.find(&notification);
    if (iter != m_notificationList.end()) {
        RELEASE_ASSERT(!m_notificationSet.contains(notification));
        m_notificationList.remove(iter);
    }

    m_notificationList.add(&notification);
    auto result = m_notificationSet.add(notification);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ServiceWorkerRegistration::removeNotificationFromList(Notification& notification)
{
    // The same notification might try to remove itself from this list more than once, and that's fine.
    m_notificationList.remove(&notification);
    m_notificationSet.remove(notification);
}

Vector<Ref<Notification>> ServiceWorkerRegistration::filteredNotificationList(const String& filteredTag)
{
    Vector<Ref<Notification>> results;
    for (auto* notification : m_notificationList) {
        if (!m_notificationSet.contains(*notification))
            continue;
        if (filteredTag.isEmpty() || notification->tag() == filteredTag)
            results.append(notification->copyForGetNotifications());
    }

    return results;
}

#endif // ENABLE(NOTIFICATION_EVENT)

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
