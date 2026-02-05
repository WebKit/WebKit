/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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

#include "ContentSecurityPolicy.h"
#include "ContextDestructionObserverInlines.h"
#include "CookieChangeSubscription.h"
#include "CookieStoreGetOptions.h"
#include "DOMPromiseProxy.h"
#include "DedicatedWorkerGlobalScope.h"
#include "DocumentPage.h"
#include "Event.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "Exception.h"
#include "FrameLoader.h"
#include "IDLTypes.h"
#include "JSCookieStoreGetOptions.h"
#include "JSDOMPromiseDeferred.h"
#include "JSNavigationPreloadState.h"
#include "JSPushSubscription.h"
#include "JSServiceWorkerRegistration.h"
#include "LegacySchemeRegistry.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "Logging.h"
#include "NavigatorBase.h"
#include "PushSubscriptionOptions.h"
#include "ResourceError.h"
#include "ScriptExecutionContext.h"
#include "ScriptExecutionContextInlines.h"
#include "SecurityOrigin.h"
#include "ServiceWorker.h"
#include "ServiceWorkerGlobalScope.h"
#include "ServiceWorkerJob.h"
#include "ServiceWorkerJobData.h"
#include "ServiceWorkerProvider.h"
#include "ServiceWorkerThread.h"
#include "SharedWorkerGlobalScope.h"
#include "TrustedType.h"
#include "WorkerFetchResult.h"
#include "WorkerSWClientConnection.h"
#include <wtf/Ref.h>
#include <wtf/RunLoop.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>

#define CONTAINER_RELEASE_LOG(fmt, ...) RELEASE_LOG(ServiceWorker, "%p - ServiceWorkerContainer::" fmt, this, ##__VA_ARGS__)
#define CONTAINER_RELEASE_LOG_WITH_THIS(thisPtr, fmt, ...) RELEASE_LOG(ServiceWorker, "%p - ServiceWorkerContainer::" fmt, thisPtr, ##__VA_ARGS__)
#define CONTAINER_RELEASE_LOG_ERROR(fmt, ...) RELEASE_LOG_ERROR(ServiceWorker, "%p - ServiceWorkerContainer::" fmt, this, ##__VA_ARGS__)

namespace WebCore {

static inline SWClientConnection& mainThreadConnection()
{
    return ServiceWorkerProvider::singleton().serviceWorkerConnection();
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(ServiceWorkerContainer);

UniqueRef<ServiceWorkerContainer> ServiceWorkerContainer::create(ScriptExecutionContext* context, NavigatorBase& navigator)
{
    auto result = UniqueRef(*new ServiceWorkerContainer(context, navigator));
    result->suspendIfNeeded();
    return result;
}

ServiceWorkerContainer::ServiceWorkerContainer(ScriptExecutionContext* context, NavigatorBase& navigator)
    : ActiveDOMObject(context)
    , m_navigator(navigator)
{
    // We should queue messages until the DOMContentLoaded event has fired or startMessages() has been called.
    if (RefPtr document = dynamicDowncast<Document>(context); document && document->parsing())
        m_shouldDeferMessageEvents = true;
}

ServiceWorkerContainer::~ServiceWorkerContainer()
{
    ASSERT(m_creationThread.ptr() == &Thread::currentSingleton());
}

ScriptExecutionContext* ServiceWorkerContainer::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

void ServiceWorkerContainer::refEventTarget()
{
    m_navigator->ref();
}

void ServiceWorkerContainer::derefEventTarget()
{
    m_navigator->deref();
}

void ServiceWorkerContainer::ref() const
{
    m_navigator->ref();
}

void ServiceWorkerContainer::deref() const
{
    m_navigator->deref();
}

auto ServiceWorkerContainer::ready() -> ReadyPromise&
{
    if (!m_readyPromise) {
        m_readyPromise = makeUnique<ReadyPromise>();

        if (m_isStopped)
            return *m_readyPromise;

        Ref context = *scriptExecutionContext();
        ensureProtectedSWClientConnection()->whenRegistrationReady(context->topOrigin().data(), context->url(), [this, protectedThis = Ref { *this }](ServiceWorkerRegistrationData&& registrationData) mutable {
            queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [registrationData = WTF::move(registrationData)](ServiceWorkerContainer& container) mutable {
                RefPtr context = container.scriptExecutionContext();
                if (!context || !container.m_readyPromise)
                    return;
                Ref registration = ServiceWorkerRegistration::getOrCreate(*context, container, WTF::move(registrationData));
                container.m_readyPromise->resolve(WTF::move(registration));
            });
        });
    }
    return *m_readyPromise;
}

ServiceWorker* ServiceWorkerContainer::controller() const
{
    auto* context = scriptExecutionContext();
    ASSERT_WITH_MESSAGE(!context || is<Document>(*context) || is<DedicatedWorkerGlobalScope>(*context)  || is<SharedWorkerGlobalScope>(*context) || !context->activeServiceWorker(), "Only documents, dedicated and shared workers can have a controller.");
    return context ? context->activeServiceWorker() : nullptr;
}

void ServiceWorkerContainer::addRegistration(Variant<RefPtr<TrustedScriptURL>, String>&& relativeScriptURL, const RegistrationOptions& options, Ref<DeferredPromise>&& promise)
{
    auto stringValueHolder = trustedTypeCompliantString(*protectedScriptExecutionContext(), WTF::move(relativeScriptURL), "ServiceWorkerContainer register"_s);

    if (stringValueHolder.hasException()) {
        promise->reject(stringValueHolder.releaseException());
        return;
    }

    auto trustedRelativeScriptURL = stringValueHolder.releaseReturnValue();

    if (m_isStopped) {
        promise->reject(Exception(ExceptionCode::InvalidStateError));
        return;
    }

    if (trustedRelativeScriptURL.isEmpty()) {
        promise->reject(Exception { ExceptionCode::TypeError, "serviceWorker.register() cannot be called with an empty script URL"_s });
        return;
    }

    ServiceWorkerJobData jobData(ensureProtectedSWClientConnection()->serverConnectionIdentifier(), contextIdentifier());

    Ref context = *scriptExecutionContext();
    jobData.scriptURL = context->completeURL(trustedRelativeScriptURL);

    RefPtr document = dynamicDowncast<Document>(context);
    CheckedPtr contentSecurityPolicy = document ? document->contentSecurityPolicy() : nullptr;
    if (contentSecurityPolicy && !contentSecurityPolicy->allowWorkerFromSource(jobData.scriptURL)) {
        promise->reject(Exception { ExceptionCode::SecurityError });
        return;
    }

    if (!jobData.scriptURL.isValid()) {
        CONTAINER_RELEASE_LOG_ERROR("addRegistration: Invalid scriptURL");
        promise->reject(Exception { ExceptionCode::TypeError, "serviceWorker.register() must be called with a valid relative script URL"_s });
        return;
    }

    RefPtr page = document ? document->page() : nullptr;
    jobData.isFromServiceWorkerPage = page && page->isServiceWorkerPage();
    if (!jobData.scriptURL.protocolIsInHTTPFamily() && !jobData.isFromServiceWorkerPage) {
        CONTAINER_RELEASE_LOG_ERROR("addRegistration: Invalid scriptURL scheme is not HTTP or HTTPS");
        promise->reject(Exception { ExceptionCode::TypeError, "serviceWorker.register() must be called with a script URL whose protocol is either HTTP or HTTPS"_s });
        return;
    }

    auto path = jobData.scriptURL.path();
    if (path.containsIgnoringASCIICase("%2f"_s) || path.containsIgnoringASCIICase("%5c"_s)) {
        CONTAINER_RELEASE_LOG_ERROR("addRegistration: scriptURL contains invalid character");
        promise->reject(Exception { ExceptionCode::TypeError, "serviceWorker.register() must be called with a script URL whose path does not contain '%2f' or '%5c'"_s });
        return;
    }

    if (!options.scope.isEmpty())
        jobData.scopeURL = context->completeURL(options.scope);
    else
        jobData.scopeURL = URL(jobData.scriptURL, "./"_s);

    if (!jobData.scopeURL.isNull() && !jobData.scopeURL.protocolIsInHTTPFamily() && !jobData.isFromServiceWorkerPage) {
        CONTAINER_RELEASE_LOG_ERROR("addRegistration: scopeURL scheme is not HTTP or HTTPS");
        promise->reject(Exception { ExceptionCode::TypeError, "Scope URL provided to serviceWorker.register() must be either HTTP or HTTPS"_s });
        return;
    }

    path = jobData.scopeURL.path();
    if (path.containsIgnoringASCIICase("%2f"_s) || path.containsIgnoringASCIICase("%5c"_s)) {
        CONTAINER_RELEASE_LOG_ERROR("addRegistration: scopeURL contains invalid character");
        promise->reject(Exception { ExceptionCode::TypeError, "Scope URL provided to serviceWorker.register() cannot have a path that contains '%2f' or '%5c'"_s });
        return;
    }

    CONTAINER_RELEASE_LOG("addRegistration: Registering service worker. jobID=%" PRIu64, jobData.identifier().jobIdentifier.toUInt64());

    jobData.clientCreationURL = context->url();
    jobData.topOrigin = context->topOrigin().data();
    jobData.workerType = options.type;
    jobData.type = ServiceWorkerJobType::Register;
    jobData.domainForCachePartition = context->domainForCachePartition();
    jobData.registrationOptions = options;

    scheduleJob(ServiceWorkerJob::create(*this, WTF::move(promise), WTF::move(jobData)));
}

void ServiceWorkerContainer::willSettleRegistrationPromise(bool success)
{
    RefPtr document = dynamicDowncast<Document>(scriptExecutionContext());
    RefPtr page = document ? document->page() : nullptr;
    if (!page || !page->isServiceWorkerPage())
        return;
    
    RefPtr localMainFrame = page->localMainFrame();
    if (!localMainFrame)
        return;

    localMainFrame->loader().client().didFinishServiceWorkerPageRegistration(success);
}

void ServiceWorkerContainer::unregisterRegistration(ServiceWorkerRegistrationIdentifier registrationIdentifier, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    ASSERT(!m_isStopped);
    RefPtr swConnection = m_swConnection;
    if (!swConnection) {
        ASSERT_NOT_REACHED();
        promise.reject(Exception(ExceptionCode::InvalidStateError));
        return;
    }

    CONTAINER_RELEASE_LOG("unregisterRegistration: Unregistering service worker.");
    swConnection->scheduleUnregisterJobInServer(registrationIdentifier, contextIdentifier(), [promise = WTF::move(promise)](auto&& result) mutable {
        promise.settle(WTF::move(result));
    });
}

void ServiceWorkerContainer::updateRegistration(const URL& scopeURL, const URL& scriptURL, WorkerType workerType, Ref<DeferredPromise>&& promise)
{
    ASSERT(!m_isStopped);

    Ref context = *scriptExecutionContext();

    RefPtr swConnection = m_swConnection;
    if (!swConnection) {
        ASSERT_NOT_REACHED();
        promise->reject(Exception(ExceptionCode::InvalidStateError));
        return;
    }

    ServiceWorkerJobData jobData(swConnection->serverConnectionIdentifier(), contextIdentifier());
    jobData.clientCreationURL = context->url();
    jobData.topOrigin = context->topOrigin().data();
    jobData.workerType = workerType;
    jobData.type = ServiceWorkerJobType::Update;
    jobData.domainForCachePartition = context->domainForCachePartition();
    jobData.scopeURL = scopeURL;
    jobData.scriptURL = scriptURL;

    CONTAINER_RELEASE_LOG("removeRegistration: Updating service worker. jobID=%" PRIu64, jobData.identifier().jobIdentifier.toUInt64());

    scheduleJob(ServiceWorkerJob::create(*this, WTF::move(promise), WTF::move(jobData)));
}

void ServiceWorkerContainer::scheduleJob(Ref<ServiceWorkerJob>&& job)
{
    ASSERT(m_creationThread.ptr() == &Thread::currentSingleton());
    RefPtr swConnection = m_swConnection;
    ASSERT(swConnection);
    ASSERT(!isStopped());

    auto& jobData = job->data();
    auto jobIdentifier = job->identifier();
    ASSERT(!m_jobMap.contains(jobIdentifier));
    m_jobMap.add(jobIdentifier, OngoingJob { WTF::move(job), makePendingActivity(*this) });

    swConnection->scheduleJob(contextIdentifier(), jobData);
}

void ServiceWorkerContainer::getRegistration(const String& clientURL, Ref<DeferredPromise>&& promise)
{
    if (m_isStopped) {
        promise->reject(Exception { ExceptionCode::InvalidStateError });
        return;
    }

    Ref context = *scriptExecutionContext();
    URL parsedURL = context->completeURL(clientURL);
    if (!protocolHostAndPortAreEqual(parsedURL, context->url())) {
        promise->reject(Exception { ExceptionCode::SecurityError, "Origin of clientURL is not client's origin"_s });
        return;
    }

    ensureProtectedSWClientConnection()->matchRegistration(SecurityOriginData { context->topOrigin().data() }, parsedURL, [this, protectedThis = Ref { *this }, promise = WTF::move(promise)](std::optional<ServiceWorkerRegistrationData>&& result) mutable {
        queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [promise = WTF::move(promise), result = WTF::move(result)](ServiceWorkerContainer& container) mutable {
            if (!result) {
                promise->resolve();
                return;
            }
            promise->resolve<IDLInterface<ServiceWorkerRegistration>>(ServiceWorkerRegistration::getOrCreate(*container.protectedScriptExecutionContext(), container, WTF::move(result.value())));
        });
    });
}

void ServiceWorkerContainer::updateRegistrationState(ServiceWorkerRegistrationIdentifier identifier, ServiceWorkerRegistrationState state, const std::optional<ServiceWorkerData>& serviceWorkerData)
{
    if (m_isStopped)
        return;

    queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [identifier, state, serviceWorkerData = std::optional<ServiceWorkerData> { serviceWorkerData }](ServiceWorkerContainer& container) mutable {
        RefPtr<ServiceWorker> serviceWorker;
        if (serviceWorkerData)
            serviceWorker = ServiceWorker::getOrCreate(*container.protectedScriptExecutionContext(), WTF::move(*serviceWorkerData));

        if (RefPtr registration = container.m_registrations.get(identifier))
            registration->updateStateFromServer(state, WTF::move(serviceWorker));
    });
}

void ServiceWorkerContainer::updateWorkerState(ServiceWorkerIdentifier identifier, ServiceWorkerState state)
{
    if (m_isStopped)
        return;

    queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [identifier, state](auto& container) {
        if (RefPtr serviceWorker = container.scriptExecutionContext()->serviceWorker(identifier))
            serviceWorker->updateState(state);
    });
}

void ServiceWorkerContainer::getRegistrations(Ref<DeferredPromise>&& promise)
{
    if (m_isStopped) {
        promise->reject(Exception { ExceptionCode::InvalidStateError });
        return;
    }

    Ref context = *scriptExecutionContext();
    ensureProtectedSWClientConnection()->getRegistrations(SecurityOriginData { context->topOrigin().data() }, context->url(), [this, protectedThis = Ref { *this }, promise = WTF::move(promise)] (Vector<ServiceWorkerRegistrationData>&& registrationDatas) mutable {
        queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [promise = WTF::move(promise), registrationDatas = WTF::move(registrationDatas)](ServiceWorkerContainer& container) mutable {
            auto registrations = WTF::map(WTF::move(registrationDatas), [&](auto&& registrationData) {
                return ServiceWorkerRegistration::getOrCreate(*container.protectedScriptExecutionContext(), container, WTF::move(registrationData));
            });
            promise->resolve<IDLSequence<IDLInterface<ServiceWorkerRegistration>>>(WTF::move(registrations));
        });
    });
}

void ServiceWorkerContainer::startMessages()
{
    if (!context()) {
        CONTAINER_RELEASE_LOG_ERROR("Container without ScriptExecutionContext is attempting to start post message delivery");
        return;
    }

    m_shouldDeferMessageEvents = false;

    for (auto&& messageEvent : std::exchange(m_deferredMessageEvents, Vector<MessageEvent::MessageEventWithStrongData> { })) {
        queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [messageEvent = WTF::move(messageEvent)](auto& container) {
            container.dispatchEvent(messageEvent.event);
        });
    }
}

void ServiceWorkerContainer::jobFailedWithException(ServiceWorkerJob& job, const Exception& exception)
{
    ASSERT(m_creationThread.ptr() == &Thread::currentSingleton());

    auto guard = makeScopeExit([this, protectedThis = Ref { *this }, job = Ref { job }] {
        destroyJob(job);
    });

    CONTAINER_RELEASE_LOG_ERROR("jobFailedWithException: Job %" PRIu64 " failed with error %s", job.identifier().toUInt64(), exception.message().utf8().data());

    if (job.data().type == ServiceWorkerJobType::Register)
        willSettleRegistrationPromise(false);

    queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [promise = job.takePromise(), exception](auto&) mutable {
        promise->reject(exception);
    });
}

void ServiceWorkerContainer::queueTaskToFireUpdateFoundEvent(ServiceWorkerRegistrationIdentifier identifier)
{
    ASSERT(m_creationThread.ptr() == &Thread::currentSingleton());

    if (RefPtr registration = m_registrations.get(identifier))
        registration->queueTaskToFireUpdateFoundEvent();
}

void ServiceWorkerContainer::jobResolvedWithRegistration(ServiceWorkerJob& job, ServiceWorkerRegistrationData&& data, ShouldNotifyWhenResolved shouldNotifyWhenResolved)
{
    ASSERT(m_creationThread.ptr() == &Thread::currentSingleton());

    if (job.data().type == ServiceWorkerJobType::Register) {
        CONTAINER_RELEASE_LOG("jobResolvedWithRegistration: Registration job %" PRIu64 " succeeded", job.identifier().toUInt64());
        willSettleRegistrationPromise(true);
    } else {
        ASSERT(job.data().type == ServiceWorkerJobType::Update);
        CONTAINER_RELEASE_LOG("jobResolvedWithRegistration: Update job %" PRIu64 " succeeded", job.identifier().toUInt64());
    }

    auto guard = makeScopeExit([this, protectedThis = Ref { *this }, job = Ref { job }] {
        destroyJob(job);
    });

    auto notifyIfExitEarly = makeScopeExit([this, protectedThis = Ref { *this }, key = data.key, shouldNotifyWhenResolved] {
        if (shouldNotifyWhenResolved == ShouldNotifyWhenResolved::Yes)
            notifyRegistrationIsSettled(key);
    });

    if (isStopped())
        return;

    queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [promise = job.takePromise(), jobIdentifier = job.identifier(), data = WTF::move(data), shouldNotifyWhenResolved, notifyIfExitEarly = WTF::move(notifyIfExitEarly)](ServiceWorkerContainer& container) mutable {
        notifyIfExitEarly.release();

        Ref registration = ServiceWorkerRegistration::getOrCreate(*container.protectedScriptExecutionContext(), container, WTF::move(data));

        CONTAINER_RELEASE_LOG_WITH_THIS(&container, "jobResolvedWithRegistration: Resolving promise for job %" PRIu64 ". registrationID=%" PRIu64, jobIdentifier.toUInt64(), registration->identifier().toUInt64());

        if (shouldNotifyWhenResolved == ShouldNotifyWhenResolved::Yes) {
            container.m_ongoingSettledRegistrations.add(++container.m_lastOngoingSettledRegistrationIdentifier, registration->data().key);
            promise->whenSettled([container = Ref { container }, identifier = container.m_lastOngoingSettledRegistrationIdentifier] {
                auto iterator = container->m_ongoingSettledRegistrations.find(identifier);
                if (iterator == container->m_ongoingSettledRegistrations.end())
                    return;
                container->notifyRegistrationIsSettled(iterator->value);
                container->m_ongoingSettledRegistrations.remove(iterator);
            });
            if (promise->needsAbort()) [[unlikely]]
                return;
        }

        promise->resolve<IDLInterface<ServiceWorkerRegistration>>(WTF::move(registration));
    });
}

void ServiceWorkerContainer::postMessage(MessageWithMessagePorts&& message, ServiceWorkerData&& sourceData, Ref<SecurityOrigin>&& sourceOrigin)
{
    Ref context = *scriptExecutionContext();
    if (context->isJSExecutionForbidden()) [[unlikely]]
        return;

    auto* globalObject = context->globalObject();
    if (!globalObject)
        return;

    auto& vm = globalObject->vm();
    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);

    MessageEventSource source = RefPtr<ServiceWorker> { ServiceWorker::getOrCreate(context.get(), WTF::move(sourceData)) };

    auto messageEvent = MessageEvent::create(*globalObject, message.message.releaseNonNull(), WTF::move(sourceOrigin), { }, WTF::move(source), MessagePort::entanglePorts(context.get(), WTF::move(message.transferredPorts)));
    if (scope.exception()) [[unlikely]] {
        // Currently, we assume that the only way we can get here is if we have a termination.
        RELEASE_ASSERT(vm.hasPendingTerminationException());
        return;
    }

    if (m_shouldDeferMessageEvents)
        m_deferredMessageEvents.append(WTF::move(messageEvent));
    else {
        ASSERT(m_deferredMessageEvents.isEmpty());
        queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [messageEvent = WTF::move(messageEvent)](auto& container) {
            container.dispatchEvent(messageEvent.event);
        });
    }
}

void ServiceWorkerContainer::notifyRegistrationIsSettled(const ServiceWorkerRegistrationKey& registrationKey)
{
    ensureProtectedSWClientConnection()->didResolveRegistrationPromise(registrationKey);
}

void ServiceWorkerContainer::jobResolvedWithUnregistrationResult(ServiceWorkerJob& job, bool unregistrationResult)
{
    ASSERT(m_creationThread.ptr() == &Thread::currentSingleton());

    auto guard = makeScopeExit([this, protectedThis = Ref { *this }, job = Ref { job }] {
        destroyJob(job);
    });

    CONTAINER_RELEASE_LOG("jobResolvedWithUnregistrationResult: Unregister job %" PRIu64 " finished. Success? %d", job.identifier().toUInt64(), unregistrationResult);

    if (!scriptExecutionContext()) {
        LOG_ERROR("ServiceWorkerContainer::jobResolvedWithUnregistrationResult called but the containers ScriptExecutionContext is gone");
        return;
    }

    queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [promise = job.takePromise(), unregistrationResult](auto&) mutable {
        promise->resolve<IDLBoolean>(unregistrationResult);
    });
}

void ServiceWorkerContainer::startScriptFetchForJob(ServiceWorkerJob& job, FetchOptions::Cache cachePolicy)
{
    ASSERT(m_creationThread.ptr() == &Thread::currentSingleton());

    CONTAINER_RELEASE_LOG("startScriptFetchForJob: Starting script fetch for job %" PRIu64, job.identifier().toUInt64());

    RefPtr context = scriptExecutionContext();
    if (!context) {
        LOG_ERROR("ServiceWorkerContainer::jobResolvedWithRegistration called but the container's ScriptExecutionContext is gone");
        notifyFailedFetchingScript(job, { errorDomainWebKitInternal, 0, job.data().scriptURL, "Attempt to fetch service worker script with no ScriptExecutionContext"_s });
        destroyJob(job);
        return;
    }

    job.fetchScriptWithContext(*context, cachePolicy);
}

void ServiceWorkerContainer::jobFinishedLoadingScript(ServiceWorkerJob& job, WorkerFetchResult&& fetchResult)
{
    ASSERT(m_creationThread.ptr() == &Thread::currentSingleton());

    CONTAINER_RELEASE_LOG("jobFinishedLoadingScript: Successfuly finished fetching script for job %" PRIu64, job.identifier().toUInt64());

    ensureProtectedSWClientConnection()->finishFetchingScriptInServer(job.data().identifier(), ServiceWorkerRegistrationKey { job.data().registrationKey() }, WTF::move(fetchResult));
}

void ServiceWorkerContainer::jobFailedLoadingScript(ServiceWorkerJob& job, const ResourceError& error, Exception&& exception)
{
    ASSERT(m_creationThread.ptr() == &Thread::currentSingleton());

    CONTAINER_RELEASE_LOG_ERROR("jobFinishedLoadingScript: Failed to fetch script for job %" PRIu64 ", error: %s", job.identifier().toUInt64(), error.localizedDescription().utf8().data());

    if (job.data().type == ServiceWorkerJobType::Register)
        willSettleRegistrationPromise(false);

    queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [promise = job.takePromise(), exception = WTF::move(exception)](auto&) mutable {
        promise->reject(WTF::move(exception));
    });

    notifyFailedFetchingScript(job, error);
    destroyJob(job);
}

void ServiceWorkerContainer::notifyFailedFetchingScript(ServiceWorkerJob& job, const ResourceError& error)
{
    ensureProtectedSWClientConnection()->finishFetchingScriptInServer(job.data().identifier(), ServiceWorkerRegistrationKey { job.data().registrationKey() }, workerFetchError(ResourceError { error }));
}

void ServiceWorkerContainer::destroyJob(ServiceWorkerJob& job)
{
    ASSERT(m_creationThread.ptr() == &Thread::currentSingleton());
    ASSERT(m_jobMap.contains(job.identifier()));

    bool isRegisterJob = job.data().type == ServiceWorkerJobType::Register;

    m_jobMap.remove(job.identifier());

    if (!isRegisterJob)
        return;

    if (auto callback = std::exchange(m_whenRegisterJobsAreFinished, { }))
        whenRegisterJobsAreFinished(WTF::move(callback));
}

SWClientConnection& ServiceWorkerContainer::ensureSWClientConnection()
{
    ASSERT(scriptExecutionContext());
    if (!m_swConnection || m_swConnection->isClosed()) {
        // Using RefPtr here results in an m_adoptionIsRequired assert.
        if (RefPtr workerGlobal = dynamicDowncast<WorkerGlobalScope>(*scriptExecutionContext()))
            m_swConnection = workerGlobal->swClientConnection();
        else
            m_swConnection = mainThreadConnection();
    }
    return *m_swConnection;
}

Ref<SWClientConnection> ServiceWorkerContainer::ensureProtectedSWClientConnection()
{
    return ensureSWClientConnection();
}

void ServiceWorkerContainer::addRegistration(ServiceWorkerRegistration& registration)
{
    ASSERT(m_creationThread.ptr() == &Thread::currentSingleton());

    ensureProtectedSWClientConnection()->addServiceWorkerRegistrationInServer(registration.identifier());
    m_registrations.add(registration.identifier(), registration);
}

void ServiceWorkerContainer::removeRegistration(ServiceWorkerRegistration& registration)
{
    ASSERT(m_creationThread.ptr() == &Thread::currentSingleton());

    Ref { *m_swConnection }->removeServiceWorkerRegistrationInServer(registration.identifier());
    m_registrations.remove(registration.identifier());
}

void ServiceWorkerContainer::subscribeToPushService(ServiceWorkerRegistration& registration, const Vector<uint8_t>& applicationServerKey, DOMPromiseDeferred<IDLInterface<PushSubscription>>&& promise)
{
    ensureProtectedSWClientConnection()->subscribeToPushService(registration.identifier(), applicationServerKey, [protectedRegistration = Ref { registration }, promise = WTF::move(promise)](ExceptionOr<PushSubscriptionData>&& result) mutable {
        if (result.hasException()) {
            promise.reject(result.releaseException());
            return;
        }
        
        promise.resolve(PushSubscription::create(result.releaseReturnValue(), WTF::move(protectedRegistration)));
    });
}

void ServiceWorkerContainer::unsubscribeFromPushService(ServiceWorkerRegistrationIdentifier identifier, PushSubscriptionIdentifier subscriptionIdentifier, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    ensureProtectedSWClientConnection()->unsubscribeFromPushService(identifier, subscriptionIdentifier, [promise = WTF::move(promise)](auto&& result) mutable {
        promise.settle(WTF::move(result));
    });
}

void ServiceWorkerContainer::getPushSubscription(ServiceWorkerRegistration& registration, DOMPromiseDeferred<IDLNullable<IDLInterface<PushSubscription>>>&& promise)
{
    ensureProtectedSWClientConnection()->getPushSubscription(registration.identifier(), [protectedRegistration = Ref { registration }, promise = WTF::move(promise)](ExceptionOr<std::optional<PushSubscriptionData>>&& result) mutable {
        if (result.hasException()) {
            promise.reject(result.releaseException());
            return;
        }

        std::optional<PushSubscriptionData> optionalPushSubscriptionData = result.releaseReturnValue();
        if (!optionalPushSubscriptionData) {
            promise.resolve(nullptr);
            return;
        }

        promise.resolve(PushSubscription::create(WTF::move(*optionalPushSubscriptionData), WTF::move(protectedRegistration)).ptr());
    });
}

void ServiceWorkerContainer::getPushPermissionState(ServiceWorkerRegistrationIdentifier identifier, DOMPromiseDeferred<IDLEnumeration<PushPermissionState>>&& promise)
{
    ensureProtectedSWClientConnection()->getPushPermissionState(identifier, [promise = WTF::move(promise)](auto&& result) mutable {
        promise.settle(WTF::move(result));
    });
}

#if ENABLE(NOTIFICATIONS) && ENABLE(NOTIFICATION_EVENT)
void ServiceWorkerContainer::getNotifications(const URL& serviceWorkerRegistrationURL, const String& tag, DOMPromiseDeferred<IDLSequence<IDLInterface<Notification>>>&& promise)
{
    ensureProtectedSWClientConnection()->getNotifications(serviceWorkerRegistrationURL, tag, [promise = WTF::move(promise), protectedThis = Ref { *this }](auto&& result) mutable {
        RefPtr context = protectedThis->scriptExecutionContext();
        if (!context)
            return;

        if (result.hasException()) {
            promise.reject(result.releaseException());
            return;
        }

        auto notifications = map(result.releaseReturnValue(), [context](NotificationData&& data) {
            auto notification = Notification::create(*context, WTF::move(data));
            notification->markAsShown();
            return notification;
        });
        promise.resolve(WTF::move(notifications));
    });
}
#endif

void ServiceWorkerContainer::queueTaskToDispatchControllerChangeEvent()
{
    ASSERT(m_creationThread.ptr() == &Thread::currentSingleton());

    queueTaskToDispatchEvent(*this, TaskSource::DOMManipulation, Event::create(eventNames().controllerchangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void ServiceWorkerContainer::stop()
{
    m_isStopped = true;
    removeAllEventListeners();
    m_readyPromise = nullptr;
    auto jobMap = WTF::move(m_jobMap);
    for (auto& ongoingJob : jobMap.values()) {
        if (Ref { *ongoingJob.job }->cancelPendingLoad())
            notifyFailedFetchingScript(*ongoingJob.job.get(), ResourceError { errorDomainWebKitInternal, 0, ongoingJob.job->data().scriptURL, "Job cancelled"_s, ResourceError::Type::Cancellation });
    }

    auto registrationMap = WTF::move(m_ongoingSettledRegistrations);
    for (auto& registration : registrationMap.values())
        notifyRegistrationIsSettled(registration);
}

ServiceWorkerOrClientIdentifier ServiceWorkerContainer::contextIdentifier()
{
    ASSERT(m_creationThread.ptr() == &Thread::currentSingleton());
    ASSERT(scriptExecutionContext());
    if (RefPtr serviceWorkerGlobal = dynamicDowncast<ServiceWorkerGlobalScope>(*scriptExecutionContext()))
        return serviceWorkerGlobal->thread()->identifier();
    return scriptExecutionContext()->identifier();
}

ServiceWorkerJob* ServiceWorkerContainer::job(ServiceWorkerJobIdentifier identifier)
{
    auto iterator = m_jobMap.find(identifier);
    if (iterator == m_jobMap.end())
        return nullptr;
    return iterator->value.job.get();
}

bool ServiceWorkerContainer::addEventListener(const AtomString& eventType, Ref<EventListener>&& eventListener, const AddEventListenerOptions& options)
{
    // Setting the onmessage EventHandler attribute on the ServiceWorkerContainer should start the messages
    // automatically.
    if (eventListener->isAttribute() && eventType == eventNames().messageEvent)
        startMessages();

    return EventTarget::addEventListener(eventType, WTF::move(eventListener), options);
}

void ServiceWorkerContainer::enableNavigationPreload(ServiceWorkerRegistrationIdentifier identifier, VoidPromise&& promise)
{
    ensureProtectedSWClientConnection()->enableNavigationPreload(identifier, [promise = WTF::move(promise)](auto&& result) mutable {
        promise.settle(WTF::move(result));
    });
}

void ServiceWorkerContainer::disableNavigationPreload(ServiceWorkerRegistrationIdentifier identifier, VoidPromise&& promise)
{
    ensureProtectedSWClientConnection()->disableNavigationPreload(identifier, [promise = WTF::move(promise)](auto&& result) mutable {
        promise.settle(WTF::move(result));
    });
}

void ServiceWorkerContainer::setNavigationPreloadHeaderValue(ServiceWorkerRegistrationIdentifier identifier, String&& headerValue, VoidPromise&& promise)
{
    ensureProtectedSWClientConnection()->setNavigationPreloadHeaderValue(identifier, WTF::move(headerValue), [promise = WTF::move(promise)](auto&& result) mutable {
        promise.settle(WTF::move(result));
    });
}

void ServiceWorkerContainer::getNavigationPreloadState(ServiceWorkerRegistrationIdentifier identifier, NavigationPreloadStatePromise&& promise)
{
    ensureProtectedSWClientConnection()->getNavigationPreloadState(identifier, [promise = WTF::move(promise)](auto&& result) mutable {
        promise.settle(WTF::move(result));
    });
}

void ServiceWorkerContainer::addCookieChangeSubscriptions(ServiceWorkerRegistrationIdentifier identifier, Vector<CookieChangeSubscription>&& subscriptions, Ref<DeferredPromise>&& promise)
{
    ensureProtectedSWClientConnection()->addCookieChangeSubscriptions(identifier, WTF::move(subscriptions), [promise = WTF::move(promise)](auto&& result) mutable {
        if (result.hasException())
            promise->reject(Exception { result.releaseException() });
        else
            promise->resolve();
    });
}

void ServiceWorkerContainer::removeCookieChangeSubscriptions(ServiceWorkerRegistrationIdentifier identifier, Vector<CookieChangeSubscription>&& subscriptions, Ref<DeferredPromise>&& promise)
{
    ensureProtectedSWClientConnection()->removeCookieChangeSubscriptions(identifier, WTF::move(subscriptions), [promise = WTF::move(promise)](auto&& result) mutable {
        if (result.hasException())
            promise->reject(Exception { result.releaseException() });
        else
            promise->resolve();
    });
}

void ServiceWorkerContainer::cookieChangeSubscriptions(ServiceWorkerRegistrationIdentifier identifier, Ref<DeferredPromise>&& promise)
{
    ensureProtectedSWClientConnection()->cookieChangeSubscriptions(identifier, [promise = WTF::move(promise)](auto&& result) mutable {
        if (result.hasException())
            promise->reject(Exception { result.releaseException() });
        else {
            promise->resolve<IDLSequence<IDLDictionary<CookieStoreGetOptions>>>(WTF::map(result.releaseReturnValue(), [](CookieChangeSubscription&& subscription) {
                return CookieStoreGetOptions { WTF::move(subscription.name), WTF::move(subscription.url) };
            }));
        }
    });
}

void ServiceWorkerContainer::whenRegisterJobsAreFinished(CompletionHandler<void()>&& completionHandler)
{
    bool isRegistering = std::ranges::any_of(m_jobMap.values(), [](auto& ongoing) {
        return ongoing.job->isRegistering();
    });

    if (!isRegistering) {
        completionHandler();
        return;
    }

    if (m_whenRegisterJobsAreFinished) {
        m_whenRegisterJobsAreFinished = [oldCompletionHandler = std::exchange(m_whenRegisterJobsAreFinished, { }), newCompletionHandler = WTF::move(completionHandler)]() mutable {
            oldCompletionHandler();
            newCompletionHandler();
        };
        return;
    }

    m_whenRegisterJobsAreFinished = WTF::move(completionHandler);
}

} // namespace WebCore
