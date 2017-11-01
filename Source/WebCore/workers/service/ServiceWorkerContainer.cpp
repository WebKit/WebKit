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

void ServiceWorkerContainer::scheduleJob(Ref<ServiceWorkerJob>&& job)
{
    ASSERT(m_swConnection);

    ServiceWorkerJob& rawJob = job.get();
    auto result = m_jobMap.add(rawJob.data().identifier(), WTFMove(job));
    ASSERT_UNUSED(result, result.isNewEntry);

    m_swConnection->scheduleJob(rawJob);
}

void ServiceWorkerContainer::getRegistration(const String&, RegistrationPromise&& promise)
{
    // FIXME: Implement getRegistration algorithm, for now pretend there is no registration.
    // https://bugs.webkit.org/show_bug.cgi?id=178882
    promise.resolve(nullptr);
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

class FakeServiceWorkerInstallMicrotask final : public Microtask {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit FakeServiceWorkerInstallMicrotask(Ref<ServiceWorkerRegistration>&& registration)
        : m_registration(WTFMove(registration))
    {
    }

private:
    Result run() final
    {
        // FIXME: We currently resolve the promise at the end of the install instead of the beginning so we need to fake
        // a few events to make it look like we are installing and activing the service worker.
        callOnMainThread([registration = WTFMove(m_registration)] () mutable {
            registration->dispatchEvent(Event::create(eventNames().updatefoundEvent, false, false));
            callOnMainThread([registration = WTFMove(registration)] () mutable {
                registration->installing()->setState(ServiceWorker::State::Installed);
                callOnMainThread([registration = WTFMove(registration)] () mutable {
                    registration->waiting()->setState(ServiceWorker::State::Activating);
                    callOnMainThread([registration = WTFMove(registration)] () mutable {
                        registration->active()->setState(ServiceWorker::State::Activated);
                    });
                });
            });
        });
        return Result::Done;
    }

    Ref<ServiceWorkerRegistration> m_registration;
};

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
    auto* activeServiceWorker = context->activeServiceWorker();
    if (!activeServiceWorker || activeServiceWorker->identifier() != data.activeServiceWorkerIdentifier) {
        context->setActiveServiceWorker(ServiceWorker::create(*context, data.activeServiceWorkerIdentifier, data.scriptURL));
        activeServiceWorker = context->activeServiceWorker();
    }

    activeServiceWorker->setState(ServiceWorker::State::Installing);
    auto registration = ServiceWorkerRegistration::create(*context, WTFMove(data), *activeServiceWorker);
    job.promise().resolve<IDLInterface<ServiceWorkerRegistration>>(registration.get());

    // Use a microtask because we need to make sure this is executed after the promise above is resolved.
    MicrotaskQueue::mainThreadQueue().append(std::make_unique<FakeServiceWorkerInstallMicrotask>(WTFMove(registration)));
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

void ServiceWorkerContainer::jobFailedLoadingScript(ServiceWorkerJob& job, const ResourceError& error)
{
    LOG(ServiceWorker, "SeviceWorkerContainer %p failed fetching script for job %" PRIu64, this, job.data().identifier());

    m_swConnection->failedFetchingScript(job, error);
}

void ServiceWorkerContainer::jobDidFinish(ServiceWorkerJob& job)
{
    auto taken = m_jobMap.take(job.data().identifier());
    ASSERT_UNUSED(taken, !taken || taken.get() == &job);
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
    return true;
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
