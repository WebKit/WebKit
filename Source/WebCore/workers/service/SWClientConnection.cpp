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
#include "Microtasks.h"
#include "SWContextManager.h"
#include "ServiceWorkerContainer.h"
#include "ServiceWorkerFetchResult.h"
#include "ServiceWorkerJobData.h"
#include "ServiceWorkerRegistration.h"
#include <wtf/CrossThreadCopier.h>

namespace WebCore {

SWClientConnection::SWClientConnection() = default;

SWClientConnection::~SWClientConnection() = default;

void SWClientConnection::scheduleJob(ServiceWorkerJob& job)
{
    ASSERT(isMainThread());

    auto addResult = m_scheduledJobs.add(job.identifier(), &job);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);

    scheduleJobInServer(job.data());
}

void SWClientConnection::finishedFetchingScript(ServiceWorkerJob& job, const String& script)
{
    ASSERT(isMainThread());
    ASSERT(m_scheduledJobs.get(job.identifier()) == &job);

    finishFetchingScriptInServer({ job.data().identifier(), job.data().registrationKey(), script, { } });
}

void SWClientConnection::failedFetchingScript(ServiceWorkerJob& job, const ResourceError& error)
{
    ASSERT(isMainThread());
    ASSERT(m_scheduledJobs.get(job.identifier()) == &job);

    finishFetchingScriptInServer({ job.data().identifier(), job.data().registrationKey(), { }, error });
}

void SWClientConnection::jobRejectedInServer(const ServiceWorkerJobDataIdentifier& jobDataIdentifier, const ExceptionData& exceptionData)
{
    ASSERT(isMainThread());

    auto job = m_scheduledJobs.take(jobDataIdentifier.jobIdentifier);
    if (!job) {
        LOG_ERROR("Job %s rejected from server, but was not found", jobDataIdentifier.loggingString().utf8().data());
        return;
    }

    ScriptExecutionContext::postTaskTo(job->contextIdentifier(), [job, exceptionData = exceptionData.isolatedCopy()](ScriptExecutionContext&) {
        job->failedWithException(exceptionData.toException());
    });
}

void SWClientConnection::registrationJobResolvedInServer(const ServiceWorkerJobDataIdentifier& jobDataIdentifier, ServiceWorkerRegistrationData&& registrationData, ShouldNotifyWhenResolved shouldNotifyWhenResolved)
{
    ASSERT(isMainThread());

    auto job = m_scheduledJobs.take(jobDataIdentifier.jobIdentifier);
    if (!job) {
        LOG_ERROR("Job %s resolved in server, but was not found", jobDataIdentifier.loggingString().utf8().data());
        return;
    }

    ScriptExecutionContext::postTaskTo(job->contextIdentifier(), [job, registrationData = registrationData.isolatedCopy(), shouldNotifyWhenResolved](ScriptExecutionContext&) mutable {
        job->resolvedWithRegistration(WTFMove(registrationData), shouldNotifyWhenResolved);
    });
}

void SWClientConnection::unregistrationJobResolvedInServer(const ServiceWorkerJobDataIdentifier& jobDataIdentifier, bool unregistrationResult)
{
    ASSERT(isMainThread());

    auto job = m_scheduledJobs.take(jobDataIdentifier.jobIdentifier);
    if (!job) {
        LOG_ERROR("Job %s resolved in server, but was not found", jobDataIdentifier.loggingString().utf8().data());
        return;
    }

    ScriptExecutionContext::postTaskTo(job->contextIdentifier(), [job, unregistrationResult](ScriptExecutionContext&) {
        job->resolvedWithUnregistrationResult(unregistrationResult);
    });
}

void SWClientConnection::startScriptFetchForServer(const ServiceWorkerJobDataIdentifier& jobDataIdentifier)
{
    ASSERT(isMainThread());

    auto job = m_scheduledJobs.get(jobDataIdentifier.jobIdentifier);
    if (!job) {
        LOG_ERROR("Job %s instructed to start fetch from server, but job was not found", jobDataIdentifier.loggingString().utf8().data());

        // FIXME: Should message back to the server here to signal failure to fetch,
        // but we currently need the registration key to do so, and don't have it here.
        // In the future we'll refactor to have a global, cross-process job identifier that can be used to overcome this.

        return;
    }

    ScriptExecutionContext::postTaskTo(job->contextIdentifier(), [job](ScriptExecutionContext&) {
        job->startScriptFetch();
    });
}

void SWClientConnection::postMessageToServiceWorkerClient(DocumentIdentifier destinationContextIdentifier, Ref<SerializedScriptValue>&& message, ServiceWorkerData&& sourceData, const String& sourceOrigin)
{
    ASSERT(isMainThread());

    // FIXME: destinationContextIdentifier can only identify a Document at the moment.
    auto* destinationDocument = Document::allDocumentsMap().get(destinationContextIdentifier);
    if (!destinationDocument)
        return;

    auto* container = destinationDocument->serviceWorkerContainer();
    if (!container)
        return;

    MessageEventSource source = RefPtr<ServiceWorker> { ServiceWorker::getOrCreate(*destinationDocument, WTFMove(sourceData)) };

    // FIXME: We should pass in ports.
    auto messageEvent = MessageEvent::create({ }, WTFMove(message), sourceOrigin, { }, WTFMove(source));
    container->dispatchEvent(messageEvent);
}

void SWClientConnection::updateRegistrationState(ServiceWorkerRegistrationIdentifier identifier, ServiceWorkerRegistrationState state, const std::optional<ServiceWorkerData>& serviceWorkerData)
{
    ASSERT(isMainThread());

    SWContextManager::singleton().forEachServiceWorkerThread([identifier, state, &serviceWorkerData] (auto& workerThread) {
        workerThread.thread().runLoop().postTask([identifier, state, serviceWorkerData = crossThreadCopy(serviceWorkerData)](ScriptExecutionContext& context) mutable {
            if (auto* container = context.serviceWorkerContainer())
                container->scheduleTaskToUpdateRegistrationState(identifier, state, WTFMove(serviceWorkerData));
        });
    });

    for (auto* document : Document::allDocuments()) {
        if (auto* container = document->serviceWorkerContainer())
            container->scheduleTaskToUpdateRegistrationState(identifier, state, serviceWorkerData);
    }
}

void SWClientConnection::updateWorkerState(ServiceWorkerIdentifier identifier, ServiceWorkerState state)
{
    ASSERT(isMainThread());

    SWContextManager::singleton().forEachServiceWorkerThread([identifier, state] (auto& workerThread) {
        workerThread.thread().runLoop().postTask([identifier, state](ScriptExecutionContext& context) {
            if (auto* serviceWorker = context.serviceWorker(identifier))
                serviceWorker->scheduleTaskToUpdateState(state);
        });
    });

    for (auto* document : Document::allDocuments()) {
        if (auto* serviceWorker = document->serviceWorker(identifier))
            serviceWorker->scheduleTaskToUpdateState(state);
    }
}

void SWClientConnection::fireUpdateFoundEvent(ServiceWorkerRegistrationIdentifier identifier)
{
    ASSERT(isMainThread());

    SWContextManager::singleton().forEachServiceWorkerThread([identifier] (auto& workerThread) {
        workerThread.thread().runLoop().postTask([identifier](ScriptExecutionContext& context) {
            if (auto* container = context.serviceWorkerContainer())
                container->scheduleTaskToFireUpdateFoundEvent(identifier);
        });
    });

    for (auto* document : Document::allDocuments()) {
        if (auto* container = document->serviceWorkerContainer())
            container->scheduleTaskToFireUpdateFoundEvent(identifier);
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

        ASSERT(client->activeServiceWorker());
        ASSERT(client->activeServiceWorker()->identifier() != newController.identifier);
        client->setActiveServiceWorker(ServiceWorker::getOrCreate(*client, ServiceWorkerData { newController }));
        if (auto* container = client->serviceWorkerContainer())
            container->scheduleTaskToFireControllerChangeEvent();
    }
}

void SWClientConnection::clearPendingJobs()
{
    ASSERT(isMainThread());

    auto jobs = WTFMove(m_scheduledJobs);
    for (auto& job : jobs.values()) {
        ScriptExecutionContext::postTaskTo(job->contextIdentifier(), [job](ScriptExecutionContext&) {
            job->failedWithException(Exception { TypeError, ASCIILiteral("Internal error") });
        });
    }
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
