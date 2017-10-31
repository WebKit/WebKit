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
#include "SWServerRegistration.h"

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

SWServerRegistration::SWServerRegistration(SWServer& server, const ServiceWorkerRegistrationKey& key)
    : m_jobTimer(*this, &SWServerRegistration::startNextJob)
    , m_server(server)
    , m_registrationKey(key)
{
}

SWServerRegistration::~SWServerRegistration()
{
    ASSERT(m_jobQueue.isEmpty());
}

void SWServerRegistration::enqueueJob(const ServiceWorkerJobData& jobData)
{
    // FIXME: Per the spec, check if this job is equivalent to the last job on the queue.
    // If it is, stack it along with that job.

    m_jobQueue.append(jobData);

    if (m_currentJob)
        return;

    if (!m_jobTimer.isActive())
        m_jobTimer.startOneShot(0_s);
}

void SWServerRegistration::scriptFetchFinished(SWServer::Connection& connection, const ServiceWorkerFetchResult& result)
{
    ASSERT(m_currentJob && m_currentJob->identifier() == result.jobIdentifier);

    if (!result.scriptError.isNull()) {
        rejectCurrentJob(ExceptionData { UnknownError, makeString("Script URL ", m_currentJob->scriptURL.string(), " fetch resulted in error: ", result.scriptError.localizedDescription()) });
        
        // If newestWorker is null, invoke Clear Registration algorithm passing this registration as its argument.
        // FIXME: We don't have "clear registration" yet.

        return;
    }

    m_lastUpdateTime = currentTime();
    
    // FIXME: If the script data matches byte-for-byte with the existing newestWorker,
    // then resolve and finish the job without doing anything further.

    // FIXME: Support the proper worker type (classic vs module)
    m_server.createWorker(connection, m_registrationKey, m_currentJob->scriptURL, result.script, WorkerType::Classic);
}

void SWServerRegistration::scriptContextFailedToStart(SWServer::Connection&, const String& workerID, const String& message)
{
    UNUSED_PARAM(workerID);

    rejectCurrentJob(ExceptionData { UnknownError, message });
}

void SWServerRegistration::scriptContextStarted(SWServer::Connection&, uint64_t identifier, const String& workerID)
{
    UNUSED_PARAM(workerID);
    resolveCurrentRegistrationJob(ServiceWorkerRegistrationData { m_registrationKey, identifier, m_scopeURL, m_scriptURL, m_updateViaCache.value_or(ServiceWorkerUpdateViaCache::Imports) });
}

void SWServerRegistration::startNextJob()
{
    ASSERT(isMainThread());
    ASSERT(!m_currentJob);
    ASSERT(!m_jobQueue.isEmpty());

    m_currentJob = std::make_unique<ServiceWorkerJobData>(m_jobQueue.takeFirst().isolatedCopy());

    switch (m_currentJob->type) {
    case ServiceWorkerJobType::Register:
        m_server.postTask(createCrossThreadTask(*this, &SWServerRegistration::runRegisterJob, *m_currentJob));
        return;
    case ServiceWorkerJobType::Unregister:
        m_server.postTask(createCrossThreadTask(*this, &SWServerRegistration::runUnregisterJob, *m_currentJob));
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

bool SWServerRegistration::isEmpty()
{
    ASSERT(!isMainThread());

    // Having or not-having an m_updateViaCache flag is currently
    // the signal as to whether or not this is an empty (i.e. "new") registration.
    // There will be a more explicit signal in the near future.
    return !m_updateViaCache;
}

SWServerWorker* SWServerRegistration::getNewestWorker()
{
    ASSERT(!isMainThread());
    if (m_installingWorker)
        return m_installingWorker.get();
    if (m_waitingWorker)
        return m_waitingWorker.get();

    return m_activeWorker.get();
}

void SWServerRegistration::runRegisterJob(const ServiceWorkerJobData& job)
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
    if (!isEmpty()) {
        ASSERT(m_updateViaCache);

        m_uninstalling = false;
        auto* newestWorker = getNewestWorker();
        if (newestWorker && equalIgnoringFragmentIdentifier(job.scriptURL, newestWorker->scriptURL()) && job.registrationOptions.updateViaCache == *m_updateViaCache) {
            resolveWithRegistrationOnMainThread();
            return;
        }
    } else {
        m_scopeURL = job.scopeURL.isolatedCopy();
        m_scopeURL.removeFragmentIdentifier();
        m_scriptURL = job.scriptURL.isolatedCopy();
        m_updateViaCache = job.registrationOptions.updateViaCache;
    }

    runUpdateJob(job);
}

void SWServerRegistration::runUnregisterJob(const ServiceWorkerJobData& job)
{
    // If the origin of job’s scope url is not job's client's origin, then:
    if (!protocolHostAndPortAreEqual(job.scopeURL, job.clientCreationURL))
        return rejectWithExceptionOnMainThread(ExceptionData { SecurityError, ASCIILiteral("Origin of scope URL does not match the client's origin") });

    // Let registration be the result of running "Get Registration" algorithm passing job’s scope url as the argument.
    // If registration is null, then:
    if (isEmpty() || m_uninstalling) {
        // Invoke Resolve Job Promise with job and false.
        resolveWithUnregistrationResultOnMainThread(false);
        return;
    }

    // Set registration’s uninstalling flag.
    m_uninstalling = true;

    // Invoke Resolve Job Promise with job and true.
    resolveWithUnregistrationResultOnMainThread(true);

    // FIXME: Invoke Try Clear Registration with registration.
}

void SWServerRegistration::runUpdateJob(const ServiceWorkerJobData& job)
{
    // If registration is null (in our parlance "empty") or registration’s uninstalling flag is set, then:
    if (isEmpty())
        return rejectWithExceptionOnMainThread(ExceptionData { TypeError, ASCIILiteral("Cannot update a null/nonexistent service worker registration") });
    if (m_uninstalling)
        return rejectWithExceptionOnMainThread(ExceptionData { TypeError, ASCIILiteral("Cannot update a service worker registration that is uninstalling") });

    // If job’s job type is update, and newestWorker’s script url does not equal job’s script url with the exclude fragments flag set, then:
    auto* newestWorker = getNewestWorker();
    if (newestWorker && !equalIgnoringFragmentIdentifier(job.scriptURL, newestWorker->scriptURL()))
        return rejectWithExceptionOnMainThread(ExceptionData { TypeError, ASCIILiteral("Cannot update a service worker with a requested script URL whose newest worker has a different script URL") });

    startScriptFetchFromMainThread();
}

void SWServerRegistration::rejectWithExceptionOnMainThread(const ExceptionData& exception)
{
    ASSERT(!isMainThread());
    m_server.postTaskReply(createCrossThreadTask(*this, &SWServerRegistration::rejectCurrentJob, exception));
}

void SWServerRegistration::resolveWithRegistrationOnMainThread()
{
    ASSERT(!isMainThread());
    m_server.postTaskReply(createCrossThreadTask(*this, &SWServerRegistration::resolveCurrentRegistrationJob, data()));
}

void SWServerRegistration::resolveWithUnregistrationResultOnMainThread(bool unregistrationResult)
{
    ASSERT(!isMainThread());
    m_server.postTaskReply(createCrossThreadTask(*this, &SWServerRegistration::resolveCurrentUnregistrationJob, unregistrationResult));
}

void SWServerRegistration::startScriptFetchFromMainThread()
{
    ASSERT(!isMainThread());
    m_server.postTaskReply(createCrossThreadTask(*this, &SWServerRegistration::startScriptFetchForCurrentJob));
}

void SWServerRegistration::rejectCurrentJob(const ExceptionData& exceptionData)
{
    ASSERT(isMainThread());
    ASSERT(m_currentJob);

    m_server.rejectJob(*m_currentJob, exceptionData);

    finishCurrentJob();
}

void SWServerRegistration::resolveCurrentRegistrationJob(const ServiceWorkerRegistrationData& data)
{
    ASSERT(isMainThread());
    ASSERT(m_currentJob);
    ASSERT(m_currentJob->type == ServiceWorkerJobType::Register);

    m_server.resolveRegistrationJob(*m_currentJob, data);

    finishCurrentJob();
}

void SWServerRegistration::resolveCurrentUnregistrationJob(bool unregistrationResult)
{
    ASSERT(isMainThread());
    ASSERT(m_currentJob);
    ASSERT(m_currentJob->type == ServiceWorkerJobType::Unregister);

    m_server.resolveUnregistrationJob(*m_currentJob, m_registrationKey, unregistrationResult);

    finishCurrentJob();
}

void SWServerRegistration::startScriptFetchForCurrentJob()
{
    ASSERT(isMainThread());
    ASSERT(m_currentJob);

    m_server.startScriptFetch(*m_currentJob);
}

void SWServerRegistration::finishCurrentJob()
{
    ASSERT(m_currentJob);
    ASSERT(!m_jobTimer.isActive());

    m_currentJob = nullptr;
    if (m_jobQueue.isEmpty())
        return;

    startNextJob();
}

ServiceWorkerRegistrationData SWServerRegistration::data() const
{
    return { m_registrationKey, identifier(), m_scopeURL, m_scriptURL, m_updateViaCache.value_or(ServiceWorkerUpdateViaCache::Imports) };
}


} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
