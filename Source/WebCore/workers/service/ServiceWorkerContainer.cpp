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
#include "ServiceWorkerContainer.h"

#if ENABLE(SERVICE_WORKER)

#include "EventNames.h"
#include "Exception.h"
#include "IDLTypes.h"
#include "JSDOMPromiseDeferred.h"
#include "JSServiceWorkerRegistration.h"
#include "Logging.h"
#include "Microtasks.h"
#include "NavigatorBase.h"
#include "ResourceError.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "ServiceWorker.h"
#include "ServiceWorkerJob.h"
#include "ServiceWorkerJobData.h"
#include "ServiceWorkerProvider.h"
#include "URL.h"
#include <wtf/RunLoop.h>
#include <wtf/Scope.h>

namespace WebCore {

ServiceWorkerContainer::ServiceWorkerContainer(ScriptExecutionContext& context, NavigatorBase& navigator)
    : ActiveDOMObject(&context)
    , m_navigator(navigator)
{
    suspendIfNeeded();

    m_readyPromise.reject(Exception { UnknownError, ASCIILiteral("serviceWorker.ready() is not yet implemented") });
}

ServiceWorkerContainer::~ServiceWorkerContainer()
{
#ifndef NDEBUG
    ASSERT(m_creationThread == currentThread());
#endif
}

void ServiceWorkerContainer::refEventTarget()
{
    m_navigator.ref();
}

void ServiceWorkerContainer::derefEventTarget()
{
    m_navigator.deref();
}

ServiceWorker* ServiceWorkerContainer::controller() const
{
    auto* context = scriptExecutionContext();
    return context ? context->activeServiceWorker() : nullptr;
}

void ServiceWorkerContainer::addRegistration(const String& relativeScriptURL, const RegistrationOptions& options, Ref<DeferredPromise>&& promise)
{
    auto* context = scriptExecutionContext();
    if (!context || !context->sessionID().isValid()) {
        ASSERT_NOT_REACHED();
        promise->reject(Exception(InvalidStateError));
        return;
    }

    if (relativeScriptURL.isEmpty()) {
        promise->reject(Exception { TypeError, ASCIILiteral("serviceWorker.register() cannot be called with an empty script URL") });
        return;
    }

    if (!m_swConnection)
        m_swConnection = &ServiceWorkerProvider::singleton().serviceWorkerConnectionForSession(scriptExecutionContext()->sessionID());

    ServiceWorkerJobData jobData(m_swConnection->identifier());

    jobData.scriptURL = context->completeURL(relativeScriptURL);
    if (!jobData.scriptURL.isValid()) {
        promise->reject(Exception { TypeError, ASCIILiteral("serviceWorker.register() must be called with a valid relative script URL") });
        return;
    }

    // FIXME: The spec disallows scripts outside of HTTP(S), but we'll likely support app custom URL schemes in WebKit.
    if (!jobData.scriptURL.protocolIsInHTTPFamily()) {
        promise->reject(Exception { TypeError, ASCIILiteral("serviceWorker.register() must be called with a script URL whose protocol is either HTTP or HTTPS") });
        return;
    }

    String path = jobData.scriptURL.path();
    if (path.containsIgnoringASCIICase("%2f") || path.containsIgnoringASCIICase("%5c")) {
        promise->reject(Exception { TypeError, ASCIILiteral("serviceWorker.register() must be called with a script URL whose path does not contain '%2f' or '%5c'") });
        return;
    }

    String scope = options.scope.isEmpty() ? ASCIILiteral("./") : options.scope;
    if (!scope.isEmpty())
        jobData.scopeURL = context->completeURL(scope);

    if (!jobData.scopeURL.isNull() && !jobData.scopeURL.protocolIsInHTTPFamily()) {
        promise->reject(Exception { TypeError, ASCIILiteral("Scope URL provided to serviceWorker.register() must be either HTTP or HTTPS") });
        return;
    }

    path = jobData.scopeURL.path();
    if (path.containsIgnoringASCIICase("%2f") || path.containsIgnoringASCIICase("%5c")) {
        promise->reject(Exception { TypeError, ASCIILiteral("Scope URL provided to serviceWorker.register() cannot have a path that contains '%2f' or '%5c'") });
        return;
    }

    jobData.clientCreationURL = context->url();
    jobData.topOrigin = SecurityOriginData::fromSecurityOrigin(context->topOrigin());
    jobData.type = ServiceWorkerJobType::Register;
    jobData.registrationOptions = options;

    scheduleJob(ServiceWorkerJob::create(*this, WTFMove(promise), WTFMove(jobData)));
}

void ServiceWorkerContainer::removeRegistration(const URL& scopeURL, Ref<DeferredPromise>&& promise)
{
    auto* context = scriptExecutionContext();
    if (!context || !context->sessionID().isValid()) {
        ASSERT_NOT_REACHED();
        promise->reject(Exception(InvalidStateError));
        return;
    }

    if (!m_swConnection) {
        ASSERT_NOT_REACHED();
        promise->reject(Exception(InvalidStateError));
        return;
    }

    ServiceWorkerJobData jobData(m_swConnection->identifier());
    jobData.clientCreationURL = context->url();
    jobData.topOrigin = SecurityOriginData::fromSecurityOrigin(context->topOrigin());
    jobData.type = ServiceWorkerJobType::Unregister;
    jobData.scopeURL = scopeURL;

    scheduleJob(ServiceWorkerJob::create(*this, WTFMove(promise), WTFMove(jobData)));
}

void ServiceWorkerContainer::updateRegistration(const URL& scopeURL, const URL& scriptURL, WorkerType, Ref<DeferredPromise>&& promise)
{
    auto* context = scriptExecutionContext();
    if (!context || !context->sessionID().isValid()) {
        ASSERT_NOT_REACHED();
        promise->reject(Exception(InvalidStateError));
        return;
    }

    if (!m_swConnection) {
        ASSERT_NOT_REACHED();
        promise->reject(Exception(InvalidStateError));
        return;
    }

    ServiceWorkerJobData jobData(m_swConnection->identifier());
    jobData.clientCreationURL = context->url();
    jobData.topOrigin = SecurityOriginData::fromSecurityOrigin(context->topOrigin());
    jobData.type = ServiceWorkerJobType::Update;
    jobData.scopeURL = scopeURL;
    jobData.scriptURL = scriptURL;

    scheduleJob(ServiceWorkerJob::create(*this, WTFMove(promise), WTFMove(jobData)));
}

void ServiceWorkerContainer::scheduleJob(Ref<ServiceWorkerJob>&& job)
{
    ASSERT(m_swConnection);

    setPendingActivity(this);

    ServiceWorkerJob& rawJob = job.get();
    auto result = m_jobMap.add(rawJob.data().identifier(), WTFMove(job));
    ASSERT_UNUSED(result, result.isNewEntry);

    m_swConnection->scheduleJob(rawJob);
}

void ServiceWorkerContainer::getRegistration(const String& clientURL, Ref<DeferredPromise>&& promise)
{
    auto* context = scriptExecutionContext();
    if (!context) {
        ASSERT_NOT_REACHED();
        promise->reject(Exception { InvalidStateError });
        return;
    }

    URL parsedURL = context->completeURL(clientURL);
    if (!protocolHostAndPortAreEqual(parsedURL, context->url())) {
        promise->reject(Exception { SecurityError, ASCIILiteral("Origin of clientURL is not client's origin") });
        return;
    }

    if (!m_swConnection)
        m_swConnection = &ServiceWorkerProvider::singleton().serviceWorkerConnectionForSession(context->sessionID());

    return m_swConnection->matchRegistration(context->topOrigin(), parsedURL, [promise = WTFMove(promise), protectingThis = makePendingActivity(*this), this] (auto&& result) mutable {
        if (m_isStopped)
            return;

        if (!result) {
            promise->resolve();
            return;
        }

        auto registration = ServiceWorkerRegistration::getOrCreate(*scriptExecutionContext(), *this, WTFMove(result.value()));
        promise->resolve<IDLInterface<ServiceWorkerRegistration>>(WTFMove(registration));
    });
}

void ServiceWorkerContainer::scheduleTaskToUpdateRegistrationState(ServiceWorkerRegistrationIdentifier identifier, ServiceWorkerRegistrationState state, const std::optional<ServiceWorkerData>& serviceWorkerData)
{
    auto* context = scriptExecutionContext();
    if (!context)
        return;

    RefPtr<ServiceWorker> serviceWorker;
    if (serviceWorkerData)
        serviceWorker = ServiceWorker::getOrCreate(*context, ServiceWorkerData { *serviceWorkerData });

    context->postTask([this, protectedThis = makeRef(*this), identifier, state, serviceWorker = WTFMove(serviceWorker)](ScriptExecutionContext&) mutable {
        if (auto* registration = m_registrations.get(identifier))
            registration->updateStateFromServer(state, WTFMove(serviceWorker));
    });
}

void ServiceWorkerContainer::getRegistrations(RegistrationsPromise&& promise)
{
    auto* context = scriptExecutionContext();
    if (!context) {
        promise.reject(Exception { InvalidStateError });
        return;
    }

    if (!m_swConnection)
        m_swConnection = &ServiceWorkerProvider::singleton().serviceWorkerConnectionForSession(context->sessionID());

    return m_swConnection->getRegistrations(context->topOrigin(), context->url(), [this, pendingActivity = makePendingActivity(*this), promise = WTFMove(promise)] (auto&& registrationDatas) mutable {
        if (m_isStopped)
            return;

        Vector<Ref<ServiceWorkerRegistration>> registrations;
        registrations.reserveInitialCapacity(registrationDatas.size());
        for (auto& registrationData : registrationDatas) {
            auto registration = ServiceWorkerRegistration::getOrCreate(*scriptExecutionContext(), *this, WTFMove(registrationData));
            registrations.uncheckedAppend(WTFMove(registration));
        }

        promise.resolve(WTFMove(registrations));
    });
}

void ServiceWorkerContainer::startMessages()
{
}

void ServiceWorkerContainer::jobFailedWithException(ServiceWorkerJob& job, const Exception& exception)
{
    if (auto* context = scriptExecutionContext()) {
        context->postTask([job = makeRef(job), exception](ScriptExecutionContext&) {
            job->promise().reject(exception);
        });
    }
    jobDidFinish(job);
}

void ServiceWorkerContainer::scheduleTaskToFireUpdateFoundEvent(ServiceWorkerRegistrationIdentifier identifier)
{
    if (isStopped())
        return;

    scriptExecutionContext()->postTask([this, protectedThis = makeRef(*this), identifier](ScriptExecutionContext&) {
        if (isStopped())
            return;

        if (auto* registration = m_registrations.get(identifier))
            registration->dispatchEvent(Event::create(eventNames().updatefoundEvent, false, false));
    });
}

void ServiceWorkerContainer::jobResolvedWithRegistration(ServiceWorkerJob& job, ServiceWorkerRegistrationData&& data, WTF::Function<void()>&& promiseResolvedHandler)
{
    auto guard = WTF::makeScopeExit([this, &job] {
        jobDidFinish(job);
    });

    auto* context = scriptExecutionContext();
    if (!context) {
        LOG_ERROR("ServiceWorkerContainer::jobResolvedWithRegistration called but the containers ScriptExecutionContext is gone");
        promiseResolvedHandler();
        return;
    }

    context->postTask([this, protectedThis = makeRef(*this), job = makeRef(job), data = WTFMove(data), promiseResolvedHandler = WTFMove(promiseResolvedHandler)](ScriptExecutionContext& context) mutable {
        auto registration = ServiceWorkerRegistration::getOrCreate(context, *this, WTFMove(data));

        LOG(ServiceWorker, "Container %p resolved job with registration %p", this, registration.ptr());

        job->promise().resolve<IDLInterface<ServiceWorkerRegistration>>(WTFMove(registration));

        MicrotaskQueue::mainThreadQueue().append(std::make_unique<VoidMicrotask>([promiseResolvedHandler = WTFMove(promiseResolvedHandler)] {
            promiseResolvedHandler();
        }));
    });
}

void ServiceWorkerContainer::jobResolvedWithUnregistrationResult(ServiceWorkerJob& job, bool unregistrationResult)
{
    auto guard = WTF::makeScopeExit([this, &job] {
        jobDidFinish(job);
    });

    auto* context = scriptExecutionContext();
    if (!context) {
        LOG_ERROR("ServiceWorkerContainer::jobResolvedWithUnregistrationResult called but the containers ScriptExecutionContext is gone");
        return;
    }

    // FIXME: Implement proper selection of service workers.
    if (unregistrationResult)
        context->setActiveServiceWorker(nullptr);

    context->postTask([job = makeRef(job), unregistrationResult](ScriptExecutionContext&) mutable {
        job->promise().resolve<IDLBoolean>(unregistrationResult);
    });
}

void ServiceWorkerContainer::startScriptFetchForJob(ServiceWorkerJob& job)
{
    LOG(ServiceWorker, "SeviceWorkerContainer %p starting script fetch for job %" PRIu64, this, job.data().identifier());

    auto* context = scriptExecutionContext();
    if (!context) {
        LOG_ERROR("ServiceWorkerContainer::jobResolvedWithRegistration called but the container's ScriptExecutionContext is gone");
        m_swConnection->failedFetchingScript(job, { errorDomainWebKitInternal, 0, job.data().scriptURL, ASCIILiteral("Attempt to fetch service worker script with no ScriptExecutionContext") });
        jobDidFinish(job);
        return;
    }

    job.fetchScriptWithContext(*context);
}

void ServiceWorkerContainer::jobFinishedLoadingScript(ServiceWorkerJob& job, const String& script)
{
    LOG(ServiceWorker, "SeviceWorkerContainer %p finished fetching script for job %" PRIu64, this, job.data().identifier());

    m_swConnection->finishedFetchingScript(job, script);
}

void ServiceWorkerContainer::jobFailedLoadingScript(ServiceWorkerJob& job, const ResourceError& error, std::optional<Exception>&& exception)
{
    LOG(ServiceWorker, "SeviceWorkerContainer %p failed fetching script for job %" PRIu64, this, job.data().identifier());

    if (exception)
        job.promise().reject(*exception);

    m_swConnection->failedFetchingScript(job, error);
}

void ServiceWorkerContainer::jobDidFinish(ServiceWorkerJob& job)
{
    auto taken = m_jobMap.take(job.data().identifier());
    ASSERT_UNUSED(taken, !taken || taken->ptr() == &job);

    unsetPendingActivity(this);
}

uint64_t ServiceWorkerContainer::connectionIdentifier()
{
    ASSERT(m_swConnection);
    return m_swConnection->identifier();
}

const char* ServiceWorkerContainer::activeDOMObjectName() const
{
    return "ServiceWorkerContainer";
}

bool ServiceWorkerContainer::canSuspendForDocumentSuspension() const
{
    return !hasPendingActivity();
}

void ServiceWorkerContainer::addRegistration(ServiceWorkerRegistration& registration)
{
    m_swConnection->addServiceWorkerRegistrationInServer(registration.data().key, registration.identifier());
    m_registrations.add(registration.identifier(), &registration);
}

void ServiceWorkerContainer::removeRegistration(ServiceWorkerRegistration& registration)
{
    m_swConnection->removeServiceWorkerRegistrationInServer(registration.data().key, registration.identifier());
    m_registrations.remove(registration.identifier());
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
