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
#include "ServiceWorkerFetchResult.h"
#include "ServiceWorkerGlobalScope.h"
#include "ServiceWorkerJobData.h"
#include "ServiceWorkerRegistration.h"
#include <wtf/CrossThreadCopier.h>

namespace WebCore {

static bool dispatchToContextThreadIfNecessary(const DocumentOrWorkerIdentifier& contextIdentifier, WTF::Function<void(ScriptExecutionContext&)>&& task)
{
    RELEASE_ASSERT(isMainThread());
    bool wasPosted = false;
    switchOn(contextIdentifier, [&] (DocumentIdentifier identifier) {
        auto* document = Document::allDocumentsMap().get(identifier);
        if (!document)
            return;
        task(*document);
        wasPosted = true;
    }, [&](ServiceWorkerIdentifier identifier) {
        wasPosted = SWContextManager::singleton().postTaskToServiceWorker(identifier, [task = WTFMove(task)](auto& scope) mutable {
            task(scope);
        });
    });
    return wasPosted;
}

SWClientConnection::SWClientConnection() = default;

SWClientConnection::~SWClientConnection() = default;

void SWClientConnection::scheduleJob(DocumentOrWorkerIdentifier contextIdentifier, const ServiceWorkerJobData& jobData)
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

void SWClientConnection::jobRejectedInServer(ServiceWorkerJobIdentifier jobIdentifier, const ExceptionData& exceptionData)
{
    postTaskForJob(jobIdentifier, IsJobComplete::Yes, [exceptionData = exceptionData.isolatedCopy()] (auto& job) {
        job.failedWithException(exceptionData.toException());
    });
}

void SWClientConnection::registrationJobResolvedInServer(ServiceWorkerJobIdentifier jobIdentifier, ServiceWorkerRegistrationData&& registrationData, ShouldNotifyWhenResolved shouldNotifyWhenResolved)
{
    bool isPosted = postTaskForJob(jobIdentifier, IsJobComplete::Yes, [registrationData = registrationData.isolatedCopy(), shouldNotifyWhenResolved] (auto& job) mutable {
        job.resolvedWithRegistration(WTFMove(registrationData), shouldNotifyWhenResolved);
    });

    if (!isPosted && shouldNotifyWhenResolved == ShouldNotifyWhenResolved::Yes)
        didResolveRegistrationPromise(registrationData.key);
}

void SWClientConnection::startScriptFetchForServer(ServiceWorkerJobIdentifier jobIdentifier, const ServiceWorkerRegistrationKey& registrationKey, FetchOptions::Cache cachePolicy)
{
    bool isPosted = postTaskForJob(jobIdentifier, IsJobComplete::No, [cachePolicy] (auto& job) {
        job.startScriptFetch(cachePolicy);
    });
    if (!isPosted)
        finishFetchingScriptInServer(serviceWorkerFetchError({ serverConnectionIdentifier(), jobIdentifier }, ServiceWorkerRegistrationKey { registrationKey}, ResourceError { errorDomainWebKitInternal, 0, { }, makeString("Failed to fetch script for service worker with scope ", registrationKey.scope().string()) }));
}


void SWClientConnection::postMessageToServiceWorkerClient(DocumentIdentifier destinationContextIdentifier, MessageWithMessagePorts&& message, ServiceWorkerData&& sourceData, String&& sourceOrigin)
{
    ASSERT(isMainThread());

    // FIXME: destinationContextIdentifier can only identify a Document at the moment.
    auto* destinationDocument = Document::allDocumentsMap().get(destinationContextIdentifier);
    if (!destinationDocument)
        return;

    if (auto* container = destinationDocument->ensureServiceWorkerContainer())
        container->postMessage(WTFMove(message), WTFMove(sourceData), WTFMove(sourceOrigin));
}

void SWClientConnection::updateRegistrationState(ServiceWorkerRegistrationIdentifier identifier, ServiceWorkerRegistrationState state, const Optional<ServiceWorkerData>& serviceWorkerData)
{
    ASSERT(isMainThread());

    SWContextManager::singleton().forEachServiceWorkerThread([identifier, state, &serviceWorkerData] (auto& workerThread) {
        workerThread.thread().runLoop().postTask([identifier, state, serviceWorkerData = crossThreadCopy(serviceWorkerData)](ScriptExecutionContext& context) mutable {
            if (auto* container = context.serviceWorkerContainer())
                container->updateRegistrationState(identifier, state, WTFMove(serviceWorkerData));
        });
    });

    for (auto* document : Document::allDocuments()) {
        if (auto* container = document->serviceWorkerContainer())
            container->updateRegistrationState(identifier, state, serviceWorkerData);
    }
}

void SWClientConnection::updateWorkerState(ServiceWorkerIdentifier identifier, ServiceWorkerState state)
{
    ASSERT(isMainThread());

    SWContextManager::singleton().forEachServiceWorkerThread([identifier, state] (auto& workerThread) {
        workerThread.thread().runLoop().postTask([identifier, state](ScriptExecutionContext& context) {
            if (auto* container = context.serviceWorkerContainer())
                container->updateWorkerState(identifier, state);
        });
    });

    for (auto* document : Document::allDocuments()) {
        if (auto* container = document->serviceWorkerContainer())
            container->updateWorkerState(identifier, state);
    }
}

void SWClientConnection::fireUpdateFoundEvent(ServiceWorkerRegistrationIdentifier identifier)
{
    ASSERT(isMainThread());

    SWContextManager::singleton().forEachServiceWorkerThread([identifier] (auto& workerThread) {
        workerThread.thread().runLoop().postTask([identifier](ScriptExecutionContext& context) {
            if (auto* container = context.serviceWorkerContainer())
                container->queueTaskToFireUpdateFoundEvent(identifier);
        });
    });

    for (auto* document : Document::allDocuments()) {
        if (auto* container = document->serviceWorkerContainer())
            container->queueTaskToFireUpdateFoundEvent(identifier);
    }
}

void SWClientConnection::setRegistrationLastUpdateTime(ServiceWorkerRegistrationIdentifier identifier, WallTime lastUpdateTime)
{
    ASSERT(isMainThread());

    SWContextManager::singleton().forEachServiceWorkerThread([identifier, lastUpdateTime] (auto& workerThread) {
        workerThread.thread().runLoop().postTask([identifier, lastUpdateTime](ScriptExecutionContext& context) {
            if (auto* container = context.serviceWorkerContainer()) {
                if (auto* registration = container->registration(identifier))
                    registration->setLastUpdateTime(lastUpdateTime);
            }
        });
    });

    for (auto* document : Document::allDocuments()) {
        if (auto* container = document->serviceWorkerContainer()) {
            if (auto* registration = container->registration(identifier))
                registration->setLastUpdateTime(lastUpdateTime);
        }
    }
}

void SWClientConnection::setRegistrationUpdateViaCache(ServiceWorkerRegistrationIdentifier identifier, ServiceWorkerUpdateViaCache updateViaCache)
{
    ASSERT(isMainThread());

    SWContextManager::singleton().forEachServiceWorkerThread([identifier, updateViaCache] (auto& workerThread) {
        workerThread.thread().runLoop().postTask([identifier, updateViaCache](ScriptExecutionContext& context) {
            if (auto* container = context.serviceWorkerContainer()) {
                if (auto* registration = container->registration(identifier))
                    registration->setUpdateViaCache(updateViaCache);
            }
        });
    });

    for (auto* document : Document::allDocuments()) {
        if (auto* container = document->serviceWorkerContainer()) {
            if (auto* registration = container->registration(identifier))
                registration->setUpdateViaCache(updateViaCache);
        }
    }
}

void SWClientConnection::notifyClientsOfControllerChange(const HashSet<DocumentIdentifier>& contextIdentifiers, ServiceWorkerData&& newController)
{
    ASSERT(isMainThread());
    ASSERT(!contextIdentifiers.isEmpty());

    for (auto& clientIdentifier : contextIdentifiers) {
        // FIXME: Support worker contexts.
        auto* client = Document::allDocumentsMap().get(clientIdentifier);
        if (!client)
            continue;

        client->setActiveServiceWorker(ServiceWorker::getOrCreate(*client, WTFMove(newController)));
        if (auto* container = client->serviceWorkerContainer())
            container->queueTaskToDispatchControllerChangeEvent();
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
    for (auto* document : Document::allDocuments()) {
        auto controllingServiceWorkerRegistrationIdentifier = document->activeServiceWorker() ? makeOptional<ServiceWorkerRegistrationIdentifier>(document->activeServiceWorker()->registrationIdentifier()) : WTF::nullopt;
        registerServiceWorkerClient(document->topOrigin(), ServiceWorkerClientData::from(*document, *this), controllingServiceWorkerRegistrationIdentifier, document->userAgent(document->url()));
    }
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
