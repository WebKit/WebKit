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

#include "ContentSecurityPolicy.h"
#include "DOMPromiseProxy.h"
#include "DedicatedWorkerGlobalScope.h"
#include "Document.h"
#include "Event.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "Exception.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "IDLTypes.h"
#include "JSDOMPromiseDeferred.h"
#include "JSNavigationPreloadState.h"
#include "JSPushSubscription.h"
#include "JSServiceWorkerRegistration.h"
#include "LegacySchemeRegistry.h"
#include "Logging.h"
#include "NavigatorBase.h"
#include "Page.h"
#include "PushSubscriptionOptions.h"
#include "ResourceError.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "ServiceWorker.h"
#include "ServiceWorkerGlobalScope.h"
#include "ServiceWorkerJob.h"
#include "ServiceWorkerJobData.h"
#include "ServiceWorkerProvider.h"
#include "ServiceWorkerThread.h"
#include "SharedWorkerGlobalScope.h"
#include "WorkerFetchResult.h"
#include "WorkerSWClientConnection.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/RunLoop.h>
#include <wtf/Scope.h>
#include <wtf/URL.h>

#define CONTAINER_RELEASE_LOG(fmt, ...) RELEASE_LOG(ServiceWorker, "%p - ServiceWorkerContainer::" fmt, this, ##__VA_ARGS__)
#define CONTAINER_RELEASE_LOG_ERROR(fmt, ...) RELEASE_LOG_ERROR(ServiceWorker, "%p - ServiceWorkerContainer::" fmt, this, ##__VA_ARGS__)

namespace WebCore {

static inline SWClientConnection& mainThreadConnection()
{
    return ServiceWorkerProvider::singleton().serviceWorkerConnection();
}

WTF_MAKE_ISO_ALLOCATED_IMPL(ServiceWorkerContainer);

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
    if (is<Document>(context) && downcast<Document>(*context).parsing())
        m_shouldDeferMessageEvents = true;
}

ServiceWorkerContainer::~ServiceWorkerContainer()
{
    ASSERT(m_creationThread.ptr() == &Thread::current());
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

        if (m_isStopped)
            return *m_readyPromise;

        auto& context = *scriptExecutionContext();
        ensureSWClientConnection().whenRegistrationReady(context.topOrigin().data(), context.url(), [this, protectedThis = Ref { *this }](auto&& registrationData) mutable {
            queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [this, registrationData = WTFMove(registrationData)]() mutable {
                auto* context = scriptExecutionContext();
                if (!context || !m_readyPromise)
                    return;
                auto registration = ServiceWorkerRegistration::getOrCreate(*context, *this, WTFMove(registrationData));
                m_readyPromise->resolve(WTFMove(registration));
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

void ServiceWorkerContainer::addRegistration(const String& relativeScriptURL, const RegistrationOptions& options, Ref<DeferredPromise>&& promise)
{
    auto* context = scriptExecutionContext();
    if (m_isStopped) {
        promise->reject(Exception(InvalidStateError));
        return;
    }

    if (relativeScriptURL.isEmpty()) {
        promise->reject(Exception { TypeError, "serviceWorker.register() cannot be called with an empty script URL"_s });
        return;
    }

    ServiceWorkerJobData jobData(ensureSWClientConnection().serverConnectionIdentifier(), contextIdentifier());

    jobData.scriptURL = context->completeURL(relativeScriptURL);

    auto* contentSecurityPolicy = is<Document>(context) ? downcast<Document>(context)->contentSecurityPolicy() : nullptr;
    if (contentSecurityPolicy && !contentSecurityPolicy->allowWorkerFromSource(jobData.scriptURL)) {
        promise->reject(Exception { SecurityError });
        return;
    }

    if (!jobData.scriptURL.isValid()) {
        CONTAINER_RELEASE_LOG_ERROR("addRegistration: Invalid scriptURL");
        promise->reject(Exception { TypeError, "serviceWorker.register() must be called with a valid relative script URL"_s });
        return;
    }

    Page* page = is<Document>(context) ? downcast<Document>(context)->page() : nullptr;
    jobData.isFromServiceWorkerPage = page && page->isServiceWorkerPage();
    if (!jobData.scriptURL.protocolIsInHTTPFamily() && !jobData.isFromServiceWorkerPage) {
        CONTAINER_RELEASE_LOG_ERROR("addRegistration: Invalid scriptURL scheme is not HTTP or HTTPS");
        promise->reject(Exception { TypeError, "serviceWorker.register() must be called with a script URL whose protocol is either HTTP or HTTPS"_s });
        return;
    }

    auto path = jobData.scriptURL.path();
    if (path.containsIgnoringASCIICase("%2f"_s) || path.containsIgnoringASCIICase("%5c"_s)) {
        CONTAINER_RELEASE_LOG_ERROR("addRegistration: scriptURL contains invalid character");
        promise->reject(Exception { TypeError, "serviceWorker.register() must be called with a script URL whose path does not contain '%2f' or '%5c'"_s });
        return;
    }

    if (!options.scope.isEmpty())
        jobData.scopeURL = context->completeURL(options.scope);
    else
        jobData.scopeURL = URL(jobData.scriptURL, "./"_s);

    if (!jobData.scopeURL.isNull() && !jobData.scopeURL.protocolIsInHTTPFamily() && !jobData.isFromServiceWorkerPage) {
        CONTAINER_RELEASE_LOG_ERROR("addRegistration: scopeURL scheme is not HTTP or HTTPS");
        promise->reject(Exception { TypeError, "Scope URL provided to serviceWorker.register() must be either HTTP or HTTPS"_s });
        return;
    }

    path = jobData.scopeURL.path();
    if (path.containsIgnoringASCIICase("%2f"_s) || path.containsIgnoringASCIICase("%5c"_s)) {
        CONTAINER_RELEASE_LOG_ERROR("addRegistration: scopeURL contains invalid character");
        promise->reject(Exception { TypeError, "Scope URL provided to serviceWorker.register() cannot have a path that contains '%2f' or '%5c'"_s });
        return;
    }

    CONTAINER_RELEASE_LOG("addRegistration: Registering service worker. jobID=%" PRIu64, jobData.identifier().jobIdentifier.toUInt64());

    jobData.clientCreationURL = context->url();
    jobData.topOrigin = context->topOrigin().data();
    jobData.workerType = options.type;
    jobData.type = ServiceWorkerJobType::Register;
    jobData.domainForCachePartition = context->domainForCachePartition();
    jobData.registrationOptions = options;

    scheduleJob(makeUnique<ServiceWorkerJob>(*this, WTFMove(promise), WTFMove(jobData)));
}

void ServiceWorkerContainer::willSettleRegistrationPromise(bool success)
{
    auto* context = scriptExecutionContext();
    Page* page = is<Document>(context) ? downcast<Document>(*context).page() : nullptr;
    if (!page || !page->isServiceWorkerPage())
        return;

    page->mainFrame().loader().client().didFinishServiceWorkerPageRegistration(success);
}

void ServiceWorkerContainer::unregisterRegistration(ServiceWorkerRegistrationIdentifier registrationIdentifier, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    ASSERT(!m_isStopped);
    if (!m_swConnection) {
        ASSERT_NOT_REACHED();
        promise.reject(Exception(InvalidStateError));
        return;
    }

    CONTAINER_RELEASE_LOG("unregisterRegistration: Unregistering service worker.");
    m_swConnection->scheduleUnregisterJobInServer(registrationIdentifier, contextIdentifier(), [promise = WTFMove(promise)](auto&& result) mutable {
        promise.settle(WTFMove(result));
    });
}

void ServiceWorkerContainer::updateRegistration(const URL& scopeURL, const URL& scriptURL, WorkerType workerType, RefPtr<DeferredPromise>&& promise)
{
    ASSERT(!m_isStopped);

    auto& context = *scriptExecutionContext();

    if (!m_swConnection) {
        ASSERT_NOT_REACHED();
        if (promise)
            promise->reject(Exception(InvalidStateError));
        return;
    }

    ServiceWorkerJobData jobData(m_swConnection->serverConnectionIdentifier(), contextIdentifier());
    jobData.clientCreationURL = context.url();
    jobData.topOrigin = context.topOrigin().data();
    jobData.workerType = workerType;
    jobData.type = ServiceWorkerJobType::Update;
    jobData.domainForCachePartition = context.domainForCachePartition();
    jobData.scopeURL = scopeURL;
    jobData.scriptURL = scriptURL;

    CONTAINER_RELEASE_LOG("removeRegistration: Updating service worker. jobID=%" PRIu64, jobData.identifier().jobIdentifier.toUInt64());

    scheduleJob(makeUnique<ServiceWorkerJob>(*this, WTFMove(promise), WTFMove(jobData)));
}

void ServiceWorkerContainer::scheduleJob(std::unique_ptr<ServiceWorkerJob>&& job)
{
    ASSERT(m_creationThread.ptr() == &Thread::current());
    ASSERT(m_swConnection);
    ASSERT(!isStopped());

    auto& jobData = job->data();
    auto jobIdentifier = job->identifier();
    ASSERT(!m_jobMap.contains(jobIdentifier));
    m_jobMap.add(jobIdentifier, OngoingJob { WTFMove(job), makePendingActivity(*this) });

    m_swConnection->scheduleJob(contextIdentifier(), jobData);
}

void ServiceWorkerContainer::getRegistration(const String& clientURL, Ref<DeferredPromise>&& promise)
{
    if (m_isStopped) {
        promise->reject(Exception { InvalidStateError });
        return;
    }

    auto& context = *scriptExecutionContext();
    URL parsedURL = context.completeURL(clientURL);
    if (!protocolHostAndPortAreEqual(parsedURL, context.url())) {
        promise->reject(Exception { SecurityError, "Origin of clientURL is not client's origin"_s });
        return;
    }

    ensureSWClientConnection().matchRegistration(SecurityOriginData { context.topOrigin().data() }, parsedURL, [this, protectedThis = Ref { *this }, promise = WTFMove(promise)](auto&& result) mutable {
        queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [this, promise = WTFMove(promise), result = WTFMove(result)]() mutable {
            if (!result) {
                promise->resolve();
                return;
            }
            promise->resolve<IDLInterface<ServiceWorkerRegistration>>(ServiceWorkerRegistration::getOrCreate(*scriptExecutionContext(), *this, WTFMove(result.value())));
        });
    });
}

void ServiceWorkerContainer::updateRegistrationState(ServiceWorkerRegistrationIdentifier identifier, ServiceWorkerRegistrationState state, const std::optional<ServiceWorkerData>& serviceWorkerData)
{
    if (m_isStopped)
        return;

    queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [this, identifier, state, serviceWorkerData = std::optional<ServiceWorkerData> { serviceWorkerData }]() mutable {
        RefPtr<ServiceWorker> serviceWorker;
        if (serviceWorkerData)
            serviceWorker = ServiceWorker::getOrCreate(*scriptExecutionContext(), WTFMove(*serviceWorkerData));

        if (auto* registration = m_registrations.get(identifier))
            registration->updateStateFromServer(state, WTFMove(serviceWorker));
    });
}

void ServiceWorkerContainer::updateWorkerState(ServiceWorkerIdentifier identifier, ServiceWorkerState state)
{
    if (m_isStopped)
        return;

    queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [this, identifier, state] {
        if (auto* serviceWorker = scriptExecutionContext()->serviceWorker(identifier))
            serviceWorker->updateState(state);
    });
}

void ServiceWorkerContainer::getRegistrations(Ref<DeferredPromise>&& promise)
{
    if (m_isStopped) {
        promise->reject(Exception { InvalidStateError });
        return;
    }

    auto& context = *scriptExecutionContext();
    ensureSWClientConnection().getRegistrations(SecurityOriginData { context.topOrigin().data() }, context.url(), [this, protectedThis = Ref { *this }, promise = WTFMove(promise)] (auto&& registrationDatas) mutable {
        queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [this, promise = WTFMove(promise), registrationDatas = WTFMove(registrationDatas)]() mutable {
            auto registrations = WTF::map(WTFMove(registrationDatas), [&](auto&& registrationData) {
                return ServiceWorkerRegistration::getOrCreate(*scriptExecutionContext(), *this, WTFMove(registrationData));
            });
            promise->resolve<IDLSequence<IDLInterface<ServiceWorkerRegistration>>>(WTFMove(registrations));
        });
    });
}

void ServiceWorkerContainer::startMessages()
{
    m_shouldDeferMessageEvents = false;
    for (auto&& messageEvent : std::exchange(m_deferredMessageEvents, Vector<MessageEvent::MessageEventWithStrongData> { })) {
        queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [this, messageEvent = WTFMove(messageEvent)] {
            dispatchEvent(messageEvent.event);
        });
    }
}

void ServiceWorkerContainer::jobFailedWithException(ServiceWorkerJob& job, const Exception& exception)
{
    ASSERT(m_creationThread.ptr() == &Thread::current());
    ASSERT_WITH_MESSAGE(job.hasPromise() || job.data().type == ServiceWorkerJobType::Update, "Only soft updates have no promise");

    auto guard = makeScopeExit([this, &job] {
        destroyJob(job);
    });

    CONTAINER_RELEASE_LOG_ERROR("jobFailedWithException: Job %" PRIu64 " failed with error %s", job.identifier().toUInt64(), exception.message().utf8().data());

    if (job.data().type == ServiceWorkerJobType::Register)
        willSettleRegistrationPromise(false);

    auto promise = job.takePromise();
    if (!promise)
        return;

    queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [promise = WTFMove(promise), exception]() mutable {
        promise->reject(exception);
    });
}

void ServiceWorkerContainer::queueTaskToFireUpdateFoundEvent(ServiceWorkerRegistrationIdentifier identifier)
{
    ASSERT(m_creationThread.ptr() == &Thread::current());

    if (auto* registration = m_registrations.get(identifier))
        registration->queueTaskToFireUpdateFoundEvent();
}

void ServiceWorkerContainer::jobResolvedWithRegistration(ServiceWorkerJob& job, ServiceWorkerRegistrationData&& data, ShouldNotifyWhenResolved shouldNotifyWhenResolved)
{
    ASSERT(m_creationThread.ptr() == &Thread::current());
    ASSERT_WITH_MESSAGE(job.hasPromise() || job.data().type == ServiceWorkerJobType::Update, "Only soft updates have no promise");

    if (job.data().type == ServiceWorkerJobType::Register) {
        CONTAINER_RELEASE_LOG("jobResolvedWithRegistration: Registration job %" PRIu64 " succeeded", job.identifier().toUInt64());
        willSettleRegistrationPromise(true);
    } else {
        ASSERT(job.data().type == ServiceWorkerJobType::Update);
        CONTAINER_RELEASE_LOG("jobResolvedWithRegistration: Update job %" PRIu64 " succeeded", job.identifier().toUInt64());
    }

    auto guard = makeScopeExit([this, &job] {
        destroyJob(job);
    });

    auto notifyIfExitEarly = makeScopeExit([this, protectedThis = Ref { *this }, key = data.key, shouldNotifyWhenResolved] {
        if (shouldNotifyWhenResolved == ShouldNotifyWhenResolved::Yes)
            notifyRegistrationIsSettled(key);
    });

    if (isStopped())
        return;

    auto promise = job.takePromise();
    if (!promise)
        return;

    queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [this, protectedThis = Ref { *this }, promise = WTFMove(promise), jobIdentifier = job.identifier(), data = WTFMove(data), shouldNotifyWhenResolved, notifyIfExitEarly = WTFMove(notifyIfExitEarly)]() mutable {
        notifyIfExitEarly.release();

        auto registration = ServiceWorkerRegistration::getOrCreate(*scriptExecutionContext(), *this, WTFMove(data));

        CONTAINER_RELEASE_LOG("jobResolvedWithRegistration: Resolving promise for job %" PRIu64 ". registrationID=%" PRIu64, jobIdentifier.toUInt64(), registration->identifier().toUInt64());

        if (shouldNotifyWhenResolved == ShouldNotifyWhenResolved::Yes) {
            m_ongoingSettledRegistrations.add(++m_lastOngoingSettledRegistrationIdentifier, registration->data().key);
            promise->whenSettled([this, protectedThis = WTFMove(protectedThis), identifier = m_lastOngoingSettledRegistrationIdentifier] {
                auto iterator = m_ongoingSettledRegistrations.find(identifier);
                if (iterator == m_ongoingSettledRegistrations.end())
                    return;
                notifyRegistrationIsSettled(iterator->value);
                m_ongoingSettledRegistrations.remove(iterator);
            });
        }

        promise->resolve<IDLInterface<ServiceWorkerRegistration>>(WTFMove(registration));
    });
}

void ServiceWorkerContainer::postMessage(MessageWithMessagePorts&& message, ServiceWorkerData&& sourceData, String&& sourceOrigin)
{
    auto& context = *scriptExecutionContext();
    auto* globalObject = context.globalObject();
    if (!globalObject)
        return;

    MessageEventSource source = RefPtr<ServiceWorker> { ServiceWorker::getOrCreate(context, WTFMove(sourceData)) };

    auto messageEvent = MessageEvent::create(*globalObject, message.message.releaseNonNull(), sourceOrigin, { }, WTFMove(source), MessagePort::entanglePorts(context, WTFMove(message.transferredPorts)));
    if (m_shouldDeferMessageEvents)
        m_deferredMessageEvents.append(WTFMove(messageEvent));
    else {
        ASSERT(m_deferredMessageEvents.isEmpty());
        queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [this, messageEvent = WTFMove(messageEvent)] {
            dispatchEvent(messageEvent.event);
        });
    }
}

void ServiceWorkerContainer::notifyRegistrationIsSettled(const ServiceWorkerRegistrationKey& registrationKey)
{
    ensureSWClientConnection().didResolveRegistrationPromise(registrationKey);
}

void ServiceWorkerContainer::jobResolvedWithUnregistrationResult(ServiceWorkerJob& job, bool unregistrationResult)
{
    ASSERT(m_creationThread.ptr() == &Thread::current());
    ASSERT(job.hasPromise());

    auto guard = makeScopeExit([this, &job] {
        destroyJob(job);
    });

    CONTAINER_RELEASE_LOG("jobResolvedWithUnregistrationResult: Unregister job %" PRIu64 " finished. Success? %d", job.identifier().toUInt64(), unregistrationResult);

    auto* context = scriptExecutionContext();
    if (!context) {
        LOG_ERROR("ServiceWorkerContainer::jobResolvedWithUnregistrationResult called but the containers ScriptExecutionContext is gone");
        return;
    }

    queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [promise = job.takePromise(), unregistrationResult]() mutable {
        promise->resolve<IDLBoolean>(unregistrationResult);
    });
}

void ServiceWorkerContainer::startScriptFetchForJob(ServiceWorkerJob& job, FetchOptions::Cache cachePolicy)
{
    ASSERT(m_creationThread.ptr() == &Thread::current());

    CONTAINER_RELEASE_LOG("startScriptFetchForJob: Starting script fetch for job %" PRIu64, job.identifier().toUInt64());

    auto* context = scriptExecutionContext();
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
    ASSERT(m_creationThread.ptr() == &Thread::current());

    CONTAINER_RELEASE_LOG("jobFinishedLoadingScript: Successfuly finished fetching script for job %" PRIu64, job.identifier().toUInt64());

    ensureSWClientConnection().finishFetchingScriptInServer(job.data().identifier(), ServiceWorkerRegistrationKey { job.data().registrationKey() }, WTFMove(fetchResult));
}

void ServiceWorkerContainer::jobFailedLoadingScript(ServiceWorkerJob& job, const ResourceError& error, Exception&& exception)
{
    ASSERT(m_creationThread.ptr() == &Thread::current());
    ASSERT_WITH_MESSAGE(job.hasPromise() || job.data().type == ServiceWorkerJobType::Update, "Only soft updates have no promise");

    CONTAINER_RELEASE_LOG_ERROR("jobFinishedLoadingScript: Failed to fetch script for job %" PRIu64 ", error: %s", job.identifier().toUInt64(), error.localizedDescription().utf8().data());

    if (job.data().type == ServiceWorkerJobType::Register)
        willSettleRegistrationPromise(false);

    if (auto promise = job.takePromise()) {
        queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [promise = WTFMove(promise), exception = WTFMove(exception)]() mutable {
            promise->reject(WTFMove(exception));
        });
    }

    notifyFailedFetchingScript(job, error);
    destroyJob(job);
}

void ServiceWorkerContainer::notifyFailedFetchingScript(ServiceWorkerJob& job, const ResourceError& error)
{
    ensureSWClientConnection().finishFetchingScriptInServer(job.data().identifier(), ServiceWorkerRegistrationKey { job.data().registrationKey() }, workerFetchError(ResourceError { error }));
}

void ServiceWorkerContainer::destroyJob(ServiceWorkerJob& job)
{
    ASSERT(m_creationThread.ptr() == &Thread::current());
    ASSERT(m_jobMap.contains(job.identifier()));
    m_jobMap.remove(job.identifier());
}

const char* ServiceWorkerContainer::activeDOMObjectName() const
{
    return "ServiceWorkerContainer";
}

SWClientConnection& ServiceWorkerContainer::ensureSWClientConnection()
{
    ASSERT(scriptExecutionContext());
    if (!m_swConnection || m_swConnection->isClosed()) {
        auto& context = *scriptExecutionContext();
        if (is<WorkerGlobalScope>(context))
            m_swConnection = &downcast<WorkerGlobalScope>(context).swClientConnection();
        else
            m_swConnection = &mainThreadConnection();
    }
    return *m_swConnection;
}

void ServiceWorkerContainer::addRegistration(ServiceWorkerRegistration& registration)
{
    ASSERT(m_creationThread.ptr() == &Thread::current());

    ensureSWClientConnection().addServiceWorkerRegistrationInServer(registration.identifier());
    m_registrations.add(registration.identifier(), &registration);
}

void ServiceWorkerContainer::removeRegistration(ServiceWorkerRegistration& registration)
{
    ASSERT(m_creationThread.ptr() == &Thread::current());

    m_swConnection->removeServiceWorkerRegistrationInServer(registration.identifier());
    m_registrations.remove(registration.identifier());
}

void ServiceWorkerContainer::subscribeToPushService(ServiceWorkerRegistration& registration, const Vector<uint8_t>& applicationServerKey, DOMPromiseDeferred<IDLInterface<PushSubscription>>&& promise)
{
    ensureSWClientConnection().subscribeToPushService(registration.identifier(), applicationServerKey, [protectedRegistration = Ref { registration }, promise = WTFMove(promise)](auto&& result) mutable {
        if (result.hasException()) {
            promise.reject(result.releaseException());
            return;
        }
        
        promise.resolve(PushSubscription::create(result.releaseReturnValue(), WTFMove(protectedRegistration)));
    });
}

void ServiceWorkerContainer::unsubscribeFromPushService(ServiceWorkerRegistrationIdentifier identifier, PushSubscriptionIdentifier subscriptionIdentifier, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    ensureSWClientConnection().unsubscribeFromPushService(identifier, subscriptionIdentifier, [promise = WTFMove(promise)](auto&& result) mutable {
        promise.settle(WTFMove(result));
    });
}

void ServiceWorkerContainer::getPushSubscription(ServiceWorkerRegistration& registration, DOMPromiseDeferred<IDLNullable<IDLInterface<PushSubscription>>>&& promise)
{
    ensureSWClientConnection().getPushSubscription(registration.identifier(), [protectedRegistration = Ref { registration }, promise = WTFMove(promise)](auto&& result) mutable {
        if (result.hasException()) {
            promise.reject(result.releaseException());
            return;
        }

        auto optionalPushSubscriptionData = result.releaseReturnValue();
        if (!optionalPushSubscriptionData) {
            promise.resolve(nullptr);
            return;
        }

        promise.resolve(PushSubscription::create(WTFMove(*optionalPushSubscriptionData), WTFMove(protectedRegistration)).ptr());
    });
}

void ServiceWorkerContainer::getPushPermissionState(ServiceWorkerRegistrationIdentifier identifier, DOMPromiseDeferred<IDLEnumeration<PushPermissionState>>&& promise)
{
    ensureSWClientConnection().getPushPermissionState(identifier, [promise = WTFMove(promise)](auto&& result) mutable {
        promise.settle(WTFMove(result));
    });
}

#if ENABLE(NOTIFICATIONS)
void ServiceWorkerContainer::getNotifications(const URL& serviceWorkerRegistrationURL, const String& tag, DOMPromiseDeferred<IDLSequence<IDLInterface<Notification>>>&& promise)
{
    ensureSWClientConnection().getNotifications(serviceWorkerRegistrationURL, tag, [promise = WTFMove(promise), protectedThis = Ref { *this }](auto&& result) mutable {
        auto* context = protectedThis->scriptExecutionContext();
        if (!context)
            return;

        if (result.hasException()) {
            promise.reject(result.releaseException());
            return;
        }

        auto data = result.releaseReturnValue();
        auto notifications = map(data, [context](auto&& data) {
            return Notification::create(*context, WTFMove(data));
        });
        promise.resolve(WTFMove(notifications));
    });
}
#endif

void ServiceWorkerContainer::queueTaskToDispatchControllerChangeEvent()
{
    ASSERT(m_creationThread.ptr() == &Thread::current());

    queueTaskToDispatchEvent(*this, TaskSource::DOMManipulation, Event::create(eventNames().controllerchangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void ServiceWorkerContainer::stop()
{
    m_isStopped = true;
    removeAllEventListeners();
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

ServiceWorkerOrClientIdentifier ServiceWorkerContainer::contextIdentifier()
{
    ASSERT(m_creationThread.ptr() == &Thread::current());
    ASSERT(scriptExecutionContext());
    if (is<ServiceWorkerGlobalScope>(*scriptExecutionContext()))
        return downcast<ServiceWorkerGlobalScope>(*scriptExecutionContext()).thread().identifier();
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

    return EventTarget::addEventListener(eventType, WTFMove(eventListener), options);
}

void ServiceWorkerContainer::enableNavigationPreload(ServiceWorkerRegistrationIdentifier identifier, VoidPromise&& promise)
{
    ensureSWClientConnection().enableNavigationPreload(identifier, [promise = WTFMove(promise)](auto&& result) mutable {
        promise.settle(WTFMove(result));
    });
}

void ServiceWorkerContainer::disableNavigationPreload(ServiceWorkerRegistrationIdentifier identifier, VoidPromise&& promise)
{
    ensureSWClientConnection().disableNavigationPreload(identifier, [promise = WTFMove(promise)](auto&& result) mutable {
        promise.settle(WTFMove(result));
    });
}

void ServiceWorkerContainer::setNavigationPreloadHeaderValue(ServiceWorkerRegistrationIdentifier identifier, String&& headerValue, VoidPromise&& promise)
{
    ensureSWClientConnection().setNavigationPreloadHeaderValue(identifier, WTFMove(headerValue), [promise = WTFMove(promise)](auto&& result) mutable {
        promise.settle(WTFMove(result));
    });
}

void ServiceWorkerContainer::getNavigationPreloadState(ServiceWorkerRegistrationIdentifier identifier, NavigationPreloadStatePromise&& promise)
{
    ensureSWClientConnection().getNavigationPreloadState(identifier, [promise = WTFMove(promise)](auto&& result) mutable {
        promise.settle(WTFMove(result));
    });
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
