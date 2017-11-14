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
#include "SWOriginStore.h"
#include "SWServerJobQueue.h"
#include "SWServerRegistration.h"
#include "SWServerToContextConnection.h"
#include "SWServerWorker.h"
#include "SecurityOrigin.h"
#include "ServiceWorkerContextData.h"
#include "ServiceWorkerFetchResult.h"
#include "ServiceWorkerJobData.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

static ServiceWorkerIdentifier generateServiceWorkerIdentifier()
{
    return generateObjectIdentifier<ServiceWorkerIdentifierType>();
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

HashSet<SWServer*>& SWServer::allServers()
{
    static NeverDestroyed<HashSet<SWServer*>> servers;
    return servers;
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
    
    allServers().remove(this);
}

SWServerRegistration* SWServer::getRegistration(const ServiceWorkerRegistrationKey& registrationKey)
{
    return m_registrations.get(registrationKey);
}

void SWServer::addRegistration(std::unique_ptr<SWServerRegistration>&& registration)
{
    auto key = registration->key();
    auto result = m_registrations.add(key, WTFMove(registration));
    ASSERT_UNUSED(result, result.isNewEntry);

    m_originStore->add(key.topOrigin().securityOrigin());
}

void SWServer::removeRegistration(const ServiceWorkerRegistrationKey& key)
{
    auto topOrigin = key.topOrigin().securityOrigin();
    auto result = m_registrations.remove(key);
    ASSERT_UNUSED(result, result);

    m_originStore->remove(topOrigin);
}

void SWServer::clearAll()
{
    m_jobQueues.clear();
    m_registrations.clear();
    m_originStore->clearAll();
    // FIXME: We should probably ask service workers to terminate.
}

void SWServer::clear(const SecurityOrigin& origin)
{
    m_originStore->clear(origin);

    // FIXME: We should clear entries in m_registrations, m_jobQueues and m_workersByID.
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

SWServer::SWServer(UniqueRef<SWOriginStore>&& originStore)
    : m_originStore(WTFMove(originStore))
{
    allServers().add(this);
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

void SWServer::scriptContextFailedToStart(SWServerWorker& worker, const String& message)
{
    if (auto* jobQueue = m_jobQueues.get(worker.registrationKey()))
        jobQueue->scriptContextFailedToStart(worker.identifier(), message);
}

void SWServer::scriptContextStarted(SWServerWorker& worker)
{
    if (auto* jobQueue = m_jobQueues.get(worker.registrationKey()))
        jobQueue->scriptContextStarted(worker.identifier());
}

void SWServer::didFinishInstall(SWServerWorker& worker, bool wasSuccessful)
{
    if (auto* jobQueue = m_jobQueues.get(worker.registrationKey()))
        jobQueue->didFinishInstall(worker.identifier(), wasSuccessful);
}

void SWServer::didFinishActivation(SWServerWorker& worker)
{
    if (auto* registration = getRegistration(worker.registrationKey()))
        SWServerJobQueue::didFinishActivation(*registration, worker.identifier());
}

void SWServer::didResolveRegistrationPromise(Connection& connection, const ServiceWorkerRegistrationKey& registrationKey)
{
    ASSERT_UNUSED(connection, m_connections.contains(connection.identifier()));

    if (auto* jobQueue = m_jobQueues.get(registrationKey))
        jobQueue->didResolveRegistrationPromise();
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

void SWServer::updateWorker(Connection&, const ServiceWorkerRegistrationKey& registrationKey, const URL& url, const String& script, WorkerType type)
{
    auto serviceWorkerIdentifier = generateServiceWorkerIdentifier();

    ServiceWorkerContextData data = { registrationKey, serviceWorkerIdentifier, script, url, type };

    // Right now we only ever keep up to one connection to one SW context process.
    // And it should always exist if we're calling updateWorker
    auto* connection = SWServerToContextConnection::globalServerToContextConnection();
    if (!connection) {
        m_pendingContextDatas.append(WTFMove(data));
        return;
    }
    
    installContextData(data);
}

void SWServer::serverToContextConnectionCreated()
{
    ASSERT(SWServerToContextConnection::globalServerToContextConnection());
    for (auto& data : m_pendingContextDatas)
        installContextData(data);
    
    m_pendingContextDatas.clear();
}

void SWServer::installContextData(const ServiceWorkerContextData& data)
{
    auto* connection = SWServerToContextConnection::globalServerToContextConnection();
    ASSERT(connection);

    auto result = m_workersByID.add(data.serviceWorkerIdentifier, SWServerWorker::create(*this, data.registrationKey, connection->identifier(), data.scriptURL, data.script, data.workerType, data.serviceWorkerIdentifier));
    ASSERT_UNUSED(result, result.isNewEntry);

    connection->installServiceWorkerContext(data);
}

void SWServer::fireInstallEvent(SWServerWorker& worker)
{
//    auto* worker = m_workersByID.get(serviceWorkerIdentifier);
//    if (!worker) {
//        LOG_ERROR("Request to fire install event on a worker that cannot be found in the server");
//        return;
//    }

    auto* connection = SWServerToContextConnection::connectionForIdentifier(worker.contextConnectionIdentifier());
    if (!connection) {
        LOG_ERROR("Request to fire install event on a worker whose context connection does not exist");
        return;
    }

    connection->fireInstallEvent(worker.identifier());
}

void SWServer::fireActivateEvent(SWServerWorker& worker)
{
//    auto* worker = m_workersByID.get(serviceWorkerIdentifier);
//    if (!worker) {
//        LOG_ERROR("Request to fire install event on a worker that cannot be found in the server");
//        return;
//    }

    auto* connection = SWServerToContextConnection::connectionForIdentifier(worker.contextConnectionIdentifier());
    if (!connection) {
        LOG_ERROR("Request to fire install event on a worker whose context connection does not exist");
        return;
    }

    connection->fireActivateEvent(worker.identifier());
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
