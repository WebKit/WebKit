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
#include "ExceptionOr.h"
#include "PageIdentifier.h"
#include "SWServerWorker.h"
#include "ScriptExecutionContextIdentifier.h"
#include "SecurityOriginData.h"
#include "ServiceWorkerClientData.h"
#include "ServiceWorkerIdentifier.h"
#include "ServiceWorkerJob.h"
#include "ServiceWorkerRegistrationData.h"
#include "ServiceWorkerRegistrationKey.h"
#include "ServiceWorkerTypes.h"
#include "WorkerThreadMode.h"
#include <pal/SessionID.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>
#include <wtf/URLHash.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class RegistrationStore;
class SWOriginStore;
class SWServerJobQueue;
class SWServerRegistration;
class SWServerToContextConnection;
class Timer;
enum class NotificationEventType : bool;
enum class ServiceWorkerRegistrationState : uint8_t;
enum class ServiceWorkerState : uint8_t;
struct ExceptionData;
struct MessageWithMessagePorts;
struct NotificationData;
struct ServiceWorkerClientQueryOptions;
struct ServiceWorkerContextData;
struct ServiceWorkerRegistrationData;
struct WorkerFetchResult;

class SWServer : public CanMakeWeakPtr<SWServer> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    class Connection : public CanMakeWeakPtr<Connection> {
        WTF_MAKE_FAST_ALLOCATED;
        friend class SWServer;
    public:
        WEBCORE_EXPORT virtual ~Connection();

        using Identifier = SWServerConnectionIdentifier;
        Identifier identifier() const { return m_identifier; }

        WEBCORE_EXPORT void didResolveRegistrationPromise(const ServiceWorkerRegistrationKey&);
        SWServerRegistration* doRegistrationMatching(const SecurityOriginData& topOrigin, const URL& clientURL) { return m_server.doRegistrationMatching(topOrigin, clientURL); }
        void resolveRegistrationReadyRequests(SWServerRegistration&);

        // Messages to the client WebProcess
        virtual void updateRegistrationStateInClient(ServiceWorkerRegistrationIdentifier, ServiceWorkerRegistrationState, const std::optional<ServiceWorkerData>&) = 0;
        virtual void updateWorkerStateInClient(ServiceWorkerIdentifier, ServiceWorkerState) = 0;
        virtual void fireUpdateFoundEvent(ServiceWorkerRegistrationIdentifier) = 0;
        virtual void setRegistrationLastUpdateTime(ServiceWorkerRegistrationIdentifier, WallTime) = 0;
        virtual void setRegistrationUpdateViaCache(ServiceWorkerRegistrationIdentifier, ServiceWorkerUpdateViaCache) = 0;
        virtual void notifyClientsOfControllerChange(const HashSet<ScriptExecutionContextIdentifier>& contextIdentifiers, const ServiceWorkerData& newController) = 0;
        virtual void postMessageToServiceWorkerClient(ScriptExecutionContextIdentifier, const MessageWithMessagePorts&, ServiceWorkerIdentifier, const String& sourceOrigin) = 0;
        virtual void focusServiceWorkerClient(ScriptExecutionContextIdentifier, CompletionHandler<void(std::optional<ServiceWorkerClientData>&&)>&&) = 0;

        virtual void contextConnectionCreated(SWServerToContextConnection&) = 0;

        SWServer& server() { return m_server; }
        const SWServer& server() const { return m_server; }

    protected:
        WEBCORE_EXPORT Connection(SWServer&, Identifier);

        WEBCORE_EXPORT void finishFetchingScriptInServer(const ServiceWorkerJobDataIdentifier&, const ServiceWorkerRegistrationKey&, WorkerFetchResult&&);
        WEBCORE_EXPORT void addServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier);
        WEBCORE_EXPORT void removeServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier);
        WEBCORE_EXPORT void whenRegistrationReady(const SecurityOriginData& topOrigin, const URL& clientURL, CompletionHandler<void(std::optional<ServiceWorkerRegistrationData>&&)>&&);

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
            CompletionHandler<void(std::optional<ServiceWorkerRegistrationData>&&)> callback;
        };

        SWServer& m_server;
        Identifier m_identifier;
        Vector<RegistrationReadyRequest> m_registrationReadyRequests;
    };

    using SoftUpdateCallback = Function<void(ServiceWorkerJobData&& jobData, bool shouldRefreshCache, ResourceRequest&&, CompletionHandler<void(WorkerFetchResult&&)>&&)>;
    using CreateContextConnectionCallback = Function<void(const RegistrableDomain&, std::optional<ProcessIdentifier> requestingProcessIdentifier, std::optional<ScriptExecutionContextIdentifier>, CompletionHandler<void()>&&)>;
    using AppBoundDomainsCallback = Function<void(CompletionHandler<void(HashSet<RegistrableDomain>&&)>&&)>;
    using AddAllowedFirstPartyForCookiesCallback = Function<void(ProcessIdentifier, std::optional<ProcessIdentifier> requestingProcessIdentifier, WebCore::RegistrableDomain&&)>;
    WEBCORE_EXPORT SWServer(UniqueRef<SWOriginStore>&&, bool processTerminationDelayEnabled, String&& registrationDatabaseDirectory, PAL::SessionID, bool shouldRunServiceWorkersOnMainThreadForTesting, bool hasServiceWorkerEntitlement, std::optional<unsigned> overrideServiceWorkerRegistrationCountTestingValue, SoftUpdateCallback&&, CreateContextConnectionCallback&&, AppBoundDomainsCallback&&, AddAllowedFirstPartyForCookiesCallback&&);

    WEBCORE_EXPORT ~SWServer();

    WEBCORE_EXPORT void clearAll(CompletionHandler<void()>&&);
    WEBCORE_EXPORT void clear(const SecurityOriginData&, CompletionHandler<void()>&&);

    WEBCORE_EXPORT void startSuspension(CompletionHandler<void()>&&);
    WEBCORE_EXPORT void endSuspension();

    SWServerRegistration* getRegistration(ServiceWorkerRegistrationIdentifier identifier) { return m_registrations.get(identifier); }
    WEBCORE_EXPORT SWServerRegistration* getRegistration(const ServiceWorkerRegistrationKey&);
    void addRegistration(std::unique_ptr<SWServerRegistration>&&);
    void removeRegistration(ServiceWorkerRegistrationIdentifier);
    WEBCORE_EXPORT Vector<ServiceWorkerRegistrationData> getRegistrations(const SecurityOriginData& topOrigin, const URL& clientURL);

    WEBCORE_EXPORT void scheduleJob(ServiceWorkerJobData&&);
    WEBCORE_EXPORT void scheduleUnregisterJob(ServiceWorkerJobDataIdentifier, SWServerRegistration&, ServiceWorkerOrClientIdentifier, URL&&);

    void rejectJob(const ServiceWorkerJobData&, const ExceptionData&);
    void resolveRegistrationJob(const ServiceWorkerJobData&, const ServiceWorkerRegistrationData&, ShouldNotifyWhenResolved);
    void resolveUnregistrationJob(const ServiceWorkerJobData&, const ServiceWorkerRegistrationKey&, bool unregistrationResult);
    void startScriptFetch(const ServiceWorkerJobData&, SWServerRegistration&);
    void refreshImportedScripts(const ServiceWorkerJobData&, SWServerRegistration&, const Vector<URL>&, const std::optional<ProcessIdentifier>&);

    void updateWorker(const ServiceWorkerJobDataIdentifier&, const std::optional<ProcessIdentifier>&, SWServerRegistration&, const URL&, const ScriptBuffer&, const CertificateInfo&, const ContentSecurityPolicyResponseHeaders&, const CrossOriginEmbedderPolicy&, const String& referrerPolicy, WorkerType, MemoryCompactRobinHoodHashMap<URL, ServiceWorkerContextData::ImportedScript>&&, std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier);
    void fireInstallEvent(SWServerWorker&);
    void fireActivateEvent(SWServerWorker&);

    WEBCORE_EXPORT SWServerWorker* workerByID(ServiceWorkerIdentifier) const;
    WEBCORE_EXPORT std::optional<ServiceWorkerClientData> serviceWorkerClientWithOriginByID(const ClientOrigin&, const ScriptExecutionContextIdentifier&) const;
    WEBCORE_EXPORT std::optional<ServiceWorkerClientData> topLevelServiceWorkerClientFromPageIdentifier(const ClientOrigin&, PageIdentifier) const;
    String serviceWorkerClientUserAgent(const ClientOrigin&) const;
    WEBCORE_EXPORT SWServerWorker* activeWorkerFromRegistrationID(ServiceWorkerRegistrationIdentifier);

    WEBCORE_EXPORT void markAllWorkersForRegistrableDomainAsTerminated(const RegistrableDomain&);
    
    WEBCORE_EXPORT void addConnection(std::unique_ptr<Connection>&&);
    WEBCORE_EXPORT void removeConnection(SWServerConnectionIdentifier);
    Connection* connection(SWServerConnectionIdentifier identifier) const { return m_connections.get(identifier); }

    const HashMap<SWServerConnectionIdentifier, std::unique_ptr<Connection>>& connections() const { return m_connections; }
    WEBCORE_EXPORT bool canHandleScheme(StringView) const;

    SWOriginStore& originStore() { return m_originStore; }

    void refreshImportedScriptsFinished(const ServiceWorkerJobDataIdentifier&, const ServiceWorkerRegistrationKey&, const Vector<std::pair<URL, ScriptBuffer>>&, const std::optional<ProcessIdentifier>&);
    void scriptContextFailedToStart(const std::optional<ServiceWorkerJobDataIdentifier>&, SWServerWorker&, const String& message);
    void scriptContextStarted(const std::optional<ServiceWorkerJobDataIdentifier>&, SWServerWorker&);
    void didFinishInstall(const std::optional<ServiceWorkerJobDataIdentifier>&, SWServerWorker&, bool wasSuccessful);
    void didFinishActivation(SWServerWorker&);
    void workerContextTerminated(SWServerWorker&);
    void matchAll(SWServerWorker&, const ServiceWorkerClientQueryOptions&, ServiceWorkerClientsMatchAllCallback&&);
    std::optional<ExceptionData> claim(SWServerWorker&);
    WEBCORE_EXPORT void terminateContextConnectionWhenPossible(const RegistrableDomain&, ProcessIdentifier);

    WEBCORE_EXPORT static HashSet<SWServer*>& allServers();

    WEBCORE_EXPORT void registerServiceWorkerClient(ClientOrigin&&, ServiceWorkerClientData&&, const std::optional<ServiceWorkerRegistrationIdentifier>&, String&& userAgent);
    WEBCORE_EXPORT void unregisterServiceWorkerClient(const ClientOrigin&, ScriptExecutionContextIdentifier);

    using RunServiceWorkerCallback = Function<void(SWServerToContextConnection*)>;
    WEBCORE_EXPORT void runServiceWorkerIfNecessary(ServiceWorkerIdentifier, RunServiceWorkerCallback&&);

    void resolveRegistrationReadyRequests(SWServerRegistration&);

    void addRegistrationFromStore(ServiceWorkerContextData&&, CompletionHandler<void()>&&);
    void didSaveWorkerScriptsToDisk(ServiceWorkerIdentifier, ScriptBuffer&& mainScript, MemoryCompactRobinHoodHashMap<URL, ScriptBuffer>&& importedScripts);
    void registrationStoreImportComplete();
    void registrationStoreDatabaseFailedToOpen();
    void storeRegistrationForWorker(SWServerWorker&);

    WEBCORE_EXPORT void getOriginsWithRegistrations(Function<void(const HashSet<SecurityOriginData>&)>&&);

    PAL::SessionID sessionID() const { return m_sessionID; }
    WEBCORE_EXPORT bool needsContextConnectionForRegistrableDomain(const RegistrableDomain&) const;

    void removeFromScopeToRegistrationMap(const ServiceWorkerRegistrationKey&);

    WEBCORE_EXPORT void addContextConnection(SWServerToContextConnection&);
    WEBCORE_EXPORT void removeContextConnection(SWServerToContextConnection&);

    SWServerToContextConnection* contextConnectionForRegistrableDomain(const RegistrableDomain& domain) { return m_contextConnections.get(domain); }
    WEBCORE_EXPORT void createContextConnection(const RegistrableDomain&, std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier);

    bool isImportCompleted() const { return m_importCompleted; }
    WEBCORE_EXPORT void whenImportIsCompleted(CompletionHandler<void()>&&);

    void softUpdate(SWServerRegistration&);

    WEBCORE_EXPORT void handleLowMemoryWarning();

    static constexpr Seconds defaultTerminationDelay = 10_s;
    static constexpr Seconds defaultFunctionalEventDuration = 2_s;

    LastNavigationWasAppInitiated clientIsAppInitiatedForRegistrableDomain(const RegistrableDomain&);
    bool shouldRunServiceWorkersOnMainThreadForTesting() const { return m_shouldRunServiceWorkersOnMainThreadForTesting; }

    WEBCORE_EXPORT void processPushMessage(std::optional<Vector<uint8_t>>&&, URL&&, CompletionHandler<void(bool)>&&);
    WEBCORE_EXPORT void processNotificationEvent(NotificationData&&, NotificationEventType, CompletionHandler<void(bool)>&&);

    enum class ShouldSkipEvent : bool { No, Yes };
    void fireFunctionalEvent(SWServerRegistration&, CompletionHandler<void(Expected<SWServerToContextConnection*, ShouldSkipEvent>)>&&);

    ScriptExecutionContextIdentifier clientIdFromVisibleClientId(const String& visibleIdentifier) const { return m_visibleClientIdToInternalClientIdMap.get(visibleIdentifier); }

    void forEachServiceWorker(const Function<bool(const SWServerWorker&)>&) const;
    bool hasClientsWithOrigin(const ClientOrigin& origin) { return m_clientIdentifiersPerOrigin.contains(origin); }

    enum class ShouldDelayRemoval : bool { No, Yes };
    ShouldDelayRemoval removeContextConnectionIfPossible(const RegistrableDomain&);

    std::optional<ServiceWorkerRegistrationIdentifier> clientIdentifierToControllingRegistration(ScriptExecutionContextIdentifier) const;
    WEBCORE_EXPORT void forEachClientForOrigin(const ClientOrigin&, const Function<void(ServiceWorkerClientData&)>&);

    struct GatheredClientData {
        ClientOrigin clientOrigin;
        ServiceWorkerClientData serviceWorkerClientData;
        std::optional<ServiceWorkerRegistrationIdentifier> controllingServiceWorkerRegistrationIdentifier;
        String userAgent;
    };
    WEBCORE_EXPORT std::optional<GatheredClientData> gatherClientData(const ClientOrigin&, ScriptExecutionContextIdentifier);

private:
    unsigned maxRegistrationCount();
    bool allowLoopbackIPAddress(const String&);
    void validateRegistrationDomain(RegistrableDomain, ServiceWorkerJobType, bool, CompletionHandler<void(bool)>&&);

    void scriptFetchFinished(const ServiceWorkerJobDataIdentifier&, const ServiceWorkerRegistrationKey&, const std::optional<ProcessIdentifier>&, WorkerFetchResult&&);

    void didResolveRegistrationPromise(Connection*, const ServiceWorkerRegistrationKey&);

    void addClientServiceWorkerRegistration(Connection&, ServiceWorkerRegistrationIdentifier);
    void removeClientServiceWorkerRegistration(Connection&, ServiceWorkerRegistrationIdentifier);

    void terminatePreinstallationWorker(SWServerWorker&);

    WEBCORE_EXPORT SWServerRegistration* doRegistrationMatching(const SecurityOriginData& topOrigin, const URL& clientURL);
    bool runServiceWorker(ServiceWorkerIdentifier);

    void tryInstallContextData(const std::optional<ProcessIdentifier>&, ServiceWorkerContextData&&);
    void installContextData(const ServiceWorkerContextData&);

    SWServerRegistration* registrationFromServiceWorkerIdentifier(ServiceWorkerIdentifier);

    void performGetOriginsWithRegistrationsCallbacks();

    void contextConnectionCreated(SWServerToContextConnection&);

    void updateAppInitiatedValueForWorkers(const ClientOrigin&, LastNavigationWasAppInitiated);
    void whenImportIsCompletedIfNeeded(CompletionHandler<void()>&&);

    ResourceRequest createScriptRequest(const URL&, const ServiceWorkerJobData&, SWServerRegistration&);

    HashMap<SWServerConnectionIdentifier, std::unique_ptr<Connection>> m_connections;
    HashMap<ServiceWorkerRegistrationKey, WeakPtr<SWServerRegistration>> m_scopeToRegistrationMap;
    HashMap<ServiceWorkerRegistrationIdentifier, std::unique_ptr<SWServerRegistration>> m_registrations;
    HashMap<ServiceWorkerRegistrationKey, std::unique_ptr<SWServerJobQueue>> m_jobQueues;

    HashMap<ServiceWorkerIdentifier, Ref<SWServerWorker>> m_runningOrTerminatingWorkers;

    HashMap<RegistrableDomain, HashSet<ScriptExecutionContextIdentifier>> m_clientsByRegistrableDomain;
    struct Clients {
        Vector<ScriptExecutionContextIdentifier> identifiers;
        std::unique_ptr<Timer> terminateServiceWorkersTimer;
        String userAgent;
    };
    HashMap<ClientOrigin, Clients> m_clientIdentifiersPerOrigin;
    HashMap<ScriptExecutionContextIdentifier, WeakPtr<SWServerRegistration>> m_serviceWorkerPageIdentifierToRegistrationMap;
    HashMap<ScriptExecutionContextIdentifier, UniqueRef<ServiceWorkerClientData>> m_clientsById;
    HashMap<ScriptExecutionContextIdentifier, ServiceWorkerRegistrationIdentifier> m_clientToControllingRegistration;
    MemoryCompactRobinHoodHashMap<String, ScriptExecutionContextIdentifier> m_visibleClientIdToInternalClientIdMap;

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

    // FIXME: This is a lot of callbacks. Make a delegate interface instead.
    CreateContextConnectionCallback m_createContextConnectionCallback;
    HashSet<RegistrableDomain> m_pendingConnectionDomains;
    Vector<CompletionHandler<void()>> m_importCompletedCallbacks;
    SoftUpdateCallback m_softUpdateCallback;
    AppBoundDomainsCallback m_appBoundDomainsCallback;
    AddAllowedFirstPartyForCookiesCallback m_addAllowedFirstPartyForCookiesCallback;
    
    HashSet<RegistrableDomain> m_appBoundDomains;
    bool m_shouldRunServiceWorkersOnMainThreadForTesting { false };
    bool m_hasServiceWorkerEntitlement { false };
    bool m_hasReceivedAppBoundDomains { false };
    unsigned m_uniqueRegistrationCount { 0 };
    std::optional<unsigned> m_overrideServiceWorkerRegistrationCountTestingValue;
    uint64_t m_focusOrder { 0 };
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
