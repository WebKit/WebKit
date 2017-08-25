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

namespace WebCore {

SWServerRegistration::SWServerRegistration(SWServer& server)
    : m_jobTimer(*this, &SWServerRegistration::startNextJob)
    , m_server(server)
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

void SWServerRegistration::startNextJob()
{
    ASSERT(isMainThread());
    ASSERT(!m_currentJob);
    ASSERT(!m_jobQueue.isEmpty());

    m_currentJob = std::make_unique<ServiceWorkerJobData>(m_jobQueue.takeFirst().isolatedCopy());
    m_server.postTask(createCrossThreadTask(*this, &SWServerRegistration::performCurrentJob));
}

void SWServerRegistration::performCurrentJob()
{
    ASSERT(!isMainThread());

    auto exception = ExceptionData { UnknownError, ASCIILiteral("serviceWorker job scheduling is not yet implemented") };
    m_server.postTaskReply(createCrossThreadTask(*this, &SWServerRegistration::rejectCurrentJob, exception));
}

void SWServerRegistration::rejectCurrentJob(const ExceptionData& exceptionData)
{
    ASSERT(isMainThread());
    ASSERT(m_currentJob);

    m_server.rejectJob(*m_currentJob, exceptionData);

    finishCurrentJob();
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

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
