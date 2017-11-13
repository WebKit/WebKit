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
#include "SWServer.h"

#if ENABLE(SERVICE_WORKER)

#include "ExceptionCode.h"
#include "ExceptionData.h"
#include "Logging.h"
#include "SWServerJobQueue.h"
#include "SWServerRegistration.h"
#include "SWServerWorker.h"
#include "SecurityOrigin.h"
#include "ServiceWorkerContextData.h"
#include "ServiceWorkerFetchResult.h"
#include "ServiceWorkerJobData.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

static ServiceWorkerIdentifier generateServiceWorkerIdentifier()
{
    static uint64_t identifier = 0;
    return makeObjectIdentifier<ServiceWorkerIdentifierType>(++identifier);
}

SWServer::Connection::Connection(SWServer& server, uint64_t identifier)
    : Identified(identifier)
    , m_server(server)
{
    m_server.registerConnection(*this);
}

SWServer::Connection::~Connection()
{
    m_server.unregisterConnection(*this);
}

SWServer::~SWServer()
{
    RELEASE_ASSERT(m_connections.isEmpty());
    RELEASE_ASSERT(m_registrations.isEmpty());
    RELEASE_ASSERT(m_jobQueues.isEmpty());

    ASSERT(m_taskQueue.isEmpty());
    ASSERT(m_taskReplyQueue.isEmpty());

    // For a SWServer to be cleanly shut down its thread must have finished and gone away.
    // At this stage in development of the feature that actually never happens.
    // But once it does start happening, this ASSERT will catch us doing it wrong.
    Locker<Lock> locker(m_taskThreadLock);
    ASSERT(!m_taskThread);
}

SWServerRegistration* SWServer::getRegistration(const ServiceWorkerRegistrationKey& registrationKey)
{
    return m_registrations.get(registrationKey);
}

void SWServer::addRegistration(std::unique_ptr<SWServerRegistration>&& registration)
{
    auto key = registration->key();
    m_registrations.add(key, WTFMove(registration));
}

void SWServer::removeRegistration(const ServiceWorkerRegistrationKey& registrationKey)
{
    m_registrations.remove(registrationKey);
}

void SWServer::clear()
{
    m_jobQueues.clear();
    m_registrations.clear();
    // FIXME: We should probably ask service workers to terminate.
}

void SWServer::Connection::scheduleJobInServer(const ServiceWorkerJobData& jobData)
{
    LOG(ServiceWorker, "Scheduling ServiceWorker job %" PRIu64 "-%" PRIu64 " in server", jobData.connectionIdentifier(), jobData.identifier());
    ASSERT(identifier() == jobData.connectionIdentifier());

    m_server.scheduleJob(jobData);
}

void SWServer::Connection::finishFetchingScriptInServer(const ServiceWorkerFetchResult& result)
{
    m_server.scriptFetchFinished(*this, result);
}

void SWServer::Connection::didFinishInstall(const ServiceWorkerRegistrationKey& key, ServiceWorkerIdentifier serviceWorkerIdentifier, bool wasSuccessful)
{
    m_server.didFinishInstall(*this, key, serviceWorkerIdentifier, wasSuccessful);
}

void SWServer::Connection::didFinishActivation(const ServiceWorkerRegistrationKey& key, ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    m_server.didFinishActivation(*this, key, serviceWorkerIdentifier);
}

void SWServer::Connection::setServiceWorkerHasPendingEvents(ServiceWorkerIdentifier serviceWorkerIdentifier, bool hasPendingEvents)
{
    m_server.setServiceWorkerHasPendingEvents(*this, serviceWorkerIdentifier, hasPendingEvents);
}

void SWServer::Connection::didResolveRegistrationPromise(const ServiceWorkerRegistrationKey& key)
{
    m_server.didResolveRegistrationPromise(*this, key);
}

void SWServer::Connection::addServiceWorkerRegistrationInServer(const ServiceWorkerRegistrationKey& key, ServiceWorkerRegistrationIdentifier identifier)
{
    m_server.addClientServiceWorkerRegistration(*this, key, identifier);
}

void SWServer::Connection::removeServiceWorkerRegistrationInServer(const ServiceWorkerRegistrationKey& key, ServiceWorkerRegistrationIdentifier identifier)
{
    m_server.removeClientServiceWorkerRegistration(*this, key, identifier);
}

void SWServer::Connection::scriptContextFailedToStart(const ServiceWorkerRegistrationKey& registrationKey, ServiceWorkerIdentifier identifier, const String& message)
{
    m_server.scriptContextFailedToStart(*this, registrationKey, identifier, message);
}

void SWServer::Connection::scriptContextStarted(const ServiceWorkerRegistrationKey& registrationKey, ServiceWorkerIdentifier identifier)
{
    m_server.scriptContextStarted(*this, registrationKey, identifier);
}

SWServer::SWServer()
{
    m_taskThread = Thread::create(ASCIILiteral("ServiceWorker Task Thread"), [this] {
        taskThreadEntryPoint();
    });
}

// https://w3c.github.io/ServiceWorker/#schedule-job-algorithm
void SWServer::scheduleJob(const ServiceWorkerJobData& jobData)
{
    ASSERT(m_connections.contains(jobData.connectionIdentifier()));

    // FIXME: Per the spec, check if this job is equivalent to the last job on the queue.
    // If it is, stack it along with that job.

    auto& jobQueue = *m_jobQueues.ensure(jobData.registrationKey(), [this, &jobData] {
        return std::make_unique<SWServerJobQueue>(*this, jobData.registrationKey());
    }).iterator->value;

    jobQueue.enqueueJob(jobData);
    if (jobQueue.size() == 1)
        jobQueue.runNextJob();
}

void SWServer::rejectJob(const ServiceWorkerJobData& jobData, const ExceptionData& exceptionData)
{
    LOG(ServiceWorker, "Rejected ServiceWorker job %" PRIu64 "-%" PRIu64 " in server", jobData.connectionIdentifier(), jobData.identifier());
    auto* connection = m_connections.get(jobData.connectionIdentifier());
    if (!connection)
        return;

    connection->rejectJobInClient(jobData.identifier(), exceptionData);
}

void SWServer::resolveRegistrationJob(const ServiceWorkerJobData& jobData, const ServiceWorkerRegistrationData& registrationData, ShouldNotifyWhenResolved shouldNotifyWhenResolved)
{
    LOG(ServiceWorker, "Resolved ServiceWorker job %" PRIu64 "-%" PRIu64 " in server with registration %s", jobData.connectionIdentifier(), jobData.identifier(), registrationData.identifier.loggingString().utf8().data());
    auto* connection = m_connections.get(jobData.connectionIdentifier());
    if (!connection)
        return;

    connection->resolveRegistrationJobInClient(jobData.identifier(), registrationData, shouldNotifyWhenResolved);
}

void SWServer::resolveUnregistrationJob(const ServiceWorkerJobData& jobData, const ServiceWorkerRegistrationKey& registrationKey, bool unregistrationResult)
{
    auto* connection = m_connections.get(jobData.connectionIdentifier());
    if (!connection)
        return;

    connection->resolveUnregistrationJobInClient(jobData.identifier(), registrationKey, unregistrationResult);
}

void SWServer::startScriptFetch(const ServiceWorkerJobData& jobData)
{
    LOG(ServiceWorker, "Server issuing startScriptFetch for current job %" PRIu64 "-%" PRIu64 " in client", jobData.connectionIdentifier(), jobData.identifier());
    auto* connection = m_connections.get(jobData.connectionIdentifier());
    if (!connection)
        return;

    connection->startScriptFetchInClient(jobData.identifier());
}

void SWServer::scriptFetchFinished(Connection& connection, const ServiceWorkerFetchResult& result)
{
    LOG(ServiceWorker, "Server handling scriptFetchFinished for current job %" PRIu64 "-%" PRIu64 " in client", result.connectionIdentifier, result.jobIdentifier);

    ASSERT(m_connections.contains(result.connectionIdentifier));

    auto jobQueue = m_jobQueues.get(result.registrationKey);
    if (!jobQueue)
        return;

    jobQueue->scriptFetchFinished(connection, result);
}

void SWServer::scriptContextFailedToStart(Connection& connection, const ServiceWorkerRegistrationKey& registrationKey, ServiceWorkerIdentifier identifier, const String& message)
{
    ASSERT(m_connections.contains(connection.identifier()));
    
    if (auto* jobQueue = m_jobQueues.get(registrationKey))
        jobQueue->scriptContextFailedToStart(connection, identifier, message);
}

void SWServer::scriptContextStarted(Connection& connection, const ServiceWorkerRegistrationKey& registrationKey, ServiceWorkerIdentifier identifier)
{
    ASSERT(m_connections.contains(connection.identifier()));

    if (auto* jobQueue = m_jobQueues.get(registrationKey))
        jobQueue->scriptContextStarted(connection, identifier);
}

void SWServer::didFinishInstall(Connection& connection, const ServiceWorkerRegistrationKey& registrationKey, ServiceWorkerIdentifier serviceWorkerIdentifier, bool wasSuccessful)
{
    ASSERT(m_connections.contains(connection.identifier()));

    if (auto* jobQueue = m_jobQueues.get(registrationKey))
        jobQueue->didFinishInstall(connection, serviceWorkerIdentifier, wasSuccessful);
}

void SWServer::didFinishActivation(Connection& connection, const ServiceWorkerRegistrationKey& registrationKey, ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    ASSERT_UNUSED(connection, m_connections.contains(connection.identifier()));

    if (auto* registration = getRegistration(registrationKey))
        SWServerJobQueue::didFinishActivation(*registration, serviceWorkerIdentifier);
}

void SWServer::setServiceWorkerHasPendingEvents(Connection& connection, ServiceWorkerIdentifier serviceWorkerIdentifier, bool hasPendingEvents)
{
    ASSERT_UNUSED(connection, m_connections.contains(connection.identifier()));

    if (auto* serviceWorker = m_workersByID.get(serviceWorkerIdentifier))
        serviceWorker->setHasPendingEvents(hasPendingEvents);
}

void SWServer::didResolveRegistrationPromise(Connection& connection, const ServiceWorkerRegistrationKey& registrationKey)
{
    ASSERT(m_connections.contains(connection.identifier()));

    if (auto* jobQueue = m_jobQueues.get(registrationKey))
        jobQueue->didResolveRegistrationPromise(connection);
}

void SWServer::addClientServiceWorkerRegistration(Connection& connection, const ServiceWorkerRegistrationKey& key, ServiceWorkerRegistrationIdentifier identifier)
{
    auto* registration = m_registrations.get(key);
    if (!registration) {
        LOG_ERROR("Request to add client-side ServiceWorkerRegistration to non-existent server-side registration");
        return;
    }

    if (registration->identifier() != identifier)
        return;
    
    registration->addClientServiceWorkerRegistration(connection.identifier());
}

void SWServer::removeClientServiceWorkerRegistration(Connection& connection, const ServiceWorkerRegistrationKey& key, ServiceWorkerRegistrationIdentifier identifier)
{
    auto* registration = m_registrations.get(key);
    if (!registration) {
        LOG_ERROR("Request to remove client-side ServiceWorkerRegistration from non-existent server-side registration");
        return;
    }

    if (registration->identifier() != identifier)
        return;
    
    registration->removeClientServiceWorkerRegistration(connection.identifier());
}

Ref<SWServerWorker> SWServer::updateWorker(Connection& connection, const ServiceWorkerRegistrationKey& registrationKey, const URL& url, const String& script, WorkerType type)
{
    auto serviceWorkerIdentifier = generateServiceWorkerIdentifier();

    auto result = m_workersByID.add(serviceWorkerIdentifier, SWServerWorker::create(registrationKey, url, script, type, serviceWorkerIdentifier));
    ASSERT(result.isNewEntry);

    connection.installServiceWorkerContext({ registrationKey, serviceWorkerIdentifier, script, url });
    
    return result.iterator->value.get();
}

void SWServer::fireInstallEvent(Connection& connection, ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    connection.fireInstallEvent(serviceWorkerIdentifier);
}

void SWServer::fireActivateEvent(Connection& connection, ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    connection.fireActivateEvent(serviceWorkerIdentifier);
}

void SWServer::taskThreadEntryPoint()
{
    ASSERT(!isMainThread());

    while (!m_taskQueue.isKilled())
        m_taskQueue.waitForMessage().performTask();

    Locker<Lock> locker(m_taskThreadLock);
    m_taskThread = nullptr;
}

void SWServer::postTask(CrossThreadTask&& task)
{
    m_taskQueue.append(WTFMove(task));
}

void SWServer::postTaskReply(CrossThreadTask&& task)
{
    m_taskReplyQueue.append(WTFMove(task));

    Locker<Lock> locker(m_mainThreadReplyLock);
    if (m_mainThreadReplyScheduled)
        return;

    m_mainThreadReplyScheduled = true;
    callOnMainThread([this] {
        handleTaskRepliesOnMainThread();
    });
}

void SWServer::handleTaskRepliesOnMainThread()
{
    {
        Locker<Lock> locker(m_mainThreadReplyLock);
        m_mainThreadReplyScheduled = false;
    }

    while (auto task = m_taskReplyQueue.tryGetMessage())
        task->performTask();
}

void SWServer::registerConnection(Connection& connection)
{
    auto result = m_connections.add(connection.identifier(), nullptr);
    ASSERT(result.isNewEntry);
    result.iterator->value = &connection;
}

void SWServer::unregisterConnection(Connection& connection)
{
    ASSERT(m_connections.get(connection.identifier()) == &connection);
    m_connections.remove(connection.identifier());
}

const SWServerRegistration* SWServer::doRegistrationMatching(const SecurityOriginData& topOrigin, const URL& clientURL) const
{
    const SWServerRegistration* selectedRegistration = nullptr;
    for (auto& registration : m_registrations.values()) {
        if (!registration->key().isMatching(topOrigin, clientURL))
            continue;
        if (!selectedRegistration || selectedRegistration->key().scopeLength() < registration->key().scopeLength())
            selectedRegistration = registration.get();
    }

    return (selectedRegistration && !selectedRegistration->isUninstalling()) ? selectedRegistration : nullptr;
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
