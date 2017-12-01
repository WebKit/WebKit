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

#include "ClientOrigin.h"
#include "DocumentIdentifier.h"
#include "SWServerWorker.h"
#include "ServiceWorkerClientData.h"
#include "ServiceWorkerIdentifier.h"
#include "ServiceWorkerJob.h"
#include "ServiceWorkerRegistrationData.h"
#include "ServiceWorkerRegistrationKey.h"
#include "ServiceWorkerTypes.h"
#include <wtf/CrossThreadQueue.h>
#include <wtf/CrossThreadTask.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class SWOriginStore;
class SWServerJobQueue;
class SWServerRegistration;
class SWServerToContextConnection;
enum class ServiceWorkerRegistrationState;
enum class ServiceWorkerState;
struct ExceptionData;
struct ServiceWorkerContextData;
struct ServiceWorkerFetchResult;
struct ServiceWorkerRegistrationData;

class SWServer {
    WTF_MAKE_FAST_ALLOCATED;
public:
    class Connection {
        WTF_MAKE_FAST_ALLOCATED;
        friend class SWServer;
    public:
        WEBCORE_EXPORT virtual ~Connection();

        using Identifier = SWServerConnectionIdentifier;
        Identifier identifier() const { return m_identifier; }

        WEBCORE_EXPORT void didResolveRegistrationPromise(const ServiceWorkerRegistrationKey&);
        const SWServerRegistration* doRegistrationMatching(const SecurityOriginData& topOrigin, const URL& clientURL) const { return m_server.doRegistrationMatching(topOrigin, clientURL); }

        // Messages to the client WebProcess
        virtual void updateRegistrationStateInClient(ServiceWorkerRegistrationIdentifier, ServiceWorkerRegistrationState, const std::optional<ServiceWorkerData>&) = 0;
        virtual void updateWorkerStateInClient(ServiceWorkerIdentifier, ServiceWorkerState) = 0;
        virtual void fireUpdateFoundEvent(ServiceWorkerRegistrationIdentifier) = 0;
        virtual void notifyClientsOfControllerChange(const HashSet<DocumentIdentifier>& contextIdentifiers, const ServiceWorkerData& newController) = 0;

    protected:
        WEBCORE_EXPORT explicit Connection(SWServer&);
        SWServer& server() { return m_server; }

        WEBCORE_EXPORT void scheduleJobInServer(const ServiceWorkerJobData&);
        WEBCORE_EXPORT void finishFetchingScriptInServer(const ServiceWorkerFetchResult&);
        WEBCORE_EXPORT void addServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier);
        WEBCORE_EXPORT void removeServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier);
        WEBCORE_EXPORT void serviceWorkerStartedControllingClient(ServiceWorkerIdentifier, ServiceWorkerRegistrationIdentifier, DocumentIdentifier);
        WEBCORE_EXPORT void serviceWorkerStoppedControllingClient(ServiceWorkerIdentifier, ServiceWorkerRegistrationIdentifier, DocumentIdentifier);
        WEBCORE_EXPORT void syncTerminateWorker(ServiceWorkerIdentifier);

    private:
        // Messages to the client WebProcess
        virtual void rejectJobInClient(const ServiceWorkerJobDataIdentifier&, const ExceptionData&) = 0;
        virtual void resolveRegistrationJobInClient(const ServiceWorkerJobDataIdentifier&, const ServiceWorkerRegistrationData&, ShouldNotifyWhenResolved) = 0;
        virtual void resolveUnregistrationJobInClient(const ServiceWorkerJobDataIdentifier&, const ServiceWorkerRegistrationKey&, bool registrationResult) = 0;
        virtual void startScriptFetchInClient(const ServiceWorkerJobDataIdentifier&) = 0;

        SWServer& m_server;
        Identifier m_identifier;
    };

    WEBCORE_EXPORT explicit SWServer(UniqueRef<SWOriginStore>&&);
    WEBCORE_EXPORT ~SWServer();

    WEBCORE_EXPORT void clearAll();
    WEBCORE_EXPORT void clear(const SecurityOrigin&);

    SWServerRegistration* getRegistration(const ServiceWorkerRegistrationKey&);
    void addRegistration(std::unique_ptr<SWServerRegistration>&&);
    void removeRegistration(const ServiceWorkerRegistrationKey&);
    WEBCORE_EXPORT Vector<ServiceWorkerRegistrationData> getRegistrations(const SecurityOriginData& topOrigin, const URL& clientURL);

    void scheduleJob(const ServiceWorkerJobData&);
    void rejectJob(const ServiceWorkerJobData&, const ExceptionData&);
    void resolveRegistrationJob(const ServiceWorkerJobData&, const ServiceWorkerRegistrationData&, ShouldNotifyWhenResolved);
    void resolveUnregistrationJob(const ServiceWorkerJobData&, const ServiceWorkerRegistrationKey&, bool unregistrationResult);
    void startScriptFetch(const ServiceWorkerJobData&);

    void postTask(CrossThreadTask&&);
    void postTaskReply(CrossThreadTask&&);

    void updateWorker(Connection&, const ServiceWorkerJobDataIdentifier&, SWServerRegistration&, const URL&, const String& script, WorkerType);
    void terminateWorker(SWServerWorker&);
    void syncTerminateWorker(SWServerWorker&);
    void fireInstallEvent(SWServerWorker&);
    void fireActivateEvent(SWServerWorker&);
    WEBCORE_EXPORT SWServerWorker* workerByID(ServiceWorkerIdentifier) const;
    
    Connection* getConnection(SWServerConnectionIdentifier identifier) { return m_connections.get(identifier); }
    SWOriginStore& originStore() { return m_originStore; }

    void scriptContextFailedToStart(const std::optional<ServiceWorkerJobDataIdentifier>&, SWServerWorker&, const String& message);
    void scriptContextStarted(const std::optional<ServiceWorkerJobDataIdentifier>&, SWServerWorker&);
    void didFinishInstall(const std::optional<ServiceWorkerJobDataIdentifier>&, SWServerWorker&, bool wasSuccessful);
    void didFinishActivation(SWServerWorker&);
    void workerContextTerminated(SWServerWorker&);

    WEBCORE_EXPORT void serverToContextConnectionCreated();
    
    WEBCORE_EXPORT static HashSet<SWServer*>& allServers();

    WEBCORE_EXPORT void registerServiceWorkerClient(ClientOrigin&&, ServiceWorkerClientIdentifier, ServiceWorkerClientData&&);
    WEBCORE_EXPORT void unregisterServiceWorkerClient(const ClientOrigin&, ServiceWorkerClientIdentifier);

    WEBCORE_EXPORT bool invokeRunServiceWorker(ServiceWorkerIdentifier);

private:
    void registerConnection(Connection&);
    void unregisterConnection(Connection&);

    void taskThreadEntryPoint();
    void handleTaskRepliesOnMainThread();

    void scriptFetchFinished(Connection&, const ServiceWorkerFetchResult&);

    void didResolveRegistrationPromise(Connection&, const ServiceWorkerRegistrationKey&);

    void addClientServiceWorkerRegistration(Connection&, ServiceWorkerRegistrationIdentifier);
    void removeClientServiceWorkerRegistration(Connection&, ServiceWorkerRegistrationIdentifier);
    void serviceWorkerStartedControllingClient(Connection&, ServiceWorkerIdentifier, ServiceWorkerRegistrationIdentifier, DocumentIdentifier);
    void serviceWorkerStoppedControllingClient(Connection&, ServiceWorkerIdentifier, ServiceWorkerRegistrationIdentifier, DocumentIdentifier);

    WEBCORE_EXPORT const SWServerRegistration* doRegistrationMatching(const SecurityOriginData& topOrigin, const URL& clientURL) const;

    void installContextData(const ServiceWorkerContextData&);

    enum TerminationMode {
        Synchronous,
        Asynchronous,
    };
    void terminateWorkerInternal(SWServerWorker&, TerminationMode);

    HashMap<SWServerConnectionIdentifier, Connection*> m_connections;
    HashMap<ServiceWorkerRegistrationKey, std::unique_ptr<SWServerRegistration>> m_registrations;
    HashMap<ServiceWorkerRegistrationIdentifier, SWServerRegistration*> m_registrationsByID;
    HashMap<ServiceWorkerRegistrationKey, std::unique_ptr<SWServerJobQueue>> m_jobQueues;

    HashMap<ServiceWorkerIdentifier, Ref<SWServerWorker>> m_runningOrTerminatingWorkers;

    struct ClientInformation {
        ServiceWorkerClientIdentifier identifier;
        ServiceWorkerClientData data;
    };
    HashMap<ClientOrigin, Vector<ClientInformation>> m_clients;

    RefPtr<Thread> m_taskThread;
    Lock m_taskThreadLock;

    CrossThreadQueue<CrossThreadTask> m_taskQueue;
    CrossThreadQueue<CrossThreadTask> m_taskReplyQueue;

    Lock m_mainThreadReplyLock;
    bool m_mainThreadReplyScheduled { false };
    UniqueRef<SWOriginStore> m_originStore;
    Deque<ServiceWorkerContextData> m_pendingContextDatas;
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
