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
#include "MessageEvent.h"
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
#include <wtf/IsoMallocInlines.h>
#include <wtf/RunLoop.h>
#include <wtf/Scope.h>
#include <wtf/URL.h>

#define CONTAINER_RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), ServiceWorker, "%p - ServiceWorkerContainer::" fmt, this, ##__VA_ARGS__)
#define CONTAINER_RELEASE_LOG_ERROR_IF_ALLOWED(fmt, ...) RELEASE_LOG_ERROR_IF(isAlwaysOnLoggingAllowed(), ServiceWorker, "%p - ServiceWorkerContainer::" fmt, this, ##__VA_ARGS__)

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ServiceWorkerContainer);

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
        m_readyPromise = makeUnique<ReadyPromise>();

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

    scheduleJob(makeUnique<ServiceWorkerJob>(*this, WTFMove(promise), WTFMove(jobData)));
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

    scheduleJob(makeUnique<ServiceWorkerJob>(*this, WTFMove(promise), WTFMove(jobData)));
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

    scheduleJob(makeUnique<ServiceWorkerJob>(*this, WTFMove(promise), WTFMove(jobData)));
}

void ServiceWorkerContainer::scheduleJob(std::unique_ptr<ServiceWorkerJob>&& job)
{
#ifndef NDEBUG
    ASSERT(m_creationThread.ptr() == &Thread::current());
#endif

    ASSERT(m_swConnection);
    ASSERT(!isStopped());

    auto& jobData = job->data();
    auto jobIdentifier = job->identifier();
    ASSERT(!m_jobMap.contains(jobIdentifier));
    m_jobMap.add(jobIdentifier, OngoingJob { WTFMove(job), makePendingActivity(*this) });

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
    auto pendingPromise = makeUnique<PendingPromise>(WTFMove(promise), makePendingActivity(*this));
    m_pendingPromises.add(pendingPromiseIdentifier, WTFMove(pendingPromise));

    auto contextIdentifier = this->contextIdentifier();
    callOnMainThread([connection = makeRef(ensureSWClientConnection()), topOrigin = context->topOrigin().data().isolatedCopy(), parsedURL = parsedURL.isolatedCopy(), contextIdentifier, pendingPromiseIdentifier]() mutable {
        connection->matchRegistration(WTFMove(topOrigin), parsedURL, [contextIdentifier, pendingPromiseIdentifier] (auto&& result) mutable {
            ScriptExecutionContext::postTaskTo(contextIdentifier, [pendingPromiseIdentifier, result = crossThreadCopy(result)](auto& context) mutable {
                auto* serviceWorkerContainer = context.serviceWorkerContainer();
                if (!serviceWorkerContainer)
                    return;
                if (serviceWorkerContainer->m_isStopped || !context.sessionID().isValid())
                    return;

                serviceWorkerContainer->didFinishGetRegistrationRequest(pendingPromiseIdentifier, WTFMove(result));
            });
        });
    });
}

void ServiceWorkerContainer::didFinishGetRegistrationRequest(uint64_t pendingPromiseIdentifier, Optional<ServiceWorkerRegistrationData>&& result)
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

void ServiceWorkerContainer::updateRegistrationState(ServiceWorkerRegistrationIdentifier identifier, ServiceWorkerRegistrationState state, const Optional<ServiceWorkerData>& serviceWorkerData)
{
    if (m_isStopped)
        return;

    RefPtr<ServiceWorker> serviceWorker;
    if (serviceWorkerData)
        serviceWorker = ServiceWorker::getOrCreate(*scriptExecutionContext(), ServiceWorkerData { *serviceWorkerData });

    if (auto* registration = m_registrations.get(identifier))
        registration->updateStateFromServer(state, WTFMove(serviceWorker));
}

void ServiceWorkerContainer::getRegistrations(Ref<DeferredPromise>&& promise)
{
    auto* context = scriptExecutionContext();
    if (m_isStopped || !context->sessionID().isValid()) {
        promise->reject(Exception { InvalidStateError });
        return;
    }

    uint64_t pendingPromiseIdentifier = ++m_lastPendingPromiseIdentifier;
    auto pendingPromise = makeUnique<PendingPromise>(WTFMove(promise), makePendingActivity(*this));
    m_pendingPromises.add(pendingPromiseIdentifier, WTFMove(pendingPromise));

    auto contextIdentifier = this->contextIdentifier();
    auto contextURL = context->url();
    callOnMainThread([connection = makeRef(ensureSWClientConnection()), topOrigin = context->topOrigin().data().isolatedCopy(), contextURL = contextURL.isolatedCopy(), contextIdentifier, pendingPromiseIdentifier]() mutable {
        connection->getRegistrations(WTFMove(topOrigin), contextURL, [contextIdentifier, pendingPromiseIdentifier] (auto&& registrationDatas) mutable {
            ScriptExecutionContext::postTaskTo(contextIdentifier, [pendingPromiseIdentifier, registrationDatas = crossThreadCopy(registrationDatas)](auto& context) mutable {
                auto* serviceWorkerContainer = context.serviceWorkerContainer();
                if (!serviceWorkerContainer)
                    return;
                if (serviceWorkerContainer->m_isStopped || !context.sessionID().isValid())
                    return;

                serviceWorkerContainer->didFinishGetRegistrationsRequest(pendingPromiseIdentifier, WTFMove(registrationDatas));
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

    ASSERT_WITH_MESSAGE(job.hasPromise() || job.data().type == ServiceWorkerJobType::Update, "Only soft updates have no promise");

    auto guard = WTF::makeScopeExit([this, &job] {
        destroyJob(job);
    });

    CONTAINER_RELEASE_LOG_ERROR_IF_ALLOWED("jobFailedWithException: Job %" PRIu64 " failed with error %s", job.identifier().toUInt64(), exception.message().utf8().data());

    auto promise = job.takePromise();
    if (!promise)
        return;

    if (auto* context = scriptExecutionContext()) {
        context->postTask([promise = WTFMove(promise), exception](auto&) mutable {
            promise->reject(exception);
        });
    }
}

void ServiceWorkerContainer::fireUpdateFoundEvent(ServiceWorkerRegistrationIdentifier identifier)
{
#ifndef NDEBUG
    ASSERT(m_creationThread.ptr() == &Thread::current());
#endif

    if (auto* registration = m_registrations.get(identifier))
        registration->fireUpdateFoundEvent();
}

void ServiceWorkerContainer::jobResolvedWithRegistration(ServiceWorkerJob& job, ServiceWorkerRegistrationData&& data, ShouldNotifyWhenResolved shouldNotifyWhenResolved)
{
#ifndef NDEBUG
    ASSERT(m_creationThread.ptr() == &Thread::current());
#endif
    ASSERT_WITH_MESSAGE(job.hasPromise() || job.data().type == ServiceWorkerJobType::Update, "Only soft updates have no promise");

    if (job.data().type == ServiceWorkerJobType::Register)
        CONTAINER_RELEASE_LOG_IF_ALLOWED("jobResolvedWithRegistration: Registration job %" PRIu64 " succeeded", job.identifier().toUInt64());
    else {
        ASSERT(job.data().type == ServiceWorkerJobType::Update);
        CONTAINER_RELEASE_LOG_IF_ALLOWED("jobResolvedWithRegistration: Update job %" PRIu64 " succeeded", job.identifier().toUInt64());
    }

    auto guard = WTF::makeScopeExit([this, &job] {
        destroyJob(job);
    });

    auto notifyIfExitEarly = WTF::makeScopeExit([this, &data, &shouldNotifyWhenResolved] {
        if (shouldNotifyWhenResolved == ShouldNotifyWhenResolved::Yes)
            notifyRegistrationIsSettled(data.key);
    });

    if (isStopped())
        return;

    auto promise = job.takePromise();
    if (!promise)
        return;

    notifyIfExitEarly.release();

    scriptExecutionContext()->postTask([this, protectedThis = RefPtr<ServiceWorkerContainer>(this), promise = WTFMove(promise), jobIdentifier = job.identifier(), data = WTFMove(data), shouldNotifyWhenResolved](ScriptExecutionContext& context) mutable {
        if (isStopped() || !context.sessionID().isValid()) {
            if (shouldNotifyWhenResolved == ShouldNotifyWhenResolved::Yes)
                notifyRegistrationIsSettled(data.key);
            return;
        }

        auto registration = ServiceWorkerRegistration::getOrCreate(context, *this, WTFMove(data));

        CONTAINER_RELEASE_LOG_IF_ALLOWED("jobResolvedWithRegistration: Resolving promise for job %" PRIu64 ". Registration ID: %" PRIu64, jobIdentifier.toUInt64(), registration->identifier().toUInt64());

        if (shouldNotifyWhenResolved == ShouldNotifyWhenResolved::Yes) {
            m_ongoingSettledRegistrations.add(++m_lastOngoingSettledRegistrationIdentifier, registration->data().key);
            promise->whenSettled([this, protectedThis = WTFMove(protectedThis), identifier = m_lastOngoingSettledRegistrationIdentifier] {
                notifyRegistrationIsSettled(m_ongoingSettledRegistrations.take(identifier));
            });
        }

        promise->resolve<IDLInterface<ServiceWorkerRegistration>>(WTFMove(registration));
    });
}

void ServiceWorkerContainer::postMessage(MessageWithMessagePorts&& message, ServiceWorkerData&& sourceData, String&& sourceOrigin)
{
    auto& context = *scriptExecutionContext();
    MessageEventSource source = RefPtr<ServiceWorker> { ServiceWorker::getOrCreate(context, WTFMove(sourceData)) };

    auto messageEvent = MessageEvent::create(MessagePort::entanglePorts(context, WTFMove(message.transferredPorts)), message.message.releaseNonNull(), sourceOrigin, { }, WTFMove(source));
    dispatchEvent(messageEvent);
}

void ServiceWorkerContainer::notifyRegistrationIsSettled(const ServiceWorkerRegistrationKey& registrationKey)
{
    callOnMainThread([connection = m_swConnection, registrationKey = registrationKey.isolatedCopy()] {
        connection->didResolveRegistrationPromise(registrationKey);
    });
}

void ServiceWorkerContainer::jobResolvedWithUnregistrationResult(ServiceWorkerJob& job, bool unregistrationResult)
{
#ifndef NDEBUG
    ASSERT(m_creationThread.ptr() == &Thread::current());
#endif

    ASSERT(job.hasPromise());

    auto guard = WTF::makeScopeExit([this, &job] {
        destroyJob(job);
    });

    CONTAINER_RELEASE_LOG_IF_ALLOWED("jobResolvedWithUnregistrationResult: Unregister job %" PRIu64 " finished. Success? %d", job.identifier().toUInt64(), unregistrationResult);

    auto* context = scriptExecutionContext();
    if (!context) {
        LOG_ERROR("ServiceWorkerContainer::jobResolvedWithUnregistrationResult called but the containers ScriptExecutionContext is gone");
        return;
    }

    context->postTask([promise = job.takePromise(), unregistrationResult](auto&) mutable {
        promise->resolve<IDLBoolean>(unregistrationResult);
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
        notifyFailedFetchingScript(job, { errorDomainWebKitInternal, 0, job.data().scriptURL, "Attempt to fetch service worker script with no ScriptExecutionContext"_s });
        destroyJob(job);
        return;
    }

    job.fetchScriptWithContext(*context, cachePolicy);
}

void ServiceWorkerContainer::jobFinishedLoadingScript(ServiceWorkerJob& job, const String& script, const ContentSecurityPolicyResponseHeaders& contentSecurityPolicy, const String& referrerPolicy)
{
#ifndef NDEBUG
    ASSERT(m_creationThread.ptr() == &Thread::current());
#endif

    CONTAINER_RELEASE_LOG_IF_ALLOWED("jobFinishedLoadingScript: Successfuly finished fetching script for job %" PRIu64, job.identifier().toUInt64());

    callOnMainThread([connection = m_swConnection, jobDataIdentifier = job.data().identifier(), registrationKey = job.data().registrationKey().isolatedCopy(), script = script.isolatedCopy(), contentSecurityPolicy = contentSecurityPolicy.isolatedCopy(), referrerPolicy = referrerPolicy.isolatedCopy()] {
        connection->finishFetchingScriptInServer({ jobDataIdentifier, registrationKey, script, contentSecurityPolicy, referrerPolicy, { } });
    });
}

void ServiceWorkerContainer::jobFailedLoadingScript(ServiceWorkerJob& job, const ResourceError& error, Exception&& exception)
{
#ifndef NDEBUG
    ASSERT(m_creationThread.ptr() == &Thread::current());
#endif
    ASSERT_WITH_MESSAGE(job.hasPromise() || job.data().type == ServiceWorkerJobType::Update, "Only soft updates have no promise");

    CONTAINER_RELEASE_LOG_ERROR_IF_ALLOWED("jobFinishedLoadingScript: Failed to fetch script for job %" PRIu64 ", error: %s", job.identifier().toUInt64(), error.localizedDescription().utf8().data());

    if (auto promise = job.takePromise())
        promise->reject(WTFMove(exception));

    notifyFailedFetchingScript(job, error);
    destroyJob(job);
}

void ServiceWorkerContainer::notifyFailedFetchingScript(ServiceWorkerJob& job, const ResourceError& error)
{
    callOnMainThread([connection = m_swConnection, jobIdentifier = job.identifier(), registrationKey = job.data().registrationKey().isolatedCopy(), error = error.isolatedCopy()] {
        connection->failedFetchingScript(jobIdentifier, registrationKey, error);
    });
}

void ServiceWorkerContainer::destroyJob(ServiceWorkerJob& job)
{
#ifndef NDEBUG
    ASSERT(m_creationThread.ptr() == &Thread::current());
#endif

    ASSERT(m_jobMap.contains(job.identifier()));
    m_jobMap.remove(job.identifier());
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

void ServiceWorkerContainer::fireControllerChangeEvent()
{
#ifndef NDEBUG
    ASSERT(m_creationThread.ptr() == &Thread::current());
#endif

    if (m_isStopped)
        return;

    dispatchEvent(Event::create(eventNames().controllerchangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void ServiceWorkerContainer::stop()
{
    m_isStopped = true;
    removeAllEventListeners();
    m_pendingPromises.clear();
    m_readyPromise = nullptr;
    auto jobMap = WTFMove(m_jobMap);
    for (auto& ongoingJob : jobMap.values()) {
        if (ongoingJob.job->cancelPendingLoad())
            notifyFailedFetchingScript(*ongoingJob.job.get(), ResourceError { errorDomainWebKitInternal, 0, ongoingJob.job->data().scriptURL, "Job cancelled"_s, ResourceError::Type::Cancellation });
    }

    auto registrationMap = WTFMove(m_ongoingSettledRegistrations);
    for (auto& registration : registrationMap.values())
        notifyRegistrationIsSettled(registration);
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

ServiceWorkerJob* ServiceWorkerContainer::job(ServiceWorkerJobIdentifier identifier)
{
    auto iterator = m_jobMap.find(identifier);
    if (iterator == m_jobMap.end())
        return nullptr;
    return iterator->value.job.get();
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
