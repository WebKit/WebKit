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
#include "Logging.h"
#include "ServiceWorker.h"
#include "ServiceWorkerContainer.h"
#include "ServiceWorkerTypes.h"
#include "WorkerGlobalScope.h"

namespace WebCore {

ServiceWorkerRegistration::ServiceWorkerRegistration(ScriptExecutionContext& context, Ref<ServiceWorkerContainer>&& container, ServiceWorkerRegistrationData&& registrationData)
    : ActiveDOMObject(&context)
    , m_registrationData(WTFMove(registrationData))
    , m_container(WTFMove(container))
{
    LOG(ServiceWorker, "Creating registration %p for registration key %s", this, m_registrationData.key.loggingString().utf8().data());
    suspendIfNeeded();

    // FIXME: Reconcile worker state properly (see below)
    if (m_registrationData.installingServiceWorkerIdentifier)
        m_installingWorker = ServiceWorker::create(context, *m_registrationData.installingServiceWorkerIdentifier, m_registrationData.scriptURL, ServiceWorker::State::Installing);
    if (m_registrationData.waitingServiceWorkerIdentifier)
        m_waitingWorker = ServiceWorker::create(context, *m_registrationData.waitingServiceWorkerIdentifier, m_registrationData.scriptURL, ServiceWorker::State::Installed);
    if (m_registrationData.activeServiceWorkerIdentifier) {
        // FIXME: Activating or Activated? This is why we have to have more data here...
        m_activeWorker = ServiceWorker::create(context, *m_registrationData.activeServiceWorkerIdentifier, m_registrationData.scriptURL, ServiceWorker::State::Activated);
    }

    // FIXME: Implement proper selection of service workers.
    context.setActiveServiceWorker(getNewestWorker());

    m_container->addRegistration(*this);
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

ServiceWorker* ServiceWorkerRegistration::getNewestWorker()
{
    if (m_installingWorker)
        return m_installingWorker.get();
    if (m_waitingWorker)
        return m_waitingWorker.get();

    return m_activeWorker.get();
}

const String& ServiceWorkerRegistration::scope() const
{
    return m_registrationData.scopeURL;
}

ServiceWorkerUpdateViaCache ServiceWorkerRegistration::updateViaCache() const
{
    return m_registrationData.updateViaCache;
}

void ServiceWorkerRegistration::update(Ref<DeferredPromise>&& promise)
{
    auto* context = scriptExecutionContext();
    if (!context) {
        ASSERT_NOT_REACHED();
        promise->reject(Exception(InvalidStateError));
        return;
    }

    auto* container = context->serviceWorkerContainer();
    if (!container) {
        promise->reject(Exception(InvalidStateError));
        return;
    }

    auto* newestWorker = getNewestWorker();
    if (!newestWorker) {
        promise->reject(Exception(InvalidStateError, ASCIILiteral("newestWorker is null")));
        return;
    }

    // FIXME: Support worker types.
    container->updateRegistration(m_registrationData.scopeURL, newestWorker->scriptURL(), WorkerType::Classic, WTFMove(promise));
}

void ServiceWorkerRegistration::unregister(Ref<DeferredPromise>&& promise)
{
    auto* context = scriptExecutionContext();
    if (!context) {
        ASSERT_NOT_REACHED();
        promise->reject(Exception(InvalidStateError));
        return;
    }

    auto* container = context->serviceWorkerContainer();
    if (!container) {
        promise->reject(Exception(InvalidStateError));
        return;
    }

    container->removeRegistration(m_registrationData.scopeURL, WTFMove(promise));
}

void ServiceWorkerRegistration::updateStateFromServer(ServiceWorkerRegistrationState state, std::optional<ServiceWorkerIdentifier> serviceWorkerIdentifier)
{
    auto* context = scriptExecutionContext();
    if (!context)
        return;

    RefPtr<ServiceWorker> worker;
    if (serviceWorkerIdentifier) {
        auto* newestWorker = getNewestWorker();
        if (newestWorker && newestWorker->identifier() == *serviceWorkerIdentifier)
            worker = newestWorker;
        else
            worker = ServiceWorker::create(*context, *serviceWorkerIdentifier, m_registrationData.scriptURL);
    }

    switch (state) {
    case ServiceWorkerRegistrationState::Installing:
        m_installingWorker = WTFMove(worker);
        break;
    case ServiceWorkerRegistrationState::Waiting:
        m_waitingWorker = WTFMove(worker);
        break;
    case ServiceWorkerRegistrationState::Active:
        m_activeWorker = WTFMove(worker);
        break;
    }
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

bool ServiceWorkerRegistration::canSuspendForDocumentSuspension() const
{
    return !hasPendingActivity();
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
