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
#include "SWServerRegistration.h"
#include "SWServerWorker.h"
#include "ServiceWorkerContextData.h"
#include "ServiceWorkerFetchResult.h"
#include "ServiceWorkerJobData.h"
#include <wtf/UUID.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

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

    ASSERT(m_taskQueue.isEmpty());
    ASSERT(m_taskReplyQueue.isEmpty());

    // For a SWServer to be cleanly shut down its thread must have finished and gone away.
    // At this stage in development of the feature that actually never happens.
    // But once it does start happening, this ASSERT will catch us doing it wrong.
    Locker<Lock> locker(m_taskThreadLock);
    ASSERT(!m_taskThread);
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

void SWServer::Connection::scriptContextFailedToStart(const ServiceWorkerRegistrationKey& registrationKey, const String& workerID, const String& message)
{
    m_server.scriptContextFailedToStart(*this, registrationKey, workerID, message);
}

void SWServer::Connection::scriptContextStarted(const ServiceWorkerRegistrationKey& registrationKey, uint64_t identifier, const String& workerID)
{
    m_server.scriptContextStarted(*this, registrationKey, identifier, workerID);
}

SWServer::SWServer()
{
    m_taskThread = Thread::create(ASCIILiteral("ServiceWorker Task Thread"), [this] {
        taskThreadEntryPoint();
    });
}

void SWServer::scheduleJob(const ServiceWorkerJobData& jobData)
{
    ASSERT(m_connections.contains(jobData.connectionIdentifier()));

    auto result = m_registrations.add(jobData.registrationKey(), nullptr);
    if (result.isNewEntry)
        result.iterator->value = std::make_unique<SWServerRegistration>(*this, jobData.registrationKey());

    ASSERT(result.iterator->value);

    result.iterator->value->enqueueJob(jobData);
}

void SWServer::rejectJob(const ServiceWorkerJobData& jobData, const ExceptionData& exceptionData)
{
    LOG(ServiceWorker, "Rejected ServiceWorker job %" PRIu64 "-%" PRIu64 " in server", jobData.connectionIdentifier(), jobData.identifier());
    auto* connection = m_connections.get(jobData.connectionIdentifier());
    if (!connection)
        return;

    connection->rejectJobInClient(jobData.identifier(), exceptionData);
}

void SWServer::resolveRegistrationJob(const ServiceWorkerJobData& jobData, const ServiceWorkerRegistrationData& registrationData)
{
    LOG(ServiceWorker, "Resolved ServiceWorker job %" PRIu64 "-%" PRIu64 " in server with registration %" PRIu64, jobData.connectionIdentifier(), jobData.identifier(), registrationData.identifier);
    auto* connection = m_connections.get(jobData.connectionIdentifier());
    if (!connection)
        return;

    connection->resolveRegistrationJobInClient(jobData.identifier(), registrationData);
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

    auto registration = m_registrations.get(result.registrationKey);
    if (!registration)
        return;

    registration->scriptFetchFinished(connection, result);
}

void SWServer::scriptContextFailedToStart(Connection& connection, const ServiceWorkerRegistrationKey& registrationKey, const String& workerID, const String& message)
{
    ASSERT(m_connections.contains(connection.identifier()));
    
    if (auto* registration = m_registrations.get(registrationKey))
        registration->scriptContextFailedToStart(connection, workerID, message);
}

void SWServer::scriptContextStarted(Connection& connection, const ServiceWorkerRegistrationKey& registrationKey, uint64_t identifier, const String& workerID)
{
    ASSERT(m_connections.contains(connection.identifier()));

    if (auto* registration = m_registrations.get(registrationKey))
        registration->scriptContextStarted(connection, identifier, workerID);
}

Ref<SWServerWorker> SWServer::updateWorker(Connection& connection, const ServiceWorkerRegistrationKey& registrationKey, const URL& url, const String& script, WorkerType type)
{
    String workerID = createCanonicalUUIDString();
    
    auto result = m_workersByID.add(workerID, SWServerWorker::create(registrationKey, url, script, type, workerID));
    ASSERT(result.isNewEntry);
    
    connection.updateServiceWorkerContext({ registrationKey, workerID, script, url });
    
    return result.iterator->value.get();
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

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
