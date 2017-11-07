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
    : m_jobTimer(*this, &SWServerJobQueue::runNextJobSynchronously)
    , m_server(server)
    , m_registrationKey(key)
{
}

SWServerJobQueue::~SWServerJobQueue()
{
}

void SWServerJobQueue::scriptFetchFinished(SWServer::Connection& connection, const ServiceWorkerFetchResult& result)
{
    auto& job = firstJob();
    ASSERT(job.identifier() == result.jobIdentifier);

    auto* registration = m_server.getRegistration(m_registrationKey);
    ASSERT(registration);

    auto* newestWorker = registration->getNewestWorker();

    if (!result.scriptError.isNull()) {
        // Invoke Reject Job Promise with job and TypeError.
        m_server.rejectJob(firstJob(), ExceptionData { TypeError, makeString("Script URL ", job.scriptURL.string(), " fetch resulted in error: ", result.scriptError.localizedDescription()) });

        // If newestWorker is null, invoke Clear Registration algorithm passing registration as its argument.
        if (!newestWorker)
            clearRegistration(*registration);

        // Invoke Finish Job with job and abort these steps.
        finishCurrentJob();
        return;
    }

    // FIXME: If newestWorker is not null, newestWorker's script url equals job's script url with the exclude fragments
    // flag set, and script's source text is a byte-for-byte match with newestWorker's script resource's source
    // text, then:
    // - Invoke Resolve Job Promise with job and registration.
    // - Invoke Finish Job with job and abort these steps.

    // FIXME: Support the proper worker type (classic vs module)
    m_server.updateWorker(connection, m_registrationKey, job.scriptURL, result.script, WorkerType::Classic);
}

void SWServerJobQueue::scriptContextFailedToStart(SWServer::Connection&, ServiceWorkerIdentifier identifier, const String& message)
{
    auto* registration = m_server.getRegistration(m_registrationKey);
    ASSERT(registration);

    // FIXME: Install has failed. Run the install failed substeps
    // Run the Update Worker State algorithm passing registration's installing worker and redundant as the arguments.
    // Run the Update Registration State algorithm passing registration, "installing" and null as the arguments.

    // If newestWorker is null, invoke Clear Registration algorithm passing registration as its argument.
    if (!registration->getNewestWorker())
        clearRegistration(*registration);

    UNUSED_PARAM(identifier);
    UNUSED_PARAM(message);
}

void SWServerJobQueue::scriptContextStarted(SWServer::Connection& connection, ServiceWorkerIdentifier identifier)
{
    auto* registration = m_server.getRegistration(m_registrationKey);
    ASSERT(registration);

    install(*registration, connection, identifier);
}

// https://w3c.github.io/ServiceWorker/#install
void SWServerJobQueue::install(SWServerRegistration& registration, SWServer::Connection& connection, ServiceWorkerIdentifier installingWorker)
{
    // The Install algorithm should never be invoked with a null worker.
    auto* worker = m_server.workerByID(installingWorker);
    RELEASE_ASSERT(worker);

    registration.updateRegistrationState(ServiceWorkerRegistrationState::Installing, worker);
    registration.updateWorkerState(*worker, ServiceWorkerState::Installing);

    // Invoke Resolve Job Promise with job and registration.
    m_server.resolveRegistrationJob(firstJob(), registration.data());

    // Queue a task to fire an event named updatefound at all the ServiceWorkerRegistration objects
    // for all the service worker clients whose creation URL matches registration's scope url and
    // all the service workers whose containing service worker registration is registration.
    registration.fireUpdateFoundEvent(firstJob().connectionIdentifier());

    // Queue a task to fire the InstallEvent.
    m_server.fireInstallEvent(connection, installingWorker);
}

// https://w3c.github.io/ServiceWorker/#install
void SWServerJobQueue::didFinishInstall(SWServer::Connection&, ServiceWorkerIdentifier, bool wasSuccessful)
{
    auto* registration = m_server.getRegistration(m_registrationKey);
    ASSERT(registration);

    if (!wasSuccessful) {
        // FIXME: Run the Update Worker State algorithm passing registration's installing worker and redundant as the arguments.
        // FIXME: Run the Update Registration State algorithm passing registration, "installing" and null as the arguments.

        // If newestWorker is null, invoke Clear Registration algorithm passing registration as its argument.
        if (!registration->getNewestWorker())
            clearRegistration(*registration);
        // Invoke Finish Job with job and abort these steps.
        finishCurrentJob();
        return;
    }

    // FIXME: Implement real post 'install' event steps of the Install algorithm (steps 14+).
    registration->firePostInstallEvents(firstJob().connectionIdentifier());

    finishCurrentJob();
}

// https://w3c.github.io/ServiceWorker/#run-job
void SWServerJobQueue::runNextJob()
{
    ASSERT(!m_jobQueue.isEmpty());
    ASSERT(!m_jobTimer.isActive());
    m_jobTimer.startOneShot(0_s);
}

void SWServerJobQueue::runNextJobSynchronously()
{
    auto& job = firstJob();
    switch (job.type) {
    case ServiceWorkerJobType::Register:
        runRegisterJob(job);
        return;
    case ServiceWorkerJobType::Unregister:
        runUnregisterJob(job);
        return;
    case ServiceWorkerJobType::Update:
        runUpdateJob(job);
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

// https://w3c.github.io/ServiceWorker/#register-algorithm
void SWServerJobQueue::runRegisterJob(const ServiceWorkerJobData& job)
{
    ASSERT(job.type == ServiceWorkerJobType::Register);

    if (!shouldTreatAsPotentiallyTrustworthy(job.scriptURL))
        return rejectCurrentJob(ExceptionData { SecurityError, ASCIILiteral("Script URL is not potentially trustworthy") });

    // If the origin of job's script url is not job's referrer's origin, then:
    if (!protocolHostAndPortAreEqual(job.scriptURL, job.clientCreationURL))
        return rejectCurrentJob(ExceptionData { SecurityError, ASCIILiteral("Script origin does not match the registering client's origin") });

    // If the origin of job's scope url is not job's referrer's origin, then:
    if (!protocolHostAndPortAreEqual(job.scopeURL, job.clientCreationURL))
        return rejectCurrentJob(ExceptionData { SecurityError, ASCIILiteral("Scope origin does not match the registering client's origin") });

    // If registration is not null (in our parlance "empty"), then:
    if (auto* registration = m_server.getRegistration(m_registrationKey)) {
        registration->setIsUninstalling(false);
        auto* newestWorker = registration->getNewestWorker();
        if (newestWorker && equalIgnoringFragmentIdentifier(job.scriptURL, newestWorker->scriptURL()) && job.registrationOptions.updateViaCache == registration->updateViaCache()) {
            m_server.resolveRegistrationJob(firstJob(), registration->data());
            finishCurrentJob();
            return;
        }
    } else {
        auto newRegistration = std::make_unique<SWServerRegistration>(m_server, m_registrationKey, job.registrationOptions.updateViaCache, job.scopeURL, job.scriptURL);
        m_server.addRegistration(WTFMove(newRegistration));
    }

    runUpdateJob(job);
}

// https://w3c.github.io/ServiceWorker/#unregister-algorithm
void SWServerJobQueue::runUnregisterJob(const ServiceWorkerJobData& job)
{
    // If the origin of job's scope url is not job's client's origin, then:
    if (!protocolHostAndPortAreEqual(job.scopeURL, job.clientCreationURL))
        return rejectCurrentJob(ExceptionData { SecurityError, ASCIILiteral("Origin of scope URL does not match the client's origin") });

    // Let registration be the result of running "Get Registration" algorithm passing job's scope url as the argument.
    auto* registration = m_server.getRegistration(m_registrationKey);

    // If registration is null, then:
    if (!registration || registration->isUninstalling()) {
        // Invoke Resolve Job Promise with job and false.
        m_server.resolveUnregistrationJob(firstJob(), m_registrationKey, false);
        finishCurrentJob();
        return;
    }

    // Set registration's uninstalling flag.
    registration->setIsUninstalling(true);

    // Invoke Resolve Job Promise with job and true.
    m_server.resolveUnregistrationJob(firstJob(), m_registrationKey, true);

    // Invoke Try Clear Registration with registration.
    tryClearRegistration(*registration);
    finishCurrentJob();
}

// https://w3c.github.io/ServiceWorker/#try-clear-registration-algorithm
void SWServerJobQueue::tryClearRegistration(SWServerRegistration& registration)
{
    // FIXME: Make sure that the registration has no service worker client.

    // FIXME: The specification has more complex logic here. We currently clear registrations
    // too aggressively.
    clearRegistration(registration);
}

// https://w3c.github.io/ServiceWorker/#clear-registration
void SWServerJobQueue::clearRegistration(SWServerRegistration& registration)
{
    // FIXME: Update / terminate the registration's service workers.
    m_server.removeRegistration(registration.key());
}

// https://w3c.github.io/ServiceWorker/#update-algorithm
void SWServerJobQueue::runUpdateJob(const ServiceWorkerJobData& job)
{
    // Let registration be the result of running the Get Registration algorithm passing job's scope url as the argument.
    auto* registration = m_server.getRegistration(m_registrationKey);

    // If registration is null (in our parlance "empty") or registration's uninstalling flag is set, then:
    if (!registration)
        return rejectCurrentJob(ExceptionData { TypeError, ASCIILiteral("Cannot update a null/nonexistent service worker registration") });
    if (registration->isUninstalling())
        return rejectCurrentJob(ExceptionData { TypeError, ASCIILiteral("Cannot update a service worker registration that is uninstalling") });

    // Let newestWorker be the result of running Get Newest Worker algorithm passing registration as the argument.
    auto* newestWorker = registration->getNewestWorker();

    // If job's type is update, and newestWorker's script url does not equal job's script url with the exclude fragments flag set, then:
    if (job.type == ServiceWorkerJobType::Update && newestWorker && !equalIgnoringFragmentIdentifier(job.scriptURL, newestWorker->scriptURL()))
        return rejectCurrentJob(ExceptionData { TypeError, ASCIILiteral("Cannot update a service worker with a requested script URL whose newest worker has a different script URL") });

    m_server.startScriptFetch(job);
}

void SWServerJobQueue::rejectCurrentJob(const ExceptionData& exceptionData)
{
    m_server.rejectJob(firstJob(), exceptionData);

    finishCurrentJob();
}

// https://w3c.github.io/ServiceWorker/#finish-job
void SWServerJobQueue::finishCurrentJob()
{
    ASSERT(!m_jobTimer.isActive());

    m_jobQueue.removeFirst();
    if (!m_jobQueue.isEmpty())
        runNextJob();
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
