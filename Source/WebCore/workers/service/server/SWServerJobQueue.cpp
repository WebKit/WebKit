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
#include "SWServerJobQueue.h"

#if ENABLE(SERVICE_WORKER)

#include "ExceptionData.h"
#include "SWServer.h"
#include "SWServerWorker.h"
#include "SecurityOrigin.h"
#include "ServiceWorkerFetchResult.h"
#include "ServiceWorkerRegistrationData.h"
#include "ServiceWorkerUpdateViaCache.h"
#include "WorkerType.h"

namespace WebCore {

SWServerJobQueue::SWServerJobQueue(SWServer& server, const ServiceWorkerRegistrationKey& key)
    : m_jobTimer(*this, &SWServerJobQueue::startNextJob)
    , m_server(server)
    , m_registrationKey(key)
{
}

SWServerJobQueue::~SWServerJobQueue()
{
    ASSERT(m_jobQueue.isEmpty());
}

void SWServerJobQueue::enqueueJob(const ServiceWorkerJobData& jobData)
{
    // FIXME: Per the spec, check if this job is equivalent to the last job on the queue.
    // If it is, stack it along with that job.

    m_jobQueue.append(jobData);

    if (m_currentJob)
        return;

    if (!m_jobTimer.isActive())
        m_jobTimer.startOneShot(0_s);
}

void SWServerJobQueue::scriptFetchFinished(SWServer::Connection& connection, const ServiceWorkerFetchResult& result)
{
    ASSERT(isMainThread());
    ASSERT(m_currentJob && m_currentJob->identifier() == result.jobIdentifier);

    if (!result.scriptError.isNull()) {
        rejectCurrentJob(ExceptionData { UnknownError, makeString("Script URL ", m_currentJob->scriptURL.string(), " fetch resulted in error: ", result.scriptError.localizedDescription()) });

        // If newestWorker is null, invoke Clear Registration algorithm passing this registration as its argument.
        // FIXME: We don't have "clear registration" yet.

        return;
    }

    // FIXME: If the script data matches byte-for-byte with the existing newestWorker,
    // then resolve and finish the job without doing anything further.

    // FIXME: Support the proper worker type (classic vs module)
    m_server.updateWorker(connection, m_registrationKey, m_currentJob->scriptURL, result.script, WorkerType::Classic);
}

void SWServerJobQueue::scriptContextFailedToStart(SWServer::Connection&, const String& workerID, const String& message)
{
    ASSERT(isMainThread());
    // FIXME: Install has failed. Run the install failed substeps
    // Run the Update Worker State algorithm passing registration’s installing worker and redundant as the arguments.
    // Run the Update Registration State algorithm passing registration, "installing" and null as the arguments.
    // If newestWorker is null, invoke Clear Registration algorithm passing registration as its argument.

    UNUSED_PARAM(workerID);
    UNUSED_PARAM(message);
}

void SWServerJobQueue::scriptContextStarted(SWServer::Connection&, uint64_t serviceWorkerIdentifier, const String& workerID)
{
    ASSERT(isMainThread());
    UNUSED_PARAM(workerID);

    m_server.postTask(createCrossThreadTask(*this, &SWServerJobQueue::resolveCurrentRegistrationJobOnMainThead, serviceWorkerIdentifier));
}

void SWServerJobQueue::startNextJob()
{
    ASSERT(isMainThread());
    ASSERT(!m_currentJob);
    ASSERT(!m_jobQueue.isEmpty());

    m_currentJob = std::make_unique<ServiceWorkerJobData>(m_jobQueue.takeFirst().isolatedCopy());

    switch (m_currentJob->type) {
    case ServiceWorkerJobType::Register:
        m_server.postTask(createCrossThreadTask(*this, &SWServerJobQueue::runRegisterJob, *m_currentJob));
        return;
    case ServiceWorkerJobType::Unregister:
        m_server.postTask(createCrossThreadTask(*this, &SWServerJobQueue::runUnregisterJob, *m_currentJob));
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void SWServerJobQueue::runRegisterJob(const ServiceWorkerJobData& job)
{
    ASSERT(!isMainThread());
    ASSERT(job.type == ServiceWorkerJobType::Register);

    if (!shouldTreatAsPotentiallyTrustworthy(job.scriptURL))
        return rejectWithExceptionOnMainThread(ExceptionData { SecurityError, ASCIILiteral("Script URL is not potentially trustworthy") });

    // If the origin of job’s script url is not job’s referrer's origin, then:
    if (!protocolHostAndPortAreEqual(job.scriptURL, job.clientCreationURL))
        return rejectWithExceptionOnMainThread(ExceptionData { SecurityError, ASCIILiteral("Script origin does not match the registering client's origin") });

    // If the origin of job’s scope url is not job’s referrer's origin, then:
    if (!protocolHostAndPortAreEqual(job.scopeURL, job.clientCreationURL))
        return rejectWithExceptionOnMainThread(ExceptionData { SecurityError, ASCIILiteral("Scope origin does not match the registering client's origin") });

    // If registration is not null (in our parlance "empty"), then:
    if (auto* registration = m_server.getRegistration(m_registrationKey)) {
        registration->setIsUninstalling(false);
        auto* newestWorker = registration->getNewestWorker();
        if (newestWorker && equalIgnoringFragmentIdentifier(job.scriptURL, newestWorker->scriptURL()) && job.registrationOptions.updateViaCache == registration->updateViaCache()) {
            resolveWithRegistrationOnMainThread(*registration);
            return;
        }
    } else {
        auto newRegistration = std::make_unique<SWServerRegistration>(m_registrationKey, job.registrationOptions.updateViaCache, job.scopeURL, job.scriptURL);
        m_server.addRegistration(WTFMove(newRegistration));
    }

    runUpdateJob(job);
}

void SWServerJobQueue::runUnregisterJob(const ServiceWorkerJobData& job)
{
    ASSERT(!isMainThread());

    // If the origin of job’s scope url is not job's client's origin, then:
    if (!protocolHostAndPortAreEqual(job.scopeURL, job.clientCreationURL))
        return rejectWithExceptionOnMainThread(ExceptionData { SecurityError, ASCIILiteral("Origin of scope URL does not match the client's origin") });

    // Let registration be the result of running "Get Registration" algorithm passing job’s scope url as the argument.
    auto* registration = m_server.getRegistration(m_registrationKey);

    // If registration is null, then:
    if (!registration || registration->isUninstalling()) {
        // Invoke Resolve Job Promise with job and false.
        resolveWithUnregistrationResultOnMainThread(false);
        return;
    }

    // Set registration’s uninstalling flag.
    registration->setIsUninstalling(true);

    // Invoke Resolve Job Promise with job and true.
    resolveWithUnregistrationResultOnMainThread(true);

    // FIXME: Invoke Try Clear Registration with registration.
}

void SWServerJobQueue::runUpdateJob(const ServiceWorkerJobData& job)
{
    auto* registration = m_server.getRegistration(m_registrationKey);

    // If registration is null (in our parlance "empty") or registration’s uninstalling flag is set, then:
    if (!registration)
        return rejectWithExceptionOnMainThread(ExceptionData { TypeError, ASCIILiteral("Cannot update a null/nonexistent service worker registration") });
    if (registration->isUninstalling())
        return rejectWithExceptionOnMainThread(ExceptionData { TypeError, ASCIILiteral("Cannot update a service worker registration that is uninstalling") });

    // If job’s job type is update, and newestWorker’s script url does not equal job’s script url with the exclude fragments flag set, then:
    auto* newestWorker = registration->getNewestWorker();
    if (newestWorker && !equalIgnoringFragmentIdentifier(job.scriptURL, newestWorker->scriptURL()))
        return rejectWithExceptionOnMainThread(ExceptionData { TypeError, ASCIILiteral("Cannot update a service worker with a requested script URL whose newest worker has a different script URL") });

    startScriptFetchFromMainThread();
}

void SWServerJobQueue::rejectWithExceptionOnMainThread(const ExceptionData& exception)
{
    ASSERT(!isMainThread());
    m_server.postTaskReply(createCrossThreadTask(*this, &SWServerJobQueue::rejectCurrentJob, exception));
}

void SWServerJobQueue::resolveWithRegistrationOnMainThread(SWServerRegistration& registration)
{
    ASSERT(!isMainThread());
    m_server.postTaskReply(createCrossThreadTask(*this, &SWServerJobQueue::resolveCurrentRegistrationJob, registration.data()));
}

void SWServerJobQueue::resolveCurrentRegistrationJobOnMainThead(uint64_t serviceWorkerIdentifier)
{
    ASSERT(!isMainThread());
    auto* registration = m_server.getRegistration(m_registrationKey);
    ASSERT(registration);
    registration->setActiveServiceWorkerIdentifier(serviceWorkerIdentifier);
    resolveWithRegistrationOnMainThread(*registration);
}

void SWServerJobQueue::resolveWithUnregistrationResultOnMainThread(bool unregistrationResult)
{
    ASSERT(!isMainThread());
    m_server.postTaskReply(createCrossThreadTask(*this, &SWServerJobQueue::resolveCurrentUnregistrationJob, unregistrationResult));
}

void SWServerJobQueue::startScriptFetchFromMainThread()
{
    ASSERT(!isMainThread());
    m_server.postTaskReply(createCrossThreadTask(*this, &SWServerJobQueue::startScriptFetchForCurrentJob));
}

void SWServerJobQueue::rejectCurrentJob(const ExceptionData& exceptionData)
{
    ASSERT(isMainThread());
    ASSERT(m_currentJob);

    m_server.rejectJob(*m_currentJob, exceptionData);

    finishCurrentJob();
}

void SWServerJobQueue::resolveCurrentRegistrationJob(const ServiceWorkerRegistrationData& data)
{
    ASSERT(isMainThread());
    ASSERT(m_currentJob);
    ASSERT(m_currentJob->type == ServiceWorkerJobType::Register);

    m_server.resolveRegistrationJob(*m_currentJob, data);

    finishCurrentJob();
}

void SWServerJobQueue::resolveCurrentUnregistrationJob(bool unregistrationResult)
{
    ASSERT(isMainThread());
    ASSERT(m_currentJob);
    ASSERT(m_currentJob->type == ServiceWorkerJobType::Unregister);

    m_server.resolveUnregistrationJob(*m_currentJob, m_registrationKey, unregistrationResult);

    finishCurrentJob();
}

void SWServerJobQueue::startScriptFetchForCurrentJob()
{
    ASSERT(isMainThread());
    ASSERT(m_currentJob);

    m_server.startScriptFetch(*m_currentJob);
}

void SWServerJobQueue::finishCurrentJob()
{
    ASSERT(isMainThread());
    ASSERT(m_currentJob);
    ASSERT(!m_jobTimer.isActive());

    m_currentJob = nullptr;
    if (m_jobQueue.isEmpty())
        return;

    startNextJob();
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
