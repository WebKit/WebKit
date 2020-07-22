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
#include "ExceptionOr.h"
#include "SWServerWorker.h"
#include "SecurityOriginData.h"
#include "ServiceWorkerClientData.h"
#include "ServiceWorkerIdentifier.h"
#include "ServiceWorkerJob.h"
#include "ServiceWorkerRegistrationData.h"
#include "ServiceWorkerRegistrationKey.h"
#include "ServiceWorkerTypes.h"
#include <pal/SessionID.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class RegistrationStore;
class SWOriginStore;
class SWServerJobQueue;
class SWServerRegistration;
class SWServerToContextConnection;
enum class ServiceWorkerRegistrationState : uint8_t;
enum class ServiceWorkerState : uint8_t;
struct ExceptionData;
struct MessageWithMessagePorts;
struct ServiceWorkerClientQueryOptions;
struct ServiceWorkerContextData;
struct ServiceWorkerFetchResult;
struct ServiceWorkerRegistrationData;
class Timer;

class SWServer : public CanMakeWeakPtr<SWServer> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    class Connection : public CanMakeWeakPtr<Connection> {
        WTF_MAKE_FAST_ALLOCATED;
        friend class SWServer;
    public:
        virtual ~Connection() = default;

        using Identifier = SWServerConnectionIdentifier;
        Identifier identifier() const { return m_identifier; }

        WEBCORE_EXPORT void didResolveRegistrationPromise(const ServiceWorkerRegistrationKey&);
        SWServerRegistration* doRegistrationMatching(const SecurityOriginData& topOrigin, const URL& clientURL) { return m_server.doRegistrationMatching(topOrigin, clientURL); }
        void resolveRegistrationReadyRequests(SWServerRegistration&);

        // Messages to the client WebProcess
        virtual void updateRegistrationStateInClient(ServiceWorkerRegistrationIdentifier, ServiceWorkerRegistrationState, const Optional<ServiceWorkerData>&) = 0;
        virtual void updateWorkerStateInClient(ServiceWorkerIdentifier, ServiceWorkerState) = 0;
        virtual void fireUpdateFoundEvent(ServiceWorkerRegistrationIdentifier) = 0;
        virtual void setRegistrationLastUpdateTime(ServiceWorkerRegistrationIdentifier, WallTime) = 0;
        virtual void setRegistrationUpdateViaCache(ServiceWorkerRegistrationIdentifier, ServiceWorkerUpdateViaCache) = 0;
        virtual void notifyClientsOfControllerChange(const HashSet<DocumentIdentifier>& contextIdentifiers, const ServiceWorkerData& newController) = 0;
        virtual void registrationReady(uint64_t registrationReadyRequestIdentifier, ServiceWorkerRegistrationData&&) = 0;
        virtual void postMessageToServiceWorkerClient(DocumentIdentifier, const MessageWithMessagePorts&, ServiceWorkerIdentifier, const String& sourceOrigin) = 0;

        virtual void contextConnectionCreated(SWServerToContextConnection&) = 0;

        SWServer& server() { return m_server; }
        const SWServer& server() const { return m_server; }

    protected:
        WEBCORE_EXPORT Connection(SWServer&, Identifier);

        WEBCORE_EXPORT void finishFetchingScriptInServer(const ServiceWorkerFetchResult&);
        WEBCORE_EXPORT void addServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier);
        WEBCORE_EXPORT void removeServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier);
        WEBCORE_EXPORT void whenRegistrationReady(uint64_t registrationReadyRequestIdentifier, const SecurityOriginData& topOrigin, const URL& clientURL);

        WEBCORE_EXPORT void storeRegistrationsOnDisk(CompletionHandler<void()>&&);

    private:
        // Messages to the client WebProcess
        virtual void rejectJobInClient(ServiceWorkerJobIdentifier, const ExceptionData&) = 0;
        virtual void resolveRegistrationJobInClient(ServiceWorkerJobIdentifier, const ServiceWorkerRegistrationData&, ShouldNotifyWhenResolved) = 0;
        virtual void resolveUnregistrationJobInClient(ServiceWorkerJobIdentifier, const ServiceWorkerRegistrationKey&, bool registrationResult) = 0;
        virtual void startScriptFetchInClient(ServiceWorkerJobIdentifier, const ServiceWorkerRegistrationKey&, FetchOptions::Cache) = 0;

        struct RegistrationReadyRequest {
            SecurityOriginData topOrigin;
            URL clientURL;
            uint64_t identifier;
        };

        SWServer& m_server;
        Identifier m_identifier;
        Vector<RegistrationReadyRequest> m_registrationReadyRequests;
    };

    using SoftUpdateCallback = Function<void(ServiceWorkerJobData&& jobData, bool shouldRefreshCache, ResourceRequest&&, CompletionHandler<void(const ServiceWorkerFetchResult&)>&&)>;
    using CreateContextConnectionCallback = Function<void(const WebCore::RegistrableDomain&, CompletionHandler<void()>&&)>;
    using AppBoundDomainsCallback = Function<void(CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&&)>;
    WEBCORE_EXPORT SWServer(UniqueRef<SWOriginStore>&&, bool processTerminationDelayEnabled, String&& registrationDatabaseDirectory, PAL::SessionID, bool hasServiceWorkerEntitlement, SoftUpdateCallback&&, CreateContextConnectionCallback&&, AppBoundDomainsCallback&&);

    WEBCORE_EXPORT ~SWServer();

    WEBCORE_EXPORT void clearAll(WTF::CompletionHandler<void()>&&);
    WEBCORE_EXPORT void clear(const SecurityOriginData&, WTF::CompletionHandler<void()>&&);

    WEBCORE_EXPORT void startSuspension(CompletionHandler<void()>&&);
    WEBCORE_EXPORT void endSuspension();

    SWServerRegistration* getRegistration(ServiceWorkerRegistrationIdentifier identifier) { return m_registrations.get(identifier); }
    WEBCORE_EXPORT SWServerRegistration* getRegistration(const ServiceWorkerRegistrationKey&);
    void addRegistration(std::unique_ptr<SWServerRegistration>&&);
    void removeRegistration(ServiceWorkerRegistrationIdentifier);
    WEBCORE_EXPORT Vector<ServiceWorkerRegistrationData> getRegistrations(const SecurityOriginData& topOrigin, const URL& clientURL);

    WEBCORE_EXPORT void scheduleJob(ServiceWorkerJobData&&);
    WEBCORE_EXPORT void scheduleUnregisterJob(ServiceWorkerJobDataIdentifier, SWServerRegistration&, DocumentOrWorkerIdentifier, URL&&);

    void rejectJob(const ServiceWorkerJobData&, const ExceptionData&);
    void resolveRegistrationJob(const ServiceWorkerJobData&, const ServiceWorkerRegistrationData&, ShouldNotifyWhenResolved);
    void resolveUnregistrationJob(const ServiceWorkerJobData&, const ServiceWorkerRegistrationKey&, bool unregistrationResult);
    void startScriptFetch(const ServiceWorkerJobData&, bool shouldRefreshCache);

    void updateWorker(const ServiceWorkerJobDataIdentifier&, SWServerRegistration&, const URL&, const String& script, const CertificateInfo&, const ContentSecurityPolicyResponseHeaders&, const String& referrerPolicy, WorkerType, HashMap<URL, ServiceWorkerContextData::ImportedScript>&&);
    void fireInstallEvent(SWServerWorker&);
    void fireActivateEvent(SWServerWorker&);

    WEBCORE_EXPORT SWServerWorker* workerByID(ServiceWorkerIdentifier) const;
    WEBCORE_EXPORT Optional<ServiceWorkerClientData> serviceWorkerClientWithOriginByID(const ClientOrigin&, const ServiceWorkerClientIdentifier&) const;
    String serviceWorkerClientUserAgent(const ClientOrigin&) const;
    WEBCORE_EXPORT SWServerWorker* activeWorkerFromRegistrationID(ServiceWorkerRegistrationIdentifier);

    WEBCORE_EXPORT void markAllWorkersForRegistrableDomainAsTerminated(const RegistrableDomain&);
    
    WEBCORE_EXPORT void addConnection(std::unique_ptr<Connection>&&);
    WEBCORE_EXPORT void removeConnection(SWServerConnectionIdentifier);
    Connection* connection(SWServerConnectionIdentifier identifier) const { return m_connections.get(identifier); }

    const HashMap<SWServerConnectionIdentifier, std::unique_ptr<Connection>>& connections() const { return m_connections; }
    WEBCORE_EXPORT bool canHandleScheme(StringView) const;

    SWOriginStore& originStore() { return m_originStore; }

    void scriptContextFailedToStart(const Optional<ServiceWorkerJobDataIdentifier>&, SWServerWorker&, const String& message);
    void scriptContextStarted(const Optional<ServiceWorkerJobDataIdentifier>&, SWServerWorker&);
    void didFinishInstall(const Optional<ServiceWorkerJobDataIdentifier>&, SWServerWorker&, bool wasSuccessful);
    void didFinishActivation(SWServerWorker&);
    void workerContextTerminated(SWServerWorker&);
    void matchAll(SWServerWorker&, const ServiceWorkerClientQueryOptions&, ServiceWorkerClientsMatchAllCallback&&);
    Optional<ExceptionData> claim(SWServerWorker&);

    WEBCORE_EXPORT static HashSet<SWServer*>& allServers();

    WEBCORE_EXPORT void registerServiceWorkerClient(ClientOrigin&&, ServiceWorkerClientData&&, const Optional<ServiceWorkerRegistrationIdentifier>&, String&& userAgent);
    WEBCORE_EXPORT void unregisterServiceWorkerClient(const ClientOrigin&, ServiceWorkerClientIdentifier);

    using RunServiceWorkerCallback = WTF::Function<void(SWServerToContextConnection*)>;
    WEBCORE_EXPORT void runServiceWorkerIfNecessary(ServiceWorkerIdentifier, RunServiceWorkerCallback&&);

    void resolveRegistrationReadyRequests(SWServerRegistration&);

    void addRegistrationFromStore(ServiceWorkerContextData&&);
    void registrationStoreImportComplete();
    void registrationStoreDatabaseFailedToOpen();

    WEBCORE_EXPORT void getOriginsWithRegistrations(Function<void(const HashSet<SecurityOriginData>&)>&&);

    PAL::SessionID sessionID() const { return m_sessionID; }
    WEBCORE_EXPORT bool needsContextConnectionForRegistrableDomain(const RegistrableDomain&) const;

    void removeFromScopeToRegistrationMap(const ServiceWorkerRegistrationKey&);

    WEBCORE_EXPORT void addContextConnection(SWServerToContextConnection&);
    WEBCORE_EXPORT void removeContextConnection(SWServerToContextConnection&);

    SWServerToContextConnection* contextConnectionForRegistrableDomain(const RegistrableDomain& domain) { return m_contextConnections.get(domain); }
    WEBCORE_EXPORT void createContextConnection(const RegistrableDomain&);

    bool isImportCompleted() const { return m_importCompleted; }
    WEBCORE_EXPORT void whenImportIsCompleted(CompletionHandler<void()>&&);

    void softUpdate(SWServerRegistration&);

    WEBCORE_EXPORT void handleLowMemoryWarning();

    static constexpr Seconds defaultTerminationDelay = 10_s;

private:
    void validateRegistrationDomain(WebCore::RegistrableDomain, CompletionHandler<void(bool)>&&);

    void scriptFetchFinished(const ServiceWorkerFetchResult&);

    void didResolveRegistrationPromise(Connection*, const ServiceWorkerRegistrationKey&);

    void addClientServiceWorkerRegistration(Connection&, ServiceWorkerRegistrationIdentifier);
    void removeClientServiceWorkerRegistration(Connection&, ServiceWorkerRegistrationIdentifier);

    void terminatePreinstallationWorker(SWServerWorker&);

    WEBCORE_EXPORT SWServerRegistration* doRegistrationMatching(const SecurityOriginData& topOrigin, const URL& clientURL);
    bool runServiceWorker(ServiceWorkerIdentifier);

    void tryInstallContextData(ServiceWorkerContextData&&);
    void installContextData(const ServiceWorkerContextData&);

    SWServerRegistration* registrationFromServiceWorkerIdentifier(ServiceWorkerIdentifier);
    void forEachClientForOrigin(const ClientOrigin&, const WTF::Function<void(ServiceWorkerClientData&)>&);

    void performGetOriginsWithRegistrationsCallbacks();

    void contextConnectionCreated(SWServerToContextConnection&);

    HashMap<SWServerConnectionIdentifier, std::unique_ptr<Connection>> m_connections;
    HashMap<ServiceWorkerRegistrationKey, WeakPtr<SWServerRegistration>> m_scopeToRegistrationMap;
    HashMap<ServiceWorkerRegistrationIdentifier, std::unique_ptr<SWServerRegistration>> m_registrations;
    HashMap<ServiceWorkerRegistrationKey, std::unique_ptr<SWServerJobQueue>> m_jobQueues;

    HashMap<ServiceWorkerIdentifier, Ref<SWServerWorker>> m_runningOrTerminatingWorkers;

    HashMap<RegistrableDomain, HashSet<ServiceWorkerClientIdentifier>> m_clientsByRegistrableDomain;
    struct Clients {
        Vector<ServiceWorkerClientIdentifier> identifiers;
        std::unique_ptr<Timer> terminateServiceWorkersTimer;
        String userAgent;
    };
    HashMap<ClientOrigin, Clients> m_clientIdentifiersPerOrigin;
    HashMap<ServiceWorkerClientIdentifier, ServiceWorkerClientData> m_clientsById;
    HashMap<ServiceWorkerClientIdentifier, ServiceWorkerRegistrationIdentifier> m_clientToControllingRegistration;

    UniqueRef<SWOriginStore> m_originStore;
    std::unique_ptr<RegistrationStore> m_registrationStore;
    HashMap<RegistrableDomain, Vector<ServiceWorkerContextData>> m_pendingContextDatas;
    HashMap<RegistrableDomain, HashMap<ServiceWorkerIdentifier, Vector<RunServiceWorkerCallback>>> m_serviceWorkerRunRequests;
    PAL::SessionID m_sessionID;
    bool m_importCompleted { false };
    bool m_isProcessTerminationDelayEnabled { true };
    Vector<CompletionHandler<void()>> m_clearCompletionCallbacks;
    Vector<Function<void(const HashSet<SecurityOriginData>&)>> m_getOriginsWithRegistrationsCallbacks;
    HashMap<RegistrableDomain, SWServerToContextConnection*> m_contextConnections;

    CreateContextConnectionCallback m_createContextConnectionCallback;
    HashSet<WebCore::RegistrableDomain> m_pendingConnectionDomains;
    Vector<CompletionHandler<void()>> m_importCompletedCallbacks;
    SoftUpdateCallback m_softUpdateCallback;
    AppBoundDomainsCallback m_appBoundDomainsCallback;
    
    HashSet<WebCore::RegistrableDomain> m_appBoundDomains;
    bool m_hasServiceWorkerEntitlement { false };
    bool m_hasReceivedAppBoundDomains { false };
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
