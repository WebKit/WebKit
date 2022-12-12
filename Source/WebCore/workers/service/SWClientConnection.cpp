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
#include "SWClientConnection.h"

#if ENABLE(SERVICE_WORKER)

#include "Document.h"
#include "ExceptionData.h"
#include "MessageEvent.h"
#include "SWContextManager.h"
#include "ServiceWorkerContainer.h"
#include "ServiceWorkerGlobalScope.h"
#include "ServiceWorkerJobData.h"
#include "ServiceWorkerRegistration.h"
#include "SharedWorkerContextManager.h"
#include "SharedWorkerThread.h"
#include "SharedWorkerThreadProxy.h"
#include "Worker.h"
#include "WorkerFetchResult.h"
#include <wtf/CrossThreadCopier.h>

namespace WebCore {

static bool dispatchToContextThreadIfNecessary(const ServiceWorkerOrClientIdentifier& contextIdentifier, Function<void(ScriptExecutionContext&)>&& task)
{
    RELEASE_ASSERT(isMainThread());

    return switchOn(contextIdentifier, [&] (ScriptExecutionContextIdentifier identifier) {
        return ScriptExecutionContext::postTaskTo(identifier, WTFMove(task));
    }, [&](ServiceWorkerIdentifier identifier) {
        return SWContextManager::singleton().postTaskToServiceWorker(identifier, WTFMove(task));
    });
}

SWClientConnection::SWClientConnection() = default;

SWClientConnection::~SWClientConnection() = default;

void SWClientConnection::scheduleJob(ServiceWorkerOrClientIdentifier contextIdentifier, const ServiceWorkerJobData& jobData)
{
    auto addResult = m_scheduledJobSources.add(jobData.identifier().jobIdentifier, contextIdentifier);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);

    scheduleJobInServer(jobData);
}

bool SWClientConnection::postTaskForJob(ServiceWorkerJobIdentifier jobIdentifier, IsJobComplete isJobComplete, Function<void(ServiceWorkerJob&)>&& task)
{
    ASSERT(isMainThread());

    auto iterator = m_scheduledJobSources.find(jobIdentifier);
    if (iterator == m_scheduledJobSources.end()) {
        LOG_ERROR("Job %s was not found", jobIdentifier.loggingString().utf8().data());
        return false;
    }
    auto isPosted = dispatchToContextThreadIfNecessary(iterator->value, [jobIdentifier, task = WTFMove(task)] (ScriptExecutionContext& context) mutable {
        if (auto* container = context.serviceWorkerContainer()) {
            if (auto* job = container->job(jobIdentifier))
                task(*job);
        }
    });
    if (isJobComplete == IsJobComplete::Yes)
        m_scheduledJobSources.remove(iterator);
    return isPosted;
}

void SWClientConnection::jobRejectedInServer(ServiceWorkerJobIdentifier jobIdentifier, ExceptionData&& exceptionData)
{
    postTaskForJob(jobIdentifier, IsJobComplete::Yes, [exceptionData = WTFMove(exceptionData).isolatedCopy()] (auto& job) mutable {
        job.failedWithException(WTFMove(exceptionData).toException());
    });
}

void SWClientConnection::registrationJobResolvedInServer(ServiceWorkerJobIdentifier jobIdentifier, ServiceWorkerRegistrationData&& registrationData, ShouldNotifyWhenResolved shouldNotifyWhenResolved)
{
    auto registrationKey = registrationData.key;
    bool isPosted = postTaskForJob(jobIdentifier, IsJobComplete::Yes, [registrationData = WTFMove(registrationData).isolatedCopy(), shouldNotifyWhenResolved] (auto& job) mutable {
        job.resolvedWithRegistration(WTFMove(registrationData), shouldNotifyWhenResolved);
    });

    if (!isPosted && shouldNotifyWhenResolved == ShouldNotifyWhenResolved::Yes)
        didResolveRegistrationPromise(registrationKey);
}

void SWClientConnection::startScriptFetchForServer(ServiceWorkerJobIdentifier jobIdentifier, ServiceWorkerRegistrationKey&& registrationKey, FetchOptions::Cache cachePolicy)
{
    bool isPosted = postTaskForJob(jobIdentifier, IsJobComplete::No, [cachePolicy] (auto& job) {
        job.startScriptFetch(cachePolicy);
    });
    if (!isPosted)
        finishFetchingScriptInServer({ serverConnectionIdentifier(), jobIdentifier }, WTFMove(registrationKey), workerFetchError(ResourceError { errorDomainWebKitInternal, 0, { }, makeString("Failed to fetch script for service worker with scope ", registrationKey.scope().string()) }));
}

static void postMessageToContainer(ScriptExecutionContext& context, MessageWithMessagePorts&& message, ServiceWorkerData&& sourceData, String&& sourceOrigin)
{
    if (auto* container = context.ensureServiceWorkerContainer())
        container->postMessage(WTFMove(message), WTFMove(sourceData), WTFMove(sourceOrigin));
}

void SWClientConnection::postMessageToServiceWorkerClient(ScriptExecutionContextIdentifier destinationContextIdentifier, MessageWithMessagePorts&& message, ServiceWorkerData&& sourceData, String&& sourceOrigin)
{
    ASSERT(isMainThread());

    if (auto* destinationDocument = Document::allDocumentsMap().get(destinationContextIdentifier)) {
        postMessageToContainer(*destinationDocument, WTFMove(message), WTFMove(sourceData), WTFMove(sourceOrigin));
        return;
    }

    ScriptExecutionContext::postTaskTo(destinationContextIdentifier, [message = WTFMove(message), sourceData = WTFMove(sourceData).isolatedCopy(), sourceOrigin = WTFMove(sourceOrigin).isolatedCopy()](auto& context) mutable {
        postMessageToContainer(context, WTFMove(message), WTFMove(sourceData), WTFMove(sourceOrigin));
    });
}

static void forAllWorkers(const Function<Function<void(ScriptExecutionContext&)>()>& callback)
{
    SWContextManager::singleton().forEachServiceWorker(callback);
    Worker::forEachWorker(callback);
    SharedWorkerContextManager::singleton().forEachSharedWorker(callback);
}

void SWClientConnection::updateRegistrationState(ServiceWorkerRegistrationIdentifier identifier, ServiceWorkerRegistrationState state, const std::optional<ServiceWorkerData>& serviceWorkerData)
{
    ASSERT(isMainThread());

    for (auto* document : Document::allDocuments()) {
        if (auto* container = document->serviceWorkerContainer())
            container->updateRegistrationState(identifier, state, serviceWorkerData);
    }

    forAllWorkers([identifier, state, &serviceWorkerData] {
        return [identifier, state, serviceWorkerData = crossThreadCopy(serviceWorkerData)] (auto& context) mutable {
            if (auto* container = context.serviceWorkerContainer())
                container->updateRegistrationState(identifier, state, WTFMove(serviceWorkerData));
        };
    });
}

void SWClientConnection::updateWorkerState(ServiceWorkerIdentifier identifier, ServiceWorkerState state)
{
    ASSERT(isMainThread());

    for (auto* document : Document::allDocuments()) {
        if (auto* container = document->serviceWorkerContainer())
            container->updateWorkerState(identifier, state);
    }

    forAllWorkers([identifier, state] {
        return [identifier, state] (auto& context) {
            if (auto* container = context.serviceWorkerContainer())
                container->updateWorkerState(identifier, state);
        };
    });
}

void SWClientConnection::fireUpdateFoundEvent(ServiceWorkerRegistrationIdentifier identifier)
{
    ASSERT(isMainThread());

    for (auto* document : Document::allDocuments()) {
        if (auto* container = document->serviceWorkerContainer())
            container->queueTaskToFireUpdateFoundEvent(identifier);
    }

    forAllWorkers([identifier] {
        return [identifier] (auto& context) {
            if (auto* container = context.serviceWorkerContainer())
                container->queueTaskToFireUpdateFoundEvent(identifier);
        };
    });
}

void SWClientConnection::setRegistrationLastUpdateTime(ServiceWorkerRegistrationIdentifier identifier, WallTime lastUpdateTime)
{
    ASSERT(isMainThread());

    for (auto* document : Document::allDocuments()) {
        if (auto* container = document->serviceWorkerContainer()) {
            if (auto* registration = container->registration(identifier))
                registration->setLastUpdateTime(lastUpdateTime);
        }
    }

    forAllWorkers([identifier, lastUpdateTime] {
        return [identifier, lastUpdateTime] (auto& context) {
            if (auto* container = context.serviceWorkerContainer()) {
                if (auto* registration = container->registration(identifier))
                    registration->setLastUpdateTime(lastUpdateTime);
            }
        };
    });
}

void SWClientConnection::setRegistrationUpdateViaCache(ServiceWorkerRegistrationIdentifier identifier, ServiceWorkerUpdateViaCache updateViaCache)
{
    ASSERT(isMainThread());

    for (auto* document : Document::allDocuments()) {
        if (auto* container = document->serviceWorkerContainer()) {
            if (auto* registration = container->registration(identifier))
                registration->setUpdateViaCache(updateViaCache);
        }
    }

    forAllWorkers([identifier, updateViaCache] {
        return [identifier, updateViaCache] (auto& context) {
            if (auto* container = context.serviceWorkerContainer()) {
                if (auto* registration = container->registration(identifier))
                    registration->setUpdateViaCache(updateViaCache);
            }
        };
    });
}

static void updateController(ScriptExecutionContext& context, ServiceWorkerData&& newController)
{
    context.setActiveServiceWorker(ServiceWorker::getOrCreate(context, WTFMove(newController)));
    if (auto* container = context.serviceWorkerContainer())
        container->queueTaskToDispatchControllerChangeEvent();
}

void SWClientConnection::notifyClientsOfControllerChange(const HashSet<ScriptExecutionContextIdentifier>& contextIdentifiers, ServiceWorkerData&& newController)
{
    ASSERT(isMainThread());
    ASSERT(!contextIdentifiers.isEmpty());

    for (auto& clientIdentifier : contextIdentifiers) {
        if (auto* document = Document::allDocumentsMap().get(clientIdentifier)) {
            updateController(*document, ServiceWorkerData { newController });
            continue;
        }
        bool wasDispatched = ScriptExecutionContext::postTaskTo(clientIdentifier, [newController = newController.isolatedCopy()](auto& context) mutable {
            updateController(context, WTFMove(newController));
        });
        if (wasDispatched)
            continue;
        if (auto* sharedWorker = SharedWorkerThreadProxy::byIdentifier(clientIdentifier)) {
            sharedWorker->thread().runLoop().postTask([newController = newController.isolatedCopy()] (auto& context) mutable {
                updateController(context, WTFMove(newController));
            });
            continue;
        }
    }
}

void SWClientConnection::clearPendingJobs()
{
    ASSERT(isMainThread());

    auto jobSources = WTFMove(m_scheduledJobSources);
    for (auto& keyValue : jobSources) {
        dispatchToContextThreadIfNecessary(keyValue.value, [identifier = keyValue.key] (auto& context) {
            if (auto* container = context.serviceWorkerContainer()) {
                if (auto* job = container->job(identifier))
                    job->failedWithException(Exception { TypeError, "Internal error"_s });
            }
        });
    }
}

void SWClientConnection::registerServiceWorkerClients()
{
    for (auto* document : Document::allDocuments())
        document->updateServiceWorkerClientData();

    SharedWorkerContextManager::singleton().forEachSharedWorker([] { return [] (auto& context) { context.updateServiceWorkerClientData(); }; });
    Worker::forEachWorker([] { return [] (auto& context) { context.updateServiceWorkerClientData(); }; });
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
