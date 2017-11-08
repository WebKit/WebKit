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

        RefPtr<ServiceWorkerRegistration> registration = m_registrations.get(result->key);
        if (!registration) {
            auto& context = *scriptExecutionContext();
            // FIXME: We should probably not be constructing ServiceWorkerRegistration objects here. Instead, we should make
            // sure that ServiceWorkerRegistration objects stays alive as long as their SWServerRegistration on server side.
            registration = ServiceWorkerRegistration::create(context, *this, WTFMove(result.value()));
        }
        promise->resolve<IDLInterface<ServiceWorkerRegistration>>(registration.releaseNonNull());
    });
}

void ServiceWorkerContainer::updateRegistrationState(const ServiceWorkerRegistrationKey& key, ServiceWorkerRegistrationState state, const std::optional<ServiceWorkerIdentifier>& serviceWorkerIdentifier)
{
    if (auto* registration = m_registrations.get(key))
        registration->updateStateFromServer(state, serviceWorkerIdentifier);
}

void ServiceWorkerContainer::getRegistrations(RegistrationsPromise&& promise)
{
    // FIXME: Implement getRegistrations algorithm, for now pretend there is no registration.
    promise.resolve({ });
}

void ServiceWorkerContainer::startMessages()
{
}

void ServiceWorkerContainer::jobFailedWithException(ServiceWorkerJob& job, const Exception& exception)
{
    job.promise().reject(exception);
    jobDidFinish(job);
}

void ServiceWorkerContainer::fireUpdateFoundEvent(const ServiceWorkerRegistrationKey& key)
{
    if (isStopped())
        return;

    auto* registration = m_registrations.get(key);
    if (!registration)
        return;

    scriptExecutionContext()->postTask([container = makeRef(*this), registration = makeRef(*registration)] (ScriptExecutionContext&) {
        if (container->isStopped())
            return;
        registration->dispatchEvent(Event::create(eventNames().updatefoundEvent, false, false));
    });
}

class FirePostInstallEventsMicrotask final : public Microtask {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit FirePostInstallEventsMicrotask(Ref<ServiceWorkerContainer>&& container, Ref<ServiceWorkerRegistration>&& registration)
        : m_container(WTFMove(container))
        , m_registration(WTFMove(registration))
    {
    }
private:
    Result run() final
    {
        auto* serviceWorker = m_registration->installing();
        if (!serviceWorker)
            return Result::Done;

        callOnMainThread([container = WTFMove(m_container), registration = WTFMove(m_registration), serviceWorker = makeRef(*serviceWorker)] () mutable {
            if (container->isStopped())
                return;
            registration->setInstallingWorker(nullptr);
            registration->setWaitingWorker(serviceWorker.copyRef());
            serviceWorker->updateWorkerState(ServiceWorker::State::Installed);
            callOnMainThread([container = WTFMove(container), registration = WTFMove(registration), serviceWorker = WTFMove(serviceWorker)] () mutable {
                if (container->isStopped())
                    return;
                registration->setWaitingWorker(nullptr);
                registration->setActiveWorker(serviceWorker.copyRef());
                serviceWorker->updateWorkerState(ServiceWorker::State::Activating);
                callOnMainThread([container = WTFMove(container), serviceWorker = WTFMove(serviceWorker)] () mutable {
                    if (container->isStopped())
                        return;
                    serviceWorker->updateWorkerState(ServiceWorker::State::Activated);
                });
            });
        });
        return Result::Done;
    }

    Ref<ServiceWorkerContainer> m_container;
    Ref<ServiceWorkerRegistration> m_registration;
};

// FIXME: This method is only use to mimick service worker activation and will do away once we implement it.
void ServiceWorkerContainer::firePostInstallEvents(const ServiceWorkerRegistrationKey& key)
{
    if (isStopped())
        return;

    auto* registration = m_registrations.get(key);
    if (!registration)
        return;

    MicrotaskQueue::mainThreadQueue().append(std::make_unique<FirePostInstallEventsMicrotask>(*this, *registration));
}

void ServiceWorkerContainer::jobResolvedWithRegistration(ServiceWorkerJob& job, ServiceWorkerRegistrationData&& data)
{
    auto guard = WTF::makeScopeExit([this, &job] {
        jobDidFinish(job);
    });

    auto* context = scriptExecutionContext();
    if (!context) {
        LOG_ERROR("ServiceWorkerContainer::jobResolvedWithRegistration called but the containers ScriptExecutionContext is gone");
        return;
    }

    // FIXME: Implement proper selection of service workers.
    auto* installingServiceWorker = context->activeServiceWorker();
    ASSERT(data.installingServiceWorkerIdentifier);
    if (!installingServiceWorker || installingServiceWorker->identifier() != *data.installingServiceWorkerIdentifier) {
        context->setActiveServiceWorker(ServiceWorker::create(*context, *data.installingServiceWorkerIdentifier, data.scriptURL));
        installingServiceWorker = context->activeServiceWorker();
    }

    RefPtr<ServiceWorkerRegistration> registration = m_registrations.get(data.key);
    if (!registration) {
        // Currently the only registrations that can be created for the first time here should be Installing.
        ASSERT(data.installingServiceWorkerIdentifier);
        auto installingIdentifier = *data.installingServiceWorkerIdentifier;
        
        registration = ServiceWorkerRegistration::create(*context, *this, WTFMove(data));
        registration->updateStateFromServer(ServiceWorkerRegistrationState::Installing, installingIdentifier);
        ASSERT(registration->installing());

        installingServiceWorker = registration->installing();
    }

    installingServiceWorker->updateWorkerState(ServiceWorkerState::Installing, ServiceWorker::DoNotFireStateChangeEvent);
    registration->setInstallingWorker(installingServiceWorker);

    LOG(ServiceWorker, "Container %p resolved job with registration %p", this, registration.get());

    job.promise().resolve<IDLInterface<ServiceWorkerRegistration>>(*registration);
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

    job.promise().resolve<IDLBoolean>(unregistrationResult);
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
    m_registrations.add(registration.data().key, &registration);
}

void ServiceWorkerContainer::removeRegistration(ServiceWorkerRegistration& registration)
{
    m_swConnection->removeServiceWorkerRegistrationInServer(registration.data().key, registration.identifier());
    m_registrations.remove(registration.data().key);
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
