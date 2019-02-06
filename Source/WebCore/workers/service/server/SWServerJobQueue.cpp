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
#include "SchemeRegistry.h"
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

bool SWServerJobQueue::isCurrentlyProcessingJob(const ServiceWorkerJobDataIdentifier& jobDataIdentifier) const
{
    return !m_jobQueue.isEmpty() && firstJob().identifier() == jobDataIdentifier;
}

void SWServerJobQueue::scriptFetchFinished(SWServer::Connection& connection, const ServiceWorkerFetchResult& result)
{
    if (!isCurrentlyProcessingJob(result.jobDataIdentifier))
        return;

    auto& job = firstJob();

    auto* registration = m_server.getRegistration(m_registrationKey);
    if (!registration)
        return;

    auto* newestWorker = registration->getNewestWorker();

    if (!result.scriptError.isNull()) {
        // Invoke Reject Job Promise with job and TypeError.
        m_server.rejectJob(job, ExceptionData { TypeError, makeString("Script URL ", job.scriptURL.string(), " fetch resulted in error: ", result.scriptError.localizedDescription()) });

        // If newestWorker is null, invoke Clear Registration algorithm passing registration as its argument.
        if (!newestWorker)
            registration->clear();

        // Invoke Finish Job with job and abort these steps.
        finishCurrentJob();
        return;
    }

    registration->setLastUpdateTime(WallTime::now());

    // If newestWorker is not null, newestWorker's script url equals job's script url with the exclude fragments
    // flag set, and script's source text is a byte-for-byte match with newestWorker's script resource's source
    // text, then:
    if (newestWorker && equalIgnoringFragmentIdentifier(newestWorker->scriptURL(), job.scriptURL) && result.script == newestWorker->script()) {
        // FIXME: for non classic scripts, check the script’s module record's [[ECMAScriptCode]].

        // Invoke Resolve Job Promise with job and registration.
        m_server.resolveRegistrationJob(job, registration->data(), ShouldNotifyWhenResolved::No);

        // Invoke Finish Job with job and abort these steps.
        finishCurrentJob();
        return;
    }

    // FIXME: Update all the imported scripts as per spec. For now, we just do as if there is none.

    // FIXME: Support the proper worker type (classic vs module)
    m_server.updateWorker(connection, job.identifier(), *registration, job.scriptURL, result.script, result.contentSecurityPolicy, result.referrerPolicy, WorkerType::Classic, { });
}

// https://w3c.github.io/ServiceWorker/#update-algorithm
void SWServerJobQueue::scriptContextFailedToStart(const ServiceWorkerJobDataIdentifier& jobDataIdentifier, ServiceWorkerIdentifier, const String& message)
{
    if (!isCurrentlyProcessingJob(jobDataIdentifier))
        return;

    // If an uncaught runtime script error occurs during the above step, then:
    auto* registration = m_server.getRegistration(m_registrationKey);
    ASSERT(registration);

    ASSERT(registration->preInstallationWorker());
    registration->preInstallationWorker()->terminate();
    registration->setPreInstallationWorker(nullptr);

    // Invoke Reject Job Promise with job and TypeError.
    m_server.rejectJob(firstJob(), { TypeError, message });

    // If newestWorker is null, invoke Clear Registration algorithm passing registration as its argument.
    if (!registration->getNewestWorker())
        registration->clear();

    // Invoke Finish Job with job and abort these steps.
    finishCurrentJob();
}

void SWServerJobQueue::scriptContextStarted(const ServiceWorkerJobDataIdentifier& jobDataIdentifier, ServiceWorkerIdentifier identifier)
{
    if (!isCurrentlyProcessingJob(jobDataIdentifier))
        return;

    auto* registration = m_server.getRegistration(m_registrationKey);
    ASSERT(registration);

    install(*registration, identifier);
}

// https://w3c.github.io/ServiceWorker/#install
void SWServerJobQueue::install(SWServerRegistration& registration, ServiceWorkerIdentifier installingWorker)
{
    // The Install algorithm should never be invoked with a null worker.
    auto* worker = m_server.workerByID(installingWorker);
    RELEASE_ASSERT(worker);

    ASSERT(registration.preInstallationWorker() == worker);
    registration.setPreInstallationWorker(nullptr);

    registration.updateRegistrationState(ServiceWorkerRegistrationState::Installing, worker);
    registration.updateWorkerState(*worker, ServiceWorkerState::Installing);

    // Invoke Resolve Job Promise with job and registration.
    m_server.resolveRegistrationJob(firstJob(), registration.data(), ShouldNotifyWhenResolved::Yes);
}

// https://w3c.github.io/ServiceWorker/#install (after resolving promise).
void SWServerJobQueue::didResolveRegistrationPromise()
{
    auto* registration = m_server.getRegistration(m_registrationKey);
    ASSERT(registration);
    ASSERT(registration->installingWorker());

    RELEASE_LOG(ServiceWorker, "%p - SWServerJobQueue::didResolveRegistrationPromise: Registration ID: %llu. Now proceeding with install", this, registration->identifier().toUInt64());

    // Queue a task to fire an event named updatefound at all the ServiceWorkerRegistration objects
    // for all the service worker clients whose creation URL matches registration's scope url and
    // all the service workers whose containing service worker registration is registration.
    registration->fireUpdateFoundEvent();

    // Queue a task to fire the InstallEvent.
    ASSERT(registration->installingWorker());
    m_server.fireInstallEvent(*registration->installingWorker());
}

// https://w3c.github.io/ServiceWorker/#install
void SWServerJobQueue::didFinishInstall(const ServiceWorkerJobDataIdentifier& jobDataIdentifier, ServiceWorkerIdentifier identifier, bool wasSuccessful)
{
    if (!isCurrentlyProcessingJob(jobDataIdentifier))
        return;

    auto* registration = m_server.getRegistration(m_registrationKey);
    ASSERT(registration);
    ASSERT(registration->installingWorker());
    ASSERT(registration->installingWorker()->identifier() == identifier);

    if (!wasSuccessful) {
        RefPtr<SWServerWorker> worker = m_server.workerByID(identifier);
        RELEASE_ASSERT(worker);

        worker->terminate();
        // Run the Update Registration State algorithm passing registration, "installing" and null as the arguments.
        registration->updateRegistrationState(ServiceWorkerRegistrationState::Installing, nullptr);
        // Run the Update Worker State algorithm passing registration's installing worker and redundant as the arguments.
        registration->updateWorkerState(*worker, ServiceWorkerState::Redundant);

        // If newestWorker is null, invoke Clear Registration algorithm passing registration as its argument.
        if (!registration->getNewestWorker())
            registration->clear();

        // Invoke Finish Job with job and abort these steps.
        finishCurrentJob();
        return;
    }

    if (auto* waitingWorker = registration->waitingWorker()) {
        waitingWorker->terminate();
        registration->updateWorkerState(*waitingWorker, ServiceWorkerState::Redundant);
    }

    auto* installing = registration->installingWorker();
    ASSERT(installing);

    registration->updateRegistrationState(ServiceWorkerRegistrationState::Waiting, installing);
    registration->updateRegistrationState(ServiceWorkerRegistrationState::Installing, nullptr);
    registration->updateWorkerState(*installing, ServiceWorkerState::Installed);

    finishCurrentJob();

    // FIXME: Wait for all the tasks queued by Update Worker State invoked in this algorithm have executed.
    registration->tryActivate();
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

    if (!shouldTreatAsPotentiallyTrustworthy(job.scriptURL) && !SchemeRegistry::isServiceWorkerContainerCustomScheme(job.scriptURL.protocol().toStringWithoutCopying()))
        return rejectCurrentJob(ExceptionData { SecurityError, "Script URL is not potentially trustworthy"_s });

    // If the origin of job's script url is not job's referrer's origin, then:
    if (!protocolHostAndPortAreEqual(job.scriptURL, job.clientCreationURL))
        return rejectCurrentJob(ExceptionData { SecurityError, "Script origin does not match the registering client's origin"_s });

    // If the origin of job's scope url is not job's referrer's origin, then:
    if (!protocolHostAndPortAreEqual(job.scopeURL, job.clientCreationURL))
        return rejectCurrentJob(ExceptionData { SecurityError, "Scope origin does not match the registering client's origin"_s });

    // If registration is not null (in our parlance "empty"), then:
    if (auto* registration = m_server.getRegistration(m_registrationKey)) {
        registration->setIsUninstalling(false);
        auto* newestWorker = registration->getNewestWorker();
        if (newestWorker && equalIgnoringFragmentIdentifier(job.scriptURL, newestWorker->scriptURL()) && job.registrationOptions.updateViaCache == registration->updateViaCache()) {
            RELEASE_LOG(ServiceWorker, "%p - SWServerJobQueue::runRegisterJob: Found directly reusable registration %llu for job %s (DONE)", this, registration->identifier().toUInt64(), job.identifier().loggingString().utf8().data());
            m_server.resolveRegistrationJob(job, registration->data(), ShouldNotifyWhenResolved::No);
            finishCurrentJob();
            return;
        }
        // This is not specified yet (https://github.com/w3c/ServiceWorker/issues/1189).
        if (registration->updateViaCache() != job.registrationOptions.updateViaCache)
            registration->setUpdateViaCache(job.registrationOptions.updateViaCache);
        RELEASE_LOG(ServiceWorker, "%p - SWServerJobQueue::runRegisterJob: Found registration %llu for job %s but it needs updating", this, registration->identifier().toUInt64(), job.identifier().loggingString().utf8().data());
    } else {
        auto newRegistration = std::make_unique<SWServerRegistration>(m_server, m_registrationKey, job.registrationOptions.updateViaCache, job.scopeURL, job.scriptURL);
        m_server.addRegistration(WTFMove(newRegistration));

        RELEASE_LOG(ServiceWorker, "%p - SWServerJobQueue::runRegisterJob: No existing registration for job %s, constructing a new one.", this, job.identifier().loggingString().utf8().data());
    }

    runUpdateJob(job);
}

// https://w3c.github.io/ServiceWorker/#unregister-algorithm
void SWServerJobQueue::runUnregisterJob(const ServiceWorkerJobData& job)
{
    // If the origin of job's scope url is not job's client's origin, then:
    if (!protocolHostAndPortAreEqual(job.scopeURL, job.clientCreationURL))
        return rejectCurrentJob(ExceptionData { SecurityError, "Origin of scope URL does not match the client's origin"_s });

    // Let registration be the result of running "Get Registration" algorithm passing job's scope url as the argument.
    auto* registration = m_server.getRegistration(m_registrationKey);

    // If registration is null, then:
    if (!registration || registration->isUninstalling()) {
        // Invoke Resolve Job Promise with job and false.
        m_server.resolveUnregistrationJob(job, m_registrationKey, false);
        finishCurrentJob();
        return;
    }

    // Set registration's uninstalling flag.
    registration->setIsUninstalling(true);

    // Invoke Resolve Job Promise with job and true.
    m_server.resolveUnregistrationJob(job, m_registrationKey, true);

    // Invoke Try Clear Registration with registration.
    registration->tryClear();
    finishCurrentJob();
}

// https://w3c.github.io/ServiceWorker/#update-algorithm
void SWServerJobQueue::runUpdateJob(const ServiceWorkerJobData& job)
{
    // Let registration be the result of running the Get Registration algorithm passing job's scope url as the argument.
    auto* registration = m_server.getRegistration(m_registrationKey);

    // If registration is null (in our parlance "empty") or registration's uninstalling flag is set, then:
    if (!registration)
        return rejectCurrentJob(ExceptionData { TypeError, "Cannot update a null/nonexistent service worker registration"_s });
    if (registration->isUninstalling())
        return rejectCurrentJob(ExceptionData { TypeError, "Cannot update a service worker registration that is uninstalling"_s });

    // Let newestWorker be the result of running Get Newest Worker algorithm passing registration as the argument.
    auto* newestWorker = registration->getNewestWorker();

    // If job's type is update, and newestWorker's script url does not equal job's script url with the exclude fragments flag set, then:
    if (job.type == ServiceWorkerJobType::Update && newestWorker && !equalIgnoringFragmentIdentifier(job.scriptURL, newestWorker->scriptURL()))
        return rejectCurrentJob(ExceptionData { TypeError, "Cannot update a service worker with a requested script URL whose newest worker has a different script URL"_s });

    FetchOptions::Cache cachePolicy = FetchOptions::Cache::Default;
    // Set request's cache mode to "no-cache" if any of the following are true:
    // - registration's update via cache mode is not "all".
    // - job's force bypass cache flag is set.
    // - newestWorker is not null, and registration's last update check time is not null and the time difference in seconds calculated by the
    //   current time minus registration's last update check time is greater than 86400.
    if (registration->updateViaCache() != ServiceWorkerUpdateViaCache::All
        || (newestWorker && registration->lastUpdateTime() && (WallTime::now() - registration->lastUpdateTime()) > 86400_s)) {
        cachePolicy = FetchOptions::Cache::NoCache;
    }
    m_server.startScriptFetch(job, cachePolicy);
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

void SWServerJobQueue::removeAllJobsMatching(const WTF::Function<bool(ServiceWorkerJobData&)>& matches)
{
    bool isFirst = true;
    bool didRemoveFirstJob = false;
    m_jobQueue.removeAllMatching([&](auto& job) {
        bool shouldRemove = matches(job);
        if (isFirst) {
            isFirst = false;
            if (shouldRemove)
                didRemoveFirstJob = true;
        }
        return shouldRemove;
    });

    if (m_jobTimer.isActive()) {
        if (m_jobQueue.isEmpty())
            m_jobTimer.stop();
    } else if (didRemoveFirstJob && !m_jobQueue.isEmpty())
        runNextJob();
}

void SWServerJobQueue::cancelJobsFromConnection(SWServerConnectionIdentifier connectionIdentifier)
{
    removeAllJobsMatching([connectionIdentifier](auto& job) {
        return job.identifier().connectionIdentifier == connectionIdentifier;
    });
}

void SWServerJobQueue::cancelJobsFromServiceWorker(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    removeAllJobsMatching([serviceWorkerIdentifier](auto& job) {
        return WTF::holds_alternative<ServiceWorkerIdentifier>(job.sourceContext) && WTF::get<ServiceWorkerIdentifier>(job.sourceContext) == serviceWorkerIdentifier;
    });
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
