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

#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "Exception.h"
#include "IDLTypes.h"
#include "JSDOMPromiseDeferred.h"
#include "JSServiceWorkerRegistration.h"
#include "Logging.h"
#include "NavigatorBase.h"
#include "ResourceError.h"
#include "SchemeRegistry.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "ServiceWorker.h"
#include "ServiceWorkerFetchResult.h"
#include "ServiceWorkerGlobalScope.h"
#include "ServiceWorkerJob.h"
#include "ServiceWorkerJobData.h"
#include "ServiceWorkerProvider.h"
#include "ServiceWorkerThread.h"
#include <wtf/RunLoop.h>
#include <wtf/Scope.h>
#include <wtf/URL.h>

#define CONTAINER_RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), ServiceWorker, "%p - ServiceWorkerContainer::" fmt, this, ##__VA_ARGS__)
#define CONTAINER_RELEASE_LOG_ERROR_IF_ALLOWED(fmt, ...) RELEASE_LOG_ERROR_IF(isAlwaysOnLoggingAllowed(), ServiceWorker, "%p - ServiceWorkerContainer::" fmt, this, ##__VA_ARGS__)

namespace WebCore {

ServiceWorkerContainer::ServiceWorkerContainer(ScriptExecutionContext* context, NavigatorBase& navigator)
    : ActiveDOMObject(context)
    , m_navigator(navigator)
{
    suspendIfNeeded();
}

ServiceWorkerContainer::~ServiceWorkerContainer()
{
#ifndef NDEBUG
    ASSERT(m_creationThread.ptr() == &Thread::current());
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

auto ServiceWorkerContainer::ready() -> ReadyPromise&
{
    if (!m_readyPromise) {
        m_readyPromise = std::make_unique<ReadyPromise>();

        if (m_isStopped || !scriptExecutionContext()->sessionID().isValid())
            return *m_readyPromise;

        auto& context = *scriptExecutionContext();
        auto contextIdentifier = this->contextIdentifier();
        callOnMainThread([connection = makeRef(ensureSWClientConnection()), topOrigin = context.topOrigin().isolatedCopy(), clientURL = context.url().isolatedCopy(), contextIdentifier]() mutable {
            connection->whenRegistrationReady(topOrigin, clientURL, [contextIdentifier](auto&& registrationData) {
                ScriptExecutionContext::postTaskTo(contextIdentifier, [registrationData = crossThreadCopy(registrationData)](auto& context) mutable {
                    auto* serviceWorkerContainer = context.serviceWorkerContainer();
                    if (!serviceWorkerContainer)
                        return;
                    if (serviceWorkerContainer->m_isStopped || !context.sessionID().isValid())
                        return;

                    auto registration = ServiceWorkerRegistration::getOrCreate(context, *serviceWorkerContainer, WTFMove(registrationData));
                    serviceWorkerContainer->m_readyPromise->resolve(WTFMove(registration));
                });
            });
        });
    }
    return *m_readyPromise;
}

ServiceWorker* ServiceWorkerContainer::controller() const
{
    auto* context = scriptExecutionContext();
    ASSERT_WITH_MESSAGE(!context || is<Document>(*context) || !context->activeServiceWorker(), "Only documents can have a controller at the moment.");
    return context ? context->activeServiceWorker() : nullptr;
}

void ServiceWorkerContainer::addRegistration(const String& relativeScriptURL, const RegistrationOptions& options, Ref<DeferredPromise>&& promise)
{
    auto* context = scriptExecutionContext();
    if (m_isStopped || !context->sessionID().isValid()) {
        promise->reject(Exception(InvalidStateError));
        return;
    }

    if (relativeScriptURL.isEmpty()) {
        promise->reject(Exception { TypeError, "serviceWorker.register() cannot be called with an empty script URL"_s });
        return;
    }

    ServiceWorkerJobData jobData(ensureSWClientConnection().serverConnectionIdentifier(), contextIdentifier());

    jobData.scriptURL = context->completeURL(relativeScriptURL);
    if (!jobData.scriptURL.isValid()) {
        CONTAINER_RELEASE_LOG_ERROR_IF_ALLOWED("addRegistration: Invalid scriptURL");
        promise->reject(Exception { TypeError, "serviceWorker.register() must be called with a valid relative script URL"_s });
        return;
    }

    if (!SchemeRegistry::canServiceWorkersHandleURLScheme(jobData.scriptURL.protocol().toStringWithoutCopying())) {
        CONTAINER_RELEASE_LOG_ERROR_IF_ALLOWED("addRegistration: Invalid scriptURL scheme is not HTTP or HTTPS");
        promise->reject(Exception { TypeError, "serviceWorker.register() must be called with a script URL whose protocol is either HTTP or HTTPS"_s });
        return;
    }

    String path = jobData.scriptURL.path();
    if (path.containsIgnoringASCIICase("%2f") || path.containsIgnoringASCIICase("%5c")) {
        CONTAINER_RELEASE_LOG_ERROR_IF_ALLOWED("addRegistration: scriptURL contains invalid character");
        promise->reject(Exception { TypeError, "serviceWorker.register() must be called with a script URL whose path does not contain '%2f' or '%5c'"_s });
        return;
    }

    if (!options.scope.isEmpty())
        jobData.scopeURL = context->completeURL(options.scope);
    else
        jobData.scopeURL = URL(jobData.scriptURL, "./");

    if (!jobData.scopeURL.isNull() && !SchemeRegistry::canServiceWorkersHandleURLScheme(jobData.scopeURL.protocol().toStringWithoutCopying())) {
        CONTAINER_RELEASE_LOG_ERROR_IF_ALLOWED("addRegistration: scopeURL scheme is not HTTP or HTTPS");
        promise->reject(Exception { TypeError, "Scope URL provided to serviceWorker.register() must be either HTTP or HTTPS"_s });
        return;
    }

    path = jobData.scopeURL.path();
    if (path.containsIgnoringASCIICase("%2f") || path.containsIgnoringASCIICase("%5c")) {
        CONTAINER_RELEASE_LOG_ERROR_IF_ALLOWED("addRegistration: scopeURL contains invalid character");
        promise->reject(Exception { TypeError, "Scope URL provided to serviceWorker.register() cannot have a path that contains '%2f' or '%5c'"_s });
        return;
    }

    CONTAINER_RELEASE_LOG_IF_ALLOWED("addRegistration: Registering service worker. Job ID: %" PRIu64, jobData.identifier().jobIdentifier.toUInt64());

    jobData.clientCreationURL = context->url();
    jobData.topOrigin = context->topOrigin().data();
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

    ServiceWorkerJobData jobData(m_swConnection->serverConnectionIdentifier(), contextIdentifier());
    jobData.clientCreationURL = context->url();
    jobData.topOrigin = context->topOrigin().data();
    jobData.type = ServiceWorkerJobType::Unregister;
    jobData.scopeURL = scopeURL;

    CONTAINER_RELEASE_LOG_IF_ALLOWED("removeRegistration: Unregistering service worker. Job ID: %" PRIu64, jobData.identifier().jobIdentifier.toUInt64());

    scheduleJob(ServiceWorkerJob::create(*this, WTFMove(promise), WTFMove(jobData)));
}

void ServiceWorkerContainer::updateRegistration(const URL& scopeURL, const URL& scriptURL, WorkerType, RefPtr<DeferredPromise>&& promise)
{
    ASSERT(!m_isStopped);

    auto& context = *scriptExecutionContext();
    ASSERT(context.sessionID().isValid());

    if (!m_swConnection) {
        ASSERT_NOT_REACHED();
        if (promise)
            promise->reject(Exception(InvalidStateError));
        return;
    }

    ServiceWorkerJobData jobData(m_swConnection->serverConnectionIdentifier(), contextIdentifier());
    jobData.clientCreationURL = context.url();
    jobData.topOrigin = context.topOrigin().data();
    jobData.type = ServiceWorkerJobType::Update;
    jobData.scopeURL = scopeURL;
    jobData.scriptURL = scriptURL;

    CONTAINER_RELEASE_LOG_IF_ALLOWED("removeRegistration: Updating service worker. Job ID: %" PRIu64, jobData.identifier().jobIdentifier.toUInt64());

    scheduleJob(ServiceWorkerJob::create(*this, WTFMove(promise), WTFMove(jobData)));
}

void ServiceWorkerContainer::scheduleJob(Ref<ServiceWorkerJob>&& job)
{
#ifndef NDEBUG
    ASSERT(m_creationThread.ptr() == &Thread::current());
#endif

    ASSERT(m_swConnection);

    setPendingActivity(this);

    auto& jobData = job->data();
    auto result = m_jobMap.add(job->identifier(), WTFMove(job));
    ASSERT_UNUSED(result, result.isNewEntry);

    callOnMainThread([connection = m_swConnection, contextIdentifier = this->contextIdentifier(), jobData = jobData.isolatedCopy()] {
        connection->scheduleJob(contextIdentifier, jobData);
    });
}

void ServiceWorkerContainer::getRegistration(const String& clientURL, Ref<DeferredPromise>&& promise)
{
    auto* context = scriptExecutionContext();
    if (m_isStopped || !context->sessionID().isValid()) {
        promise->reject(Exception { InvalidStateError });
        return;
    }

    URL parsedURL = context->completeURL(clientURL);
    if (!protocolHostAndPortAreEqual(parsedURL, context->url())) {
        promise->reject(Exception { SecurityError, "Origin of clientURL is not client's origin"_s });
        return;
    }

    uint64_t pendingPromiseIdentifier = ++m_lastPendingPromiseIdentifier;
    auto pendingPromise = std::make_unique<PendingPromise>(WTFMove(promise), makePendingActivity(*this));
    m_pendingPromises.add(pendingPromiseIdentifier, WTFMove(pendingPromise));

    auto contextIdentifier = this->contextIdentifier();
    callOnMainThread([connection = makeRef(ensureSWClientConnection()), this, topOrigin = context->topOrigin().data().isolatedCopy(), parsedURL = parsedURL.isolatedCopy(), contextIdentifier, pendingPromiseIdentifier]() mutable {
        connection->matchRegistration(WTFMove(topOrigin), parsedURL, [this, contextIdentifier, pendingPromiseIdentifier] (auto&& result) mutable {
            ScriptExecutionContext::postTaskTo(contextIdentifier, [this, pendingPromiseIdentifier, result = crossThreadCopy(result)](ScriptExecutionContext&) mutable {
                this->didFinishGetRegistrationRequest(pendingPromiseIdentifier, WTFMove(result));
            });
        });
    });
}

void ServiceWorkerContainer::didFinishGetRegistrationRequest(uint64_t pendingPromiseIdentifier, std::optional<ServiceWorkerRegistrationData>&& result)
{
#ifndef NDEBUG
    ASSERT(m_creationThread.ptr() == &Thread::current());
#endif

    auto pendingPromise = m_pendingPromises.take(pendingPromiseIdentifier);
    if (!pendingPromise)
        return;

    if (m_isStopped || !scriptExecutionContext()->sessionID().isValid()) {
        pendingPromise->promise->reject(Exception { InvalidStateError });
        return;
    }

    if (!result) {
        pendingPromise->promise->resolve();
        return;
    }

    auto registration = ServiceWorkerRegistration::getOrCreate(*scriptExecutionContext(), *this, WTFMove(result.value()));
    pendingPromise->promise->resolve<IDLInterface<ServiceWorkerRegistration>>(WTFMove(registration));
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

void ServiceWorkerContainer::getRegistrations(Ref<DeferredPromise>&& promise)
{
    auto* context = scriptExecutionContext();
    if (m_isStopped || !context->sessionID().isValid()) {
        promise->reject(Exception { InvalidStateError });
        return;
    }

    uint64_t pendingPromiseIdentifier = ++m_lastPendingPromiseIdentifier;
    auto pendingPromise = std::make_unique<PendingPromise>(WTFMove(promise), makePendingActivity(*this));
    m_pendingPromises.add(pendingPromiseIdentifier, WTFMove(pendingPromise));

    auto contextIdentifier = this->contextIdentifier();
    auto contextURL = context->url();
    callOnMainThread([connection = makeRef(ensureSWClientConnection()), this, topOrigin = context->topOrigin().data().isolatedCopy(), contextURL = contextURL.isolatedCopy(), contextIdentifier, pendingPromiseIdentifier]() mutable {
        connection->getRegistrations(WTFMove(topOrigin), contextURL, [this, contextIdentifier, pendingPromiseIdentifier] (auto&& registrationDatas) mutable {
            ScriptExecutionContext::postTaskTo(contextIdentifier, [this, pendingPromiseIdentifier, registrationDatas = crossThreadCopy(registrationDatas)](ScriptExecutionContext&) mutable {
                this->didFinishGetRegistrationsRequest(pendingPromiseIdentifier, WTFMove(registrationDatas));
            });
        });
    });
}

void ServiceWorkerContainer::didFinishGetRegistrationsRequest(uint64_t pendingPromiseIdentifier, Vector<ServiceWorkerRegistrationData>&& registrationDatas)
{
#ifndef NDEBUG
    ASSERT(m_creationThread.ptr() == &Thread::current());
#endif

    auto pendingPromise = m_pendingPromises.take(pendingPromiseIdentifier);
    if (!pendingPromise)
        return;

    if (m_isStopped || !scriptExecutionContext()->sessionID().isValid()) {
        pendingPromise->promise->reject(Exception { InvalidStateError });
        return;
    }

    auto registrations = WTF::map(WTFMove(registrationDatas), [&] (auto&& registrationData) {
        return ServiceWorkerRegistration::getOrCreate(*this->scriptExecutionContext(), *this, WTFMove(registrationData));
    });

    pendingPromise->promise->resolve<IDLSequence<IDLInterface<ServiceWorkerRegistration>>>(WTFMove(registrations));
}

void ServiceWorkerContainer::startMessages()
{
}

void ServiceWorkerContainer::jobFailedWithException(ServiceWorkerJob& job, const Exception& exception)
{
#ifndef NDEBUG
    ASSERT(m_creationThread.ptr() == &Thread::current());
#endif

    ASSERT_WITH_MESSAGE(job.promise() || job.data().type == ServiceWorkerJobType::Update, "Only soft updates have no promise");

    auto guard = WTF::makeScopeExit([this, &job] {
        jobDidFinish(job);
    });

    CONTAINER_RELEASE_LOG_ERROR_IF_ALLOWED("jobFailedWithException: Job %" PRIu64 " failed with error %s", job.identifier().toUInt64(), exception.message().utf8().data());

    if (!job.promise())
        return;

    if (auto* context = scriptExecutionContext()) {
        context->postTask([job = makeRef(job), exception](ScriptExecutionContext&) {
            job->promise()->reject(exception);
        });
    }
}

void ServiceWorkerContainer::scheduleTaskToFireUpdateFoundEvent(ServiceWorkerRegistrationIdentifier identifier)
{
#ifndef NDEBUG
    ASSERT(m_creationThread.ptr() == &Thread::current());
#endif

    if (auto* registration = m_registrations.get(identifier))
        registration->scheduleTaskToFireUpdateFoundEvent();
}

void ServiceWorkerContainer::jobResolvedWithRegistration(ServiceWorkerJob& job, ServiceWorkerRegistrationData&& data, ShouldNotifyWhenResolved shouldNotifyWhenResolved)
{
#ifndef NDEBUG
    ASSERT(m_creationThread.ptr() == &Thread::current());
#endif
    ASSERT_WITH_MESSAGE(job.promise() || job.data().type == ServiceWorkerJobType::Update, "Only soft updates have no promise");

    auto guard = WTF::makeScopeExit([this, &job] {
        jobDidFinish(job);
    });

    if (job.data().type == ServiceWorkerJobType::Register)
        CONTAINER_RELEASE_LOG_IF_ALLOWED("jobResolvedWithRegistration: Registration job %" PRIu64 " succeeded", job.identifier().toUInt64());
    else {
        ASSERT(job.data().type == ServiceWorkerJobType::Update);
        CONTAINER_RELEASE_LOG_IF_ALLOWED("jobResolvedWithRegistration: Update job %" PRIu64 " succeeded", job.identifier().toUInt64());
    }

    std::function<void()> notifyWhenResolvedIfNeeded;
    if (shouldNotifyWhenResolved == ShouldNotifyWhenResolved::Yes) {
        notifyWhenResolvedIfNeeded = [connection = m_swConnection, registrationKey = data.key]() mutable {
            callOnMainThread([connection = WTFMove(connection), registrationKey = registrationKey.isolatedCopy()] {
                connection->didResolveRegistrationPromise(registrationKey);
            });
        };
    }

    if (isStopped()) {
        if (notifyWhenResolvedIfNeeded)
            notifyWhenResolvedIfNeeded();
        return;
    }

    if (!job.promise()) {
        if (notifyWhenResolvedIfNeeded)
            notifyWhenResolvedIfNeeded();
        return;
    }

    scriptExecutionContext()->postTask([this, protectedThis = makeRef(*this), job = makeRef(job), data = WTFMove(data), notifyWhenResolvedIfNeeded = WTFMove(notifyWhenResolvedIfNeeded)](ScriptExecutionContext& context) mutable {
        if (isStopped() || !context.sessionID().isValid()) {
            if (notifyWhenResolvedIfNeeded)
                notifyWhenResolvedIfNeeded();
            return;
        }

        auto registration = ServiceWorkerRegistration::getOrCreate(context, *this, WTFMove(data));

        CONTAINER_RELEASE_LOG_IF_ALLOWED("jobResolvedWithRegistration: Resolving promise for job %" PRIu64 ". Registration ID: %" PRIu64, job->identifier().toUInt64(), registration->identifier().toUInt64());

        if (notifyWhenResolvedIfNeeded) {
            job->promise()->whenSettled([notifyWhenResolvedIfNeeded = WTFMove(notifyWhenResolvedIfNeeded)] {
                notifyWhenResolvedIfNeeded();
            });
        }

        job->promise()->resolve<IDLInterface<ServiceWorkerRegistration>>(WTFMove(registration));
    });
}

void ServiceWorkerContainer::jobResolvedWithUnregistrationResult(ServiceWorkerJob& job, bool unregistrationResult)
{
#ifndef NDEBUG
    ASSERT(m_creationThread.ptr() == &Thread::current());
#endif

    ASSERT(job.promise());

    auto guard = WTF::makeScopeExit([this, &job] {
        jobDidFinish(job);
    });

    CONTAINER_RELEASE_LOG_IF_ALLOWED("jobResolvedWithUnregistrationResult: Unregister job %" PRIu64 " finished. Success? %d", job.identifier().toUInt64(), unregistrationResult);

    auto* context = scriptExecutionContext();
    if (!context) {
        LOG_ERROR("ServiceWorkerContainer::jobResolvedWithUnregistrationResult called but the containers ScriptExecutionContext is gone");
        return;
    }

    context->postTask([job = makeRef(job), unregistrationResult](ScriptExecutionContext&) mutable {
        job->promise()->resolve<IDLBoolean>(unregistrationResult);
    });
}

void ServiceWorkerContainer::startScriptFetchForJob(ServiceWorkerJob& job, FetchOptions::Cache cachePolicy)
{
#ifndef NDEBUG
    ASSERT(m_creationThread.ptr() == &Thread::current());
#endif

    CONTAINER_RELEASE_LOG_IF_ALLOWED("startScriptFetchForJob: Starting script fetch for job %" PRIu64, job.identifier().toUInt64());

    auto* context = scriptExecutionContext();
    if (!context) {
        LOG_ERROR("ServiceWorkerContainer::jobResolvedWithRegistration called but the container's ScriptExecutionContext is gone");
        callOnMainThread([connection = m_swConnection, jobIdentifier = job.identifier(), registrationKey = job.data().registrationKey().isolatedCopy(), scriptURL = job.data().scriptURL.isolatedCopy()] {
            connection->failedFetchingScript(jobIdentifier, registrationKey, { errorDomainWebKitInternal, 0, scriptURL, "Attempt to fetch service worker script with no ScriptExecutionContext"_s });
        });
        jobDidFinish(job);
        return;
    }

    job.fetchScriptWithContext(*context, cachePolicy);
}

void ServiceWorkerContainer::jobFinishedLoadingScript(ServiceWorkerJob& job, const String& script, const ContentSecurityPolicyResponseHeaders& contentSecurityPolicy)
{
#ifndef NDEBUG
    ASSERT(m_creationThread.ptr() == &Thread::current());
#endif

    CONTAINER_RELEASE_LOG_IF_ALLOWED("jobFinishedLoadingScript: Successfuly finished fetching script for job %" PRIu64, job.identifier().toUInt64());

    callOnMainThread([connection = m_swConnection, jobDataIdentifier = job.data().identifier(), registrationKey = job.data().registrationKey().isolatedCopy(), script = script.isolatedCopy(), contentSecurityPolicy = contentSecurityPolicy.isolatedCopy()] {
        connection->finishFetchingScriptInServer({ jobDataIdentifier, registrationKey, script, contentSecurityPolicy, { } });
    });
}

void ServiceWorkerContainer::jobFailedLoadingScript(ServiceWorkerJob& job, const ResourceError& error, std::optional<Exception>&& exception)
{
#ifndef NDEBUG
    ASSERT(m_creationThread.ptr() == &Thread::current());
#endif
    ASSERT_WITH_MESSAGE(job.promise() || job.data().type == ServiceWorkerJobType::Update, "Only soft updates have no promise");

    CONTAINER_RELEASE_LOG_ERROR_IF_ALLOWED("jobFinishedLoadingScript: Failed to fetch script for job %" PRIu64 ", error: %s", job.identifier().toUInt64(), error.localizedDescription().utf8().data());

    if (exception && job.promise())
        job.promise()->reject(*exception);

    callOnMainThread([connection = m_swConnection, jobIdentifier = job.identifier(), registrationKey = job.data().registrationKey().isolatedCopy(), error = error.isolatedCopy()] {
        connection->failedFetchingScript(jobIdentifier, registrationKey, error);
    });
}

void ServiceWorkerContainer::jobDidFinish(ServiceWorkerJob& job)
{
#ifndef NDEBUG
    ASSERT(m_creationThread.ptr() == &Thread::current());
#endif

    auto taken = m_jobMap.take(job.identifier());
    ASSERT_UNUSED(taken, !taken || taken->ptr() == &job);

    unsetPendingActivity(this);
}

SWServerConnectionIdentifier ServiceWorkerContainer::connectionIdentifier()
{
    ASSERT(m_swConnection);
    return m_swConnection->serverConnectionIdentifier();
}

const char* ServiceWorkerContainer::activeDOMObjectName() const
{
    return "ServiceWorkerContainer";
}

bool ServiceWorkerContainer::canSuspendForDocumentSuspension() const
{
    return !hasPendingActivity();
}

SWClientConnection& ServiceWorkerContainer::ensureSWClientConnection()
{
    ASSERT(scriptExecutionContext());
    ASSERT(scriptExecutionContext()->sessionID().isValid());
    if (!m_swConnection) {
        ASSERT(scriptExecutionContext());
        callOnMainThreadAndWait([this, sessionID = scriptExecutionContext()->sessionID()]() {
            m_swConnection = &ServiceWorkerProvider::singleton().serviceWorkerConnectionForSession(sessionID);
        });
    }
    return *m_swConnection;
}

void ServiceWorkerContainer::addRegistration(ServiceWorkerRegistration& registration)
{
#ifndef NDEBUG
    ASSERT(m_creationThread.ptr() == &Thread::current());
#endif

    ensureSWClientConnection().addServiceWorkerRegistrationInServer(registration.identifier());
    m_registrations.add(registration.identifier(), &registration);
}

void ServiceWorkerContainer::removeRegistration(ServiceWorkerRegistration& registration)
{
#ifndef NDEBUG
    ASSERT(m_creationThread.ptr() == &Thread::current());
#endif

    m_swConnection->removeServiceWorkerRegistrationInServer(registration.identifier());
    m_registrations.remove(registration.identifier());
}

void ServiceWorkerContainer::scheduleTaskToFireControllerChangeEvent()
{
#ifndef NDEBUG
    ASSERT(m_creationThread.ptr() == &Thread::current());
#endif

    if (m_isStopped)
        return;

    scriptExecutionContext()->postTask([this, protectedThis = makeRef(*this)](ScriptExecutionContext&) mutable {
        if (m_isStopped)
            return;

        dispatchEvent(Event::create(eventNames().controllerchangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
    });
}

void ServiceWorkerContainer::stop()
{
    m_isStopped = true;
    removeAllEventListeners();
    m_pendingPromises.clear();
    for (auto& job : m_jobMap.values())
        job->cancelPendingLoad();
}

DocumentOrWorkerIdentifier ServiceWorkerContainer::contextIdentifier()
{
#ifndef NDEBUG
    ASSERT(m_creationThread.ptr() == &Thread::current());
#endif

    ASSERT(scriptExecutionContext());
    if (is<ServiceWorkerGlobalScope>(*scriptExecutionContext()))
        return downcast<ServiceWorkerGlobalScope>(*scriptExecutionContext()).thread().identifier();
    return downcast<Document>(*scriptExecutionContext()).identifier();
}

bool ServiceWorkerContainer::isAlwaysOnLoggingAllowed() const
{
    auto* context = scriptExecutionContext();
    if (!context)
        return false;

    if (is<Document>(*context))
        return downcast<Document>(*context).sessionID().isAlwaysOnLoggingAllowed();

    // FIXME: No logging inside service workers for now.
    return false;
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
