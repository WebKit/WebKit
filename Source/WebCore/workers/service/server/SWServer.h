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

#pragma once

#if ENABLE(SERVICE_WORKER)

#include "ServiceWorkerIdentifier.h"
#include "ServiceWorkerJob.h"
#include "ServiceWorkerRegistrationData.h"
#include "ServiceWorkerRegistrationKey.h"
#include <wtf/CrossThreadQueue.h>
#include <wtf/CrossThreadTask.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Identified.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>

namespace WebCore {

class SWServerJobQueue;
class SWServerRegistration;
class SWServerWorker;
enum class ServiceWorkerRegistrationState;
struct ExceptionData;
struct ServiceWorkerContextData;
struct ServiceWorkerFetchResult;
struct ServiceWorkerRegistrationData;

class SWServer {
public:
    class Connection : public Identified<Connection> {
    friend class SWServer;
    public:
        WEBCORE_EXPORT virtual ~Connection();

        WEBCORE_EXPORT void scriptContextFailedToStart(const ServiceWorkerRegistrationKey&, ServiceWorkerIdentifier, const String& message);
        WEBCORE_EXPORT void scriptContextStarted(const ServiceWorkerRegistrationKey&, ServiceWorkerIdentifier);
        const SWServerRegistration* doRegistrationMatching(const SecurityOriginData& topOrigin, const URL& clientURL) const { return m_server.doRegistrationMatching(topOrigin, clientURL); }

        // Messages to the client WebProcess
        virtual void updateRegistrationStateInClient(const ServiceWorkerRegistrationKey&, ServiceWorkerRegistrationState, std::optional<ServiceWorkerIdentifier>) = 0;
        virtual void fireUpdateFoundEvent(const ServiceWorkerRegistrationKey&) = 0;

    protected:
        WEBCORE_EXPORT Connection(SWServer&, uint64_t identifier);
        SWServer& server() { return m_server; }

        WEBCORE_EXPORT void scheduleJobInServer(const ServiceWorkerJobData&);
        WEBCORE_EXPORT void finishFetchingScriptInServer(const ServiceWorkerFetchResult&);
        WEBCORE_EXPORT void addServiceWorkerRegistrationInServer(const ServiceWorkerRegistrationKey&, uint64_t clientRegistrationIdentifier);
        WEBCORE_EXPORT void removeServiceWorkerRegistrationInServer(const ServiceWorkerRegistrationKey&, uint64_t clientRegistrationIdentifier);

    private:
        // Messages to the client WebProcess
        virtual void rejectJobInClient(uint64_t jobIdentifier, const ExceptionData&) = 0;
        virtual void resolveRegistrationJobInClient(uint64_t jobIdentifier, const ServiceWorkerRegistrationData&) = 0;
        virtual void resolveUnregistrationJobInClient(uint64_t jobIdentifier, const ServiceWorkerRegistrationKey&, bool registrationResult) = 0;
        virtual void startScriptFetchInClient(uint64_t jobIdentifier) = 0;

        // Messages to the SW host WebProcess
        virtual void updateServiceWorkerContext(const ServiceWorkerContextData&) = 0;

        SWServer& m_server;
    };

    WEBCORE_EXPORT SWServer();
    WEBCORE_EXPORT ~SWServer();

    WEBCORE_EXPORT void clear();

    SWServerRegistration* getRegistration(const ServiceWorkerRegistrationKey&);
    void addRegistration(std::unique_ptr<SWServerRegistration>&&);
    void removeRegistration(const ServiceWorkerRegistrationKey&);

    void scheduleJob(const ServiceWorkerJobData&);
    void rejectJob(const ServiceWorkerJobData&, const ExceptionData&);
    void resolveRegistrationJob(const ServiceWorkerJobData&, const ServiceWorkerRegistrationData&);
    void resolveUnregistrationJob(const ServiceWorkerJobData&, const ServiceWorkerRegistrationKey&, bool unregistrationResult);
    void startScriptFetch(const ServiceWorkerJobData&);

    void postTask(CrossThreadTask&&);
    void postTaskReply(CrossThreadTask&&);

    Ref<SWServerWorker> updateWorker(Connection&, const ServiceWorkerRegistrationKey&, const URL&, const String& script, WorkerType);
    
    Connection* getConnection(uint64_t identifier) { return m_connections.get(identifier); }

private:
    void registerConnection(Connection&);
    void unregisterConnection(Connection&);

    void taskThreadEntryPoint();
    void handleTaskRepliesOnMainThread();

    void scriptFetchFinished(Connection&, const ServiceWorkerFetchResult&);
    void scriptContextFailedToStart(Connection&, const ServiceWorkerRegistrationKey&, ServiceWorkerIdentifier, const String& message);
    void scriptContextStarted(Connection&, const ServiceWorkerRegistrationKey&, ServiceWorkerIdentifier);

    void addClientServiceWorkerRegistration(Connection&, const ServiceWorkerRegistrationKey&, uint64_t registrationIdentifier);
    void removeClientServiceWorkerRegistration(Connection&, const ServiceWorkerRegistrationKey&, uint64_t registrationIdentifier);

    WEBCORE_EXPORT const SWServerRegistration* doRegistrationMatching(const SecurityOriginData& topOrigin, const URL& clientURL) const;

    HashMap<uint64_t, Connection*> m_connections;
    HashMap<ServiceWorkerRegistrationKey, std::unique_ptr<SWServerRegistration>> m_registrations;
    HashMap<ServiceWorkerRegistrationKey, std::unique_ptr<SWServerJobQueue>> m_jobQueues;

    HashMap<ServiceWorkerIdentifier, Ref<SWServerWorker>> m_workersByID;

    RefPtr<Thread> m_taskThread;
    Lock m_taskThreadLock;

    CrossThreadQueue<CrossThreadTask> m_taskQueue;
    CrossThreadQueue<CrossThreadTask> m_taskReplyQueue;

    Lock m_mainThreadReplyLock;
    bool m_mainThreadReplyScheduled { false };
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
