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
#include "EventNames.h"
#include "JSDOMPromiseDeferred.h"
#include "Logging.h"
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

    return adoptRef(*new ServiceWorkerRegistration(context, WTFMove(container), WTFMove(data)));
}

ServiceWorkerRegistration::ServiceWorkerRegistration(ScriptExecutionContext& context, Ref<ServiceWorkerContainer>&& container, ServiceWorkerRegistrationData&& registrationData)
    : ActiveDOMObject(&context)
    , m_registrationData(WTFMove(registrationData))
    , m_container(WTFMove(container))
{
    LOG(ServiceWorker, "Creating registration %p for registration key %s", this, m_registrationData.key.loggingString().utf8().data());
    suspendIfNeeded();

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

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
