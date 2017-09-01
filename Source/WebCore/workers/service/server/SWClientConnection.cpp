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

#include "ExceptionData.h"
#include "ServiceWorkerFetchResult.h"
#include "ServiceWorkerJobData.h"
#include "SharedBuffer.h"

namespace WebCore {

SWClientConnection::SWClientConnection()
{
}

SWClientConnection::~SWClientConnection()
{
}

void SWClientConnection::scheduleJob(ServiceWorkerJob& job)
{
    auto addResult = m_scheduledJobs.add(job.data().identifier(), &job);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);

    scheduleJobInServer(job.data());
}

void SWClientConnection::finishedFetchingScript(ServiceWorkerJob& job, SharedBuffer& data)
{
    ASSERT(m_scheduledJobs.get(job.data().identifier()) == &job);

    Vector<uint8_t> vector;
    vector.append(reinterpret_cast<const uint8_t *>(data.data()), data.size());
    finishFetchingScriptInServer({ job.data().identifier(), job.data().connectionIdentifier(), job.data().registrationKey(), vector, { } });
}

void SWClientConnection::failedFetchingScript(ServiceWorkerJob& job, const ResourceError& error)
{
    ASSERT(m_scheduledJobs.get(job.data().identifier()) == &job);

    finishFetchingScriptInServer({ job.data().identifier(), job.data().connectionIdentifier(), job.data().registrationKey(), { }, error });
}

void SWClientConnection::jobRejectedInServer(uint64_t jobIdentifier, const ExceptionData& exceptionData)
{
    auto job = m_scheduledJobs.take(jobIdentifier);
    if (!job) {
        LOG_ERROR("Job %" PRIu64 " rejected from server, but was not found", jobIdentifier);
        return;
    }

    job->failedWithException(exceptionData.toException());
}

void SWClientConnection::jobResolvedInServer(uint64_t jobIdentifier, const ServiceWorkerRegistrationData& registrationData)
{
    auto job = m_scheduledJobs.take(jobIdentifier);
    if (!job) {
        LOG_ERROR("Job %" PRIu64 " resolved in server, but was not found", jobIdentifier);
        return;
    }

    job->resolvedWithRegistration(registrationData);
}

void SWClientConnection::startScriptFetchForServer(uint64_t jobIdentifier)
{
    auto job = m_scheduledJobs.get(jobIdentifier);
    if (!job) {
        LOG_ERROR("Job %" PRIu64 " instructed to start fetch from server, but job was not found", jobIdentifier);

        // FIXME: Should message back to the server here to signal failure to fetch,
        // but we currently need the registration key to do so, and don't have it here.
        // In the future we'll refactor to have a global, cross-process job identifier that can be used to overcome this.

        return;
    }

    job->startScriptFetch();
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
