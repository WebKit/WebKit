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
#include "ContentSecurityPolicyResponseHeaders.h"
#include "CrossOriginEmbedderPolicy.h"
#include "RegistrableDomain.h"
#include "ScriptExecutionContextIdentifier.h"
#include "ServiceWorkerClientData.h"
#include "ServiceWorkerContextData.h"
#include "ServiceWorkerData.h"
#include "ServiceWorkerIdentifier.h"
#include "ServiceWorkerRegistrationKey.h"
#include "ServiceWorkerTypes.h"
#include "Timer.h"
#include <wtf/CompletionHandler.h>
#include <wtf/RefCounted.h>
#include <wtf/URLHash.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class SWServer;
class SWServerRegistration;
class SWServerToContextConnection;
struct ServiceWorkerClientQueryOptions;
struct ServiceWorkerContextData;
struct ServiceWorkerJobDataIdentifier;
enum class WorkerThreadMode : bool;
enum class WorkerType : bool;

class SWServerWorker : public RefCounted<SWServerWorker> {
public:
    template <typename... Args> static Ref<SWServerWorker> create(Args&&... args)
    {
        return adoptRef(*new SWServerWorker(std::forward<Args>(args)...));
    }
    
    SWServerWorker(const SWServerWorker&) = delete;
    WEBCORE_EXPORT ~SWServerWorker();

    WEBCORE_EXPORT void terminate(CompletionHandler<void()>&& = [] { });
    WEBCORE_EXPORT void whenTerminated(CompletionHandler<void()>&&);

    WEBCORE_EXPORT void whenActivated(CompletionHandler<void(bool)>&&);

    enum class State {
        Running,
        Terminating,
        NotRunning,
    };
    bool isRunning() const { return m_state == State::Running; }
    bool isTerminating() const { return m_state == State::Terminating; }
    bool isNotRunning() const { return m_state == State::NotRunning; }
    void setState(State);

    SWServer* server() { return m_server.get(); }
    const ServiceWorkerRegistrationKey& registrationKey() const { return m_registrationKey; }
    const URL& scriptURL() const { return m_data.scriptURL; }
    const ScriptBuffer& script() const { return m_script; }
    const CertificateInfo& certificateInfo() const { return m_certificateInfo; }
    WorkerType type() const { return m_data.type; }

    ServiceWorkerIdentifier identifier() const { return m_data.identifier; }

    ServiceWorkerState state() const { return m_data.state; }
    bool isActive() const { return m_data.state == ServiceWorkerState::Activated || m_data.state == ServiceWorkerState::Activating; }

    void setState(ServiceWorkerState);

    bool hasPendingEvents() const { return m_hasPendingEvents; }
    void setHasPendingEvents(bool);

    void scriptContextFailedToStart(const std::optional<ServiceWorkerJobDataIdentifier>&, const String& message);
    void scriptContextStarted(const std::optional<ServiceWorkerJobDataIdentifier>&, bool doesHandleFetch);
    void didFinishInstall(const std::optional<ServiceWorkerJobDataIdentifier>&, bool wasSuccessful);
    void didFinishActivation();
    void contextTerminated();
    WEBCORE_EXPORT std::optional<ServiceWorkerClientData> findClientByIdentifier(const ScriptExecutionContextIdentifier&) const;
    WEBCORE_EXPORT void findClientByVisibleIdentifier(const String& clientIdentifier, CompletionHandler<void(std::optional<WebCore::ServiceWorkerClientData>&&)>&&);
    void matchAll(const ServiceWorkerClientQueryOptions&, ServiceWorkerClientsMatchAllCallback&&);
    void setScriptResource(URL&&, ServiceWorkerContextData::ImportedScript&&);
    void didSaveScriptsToDisk(ScriptBuffer&& mainScript, HashMap<URL, ScriptBuffer>&& importedScripts);

    void skipWaiting();
    bool isSkipWaitingFlagSet() const { return m_isSkipWaitingFlagSet; }

    WEBCORE_EXPORT static SWServerWorker* existingWorkerForIdentifier(ServiceWorkerIdentifier);
    static HashMap<ServiceWorkerIdentifier, SWServerWorker*>& allWorkers();

    const ServiceWorkerData& data() const { return m_data; }
    ServiceWorkerContextData contextData() const;

    const ClientOrigin& origin() const;
    const RegistrableDomain& registrableDomain() const { return m_registrableDomain; }
    WEBCORE_EXPORT std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier() const;

    WEBCORE_EXPORT SWServerToContextConnection* contextConnection();
    String userAgent() const;

    bool shouldSkipFetchEvent() const { return m_shouldSkipHandleFetch; }
    
    SWServerRegistration* registration() const;

    void setHasTimedOutAnyFetchTasks() { m_hasTimedOutAnyFetchTasks = true; }
    bool hasTimedOutAnyFetchTasks() const { return m_hasTimedOutAnyFetchTasks; }
    void didFailHeartBeatCheck();
    void updateAppInitiatedValue(LastNavigationWasAppInitiated);

    WorkerThreadMode workerThreadMode() const;

private:
    SWServerWorker(SWServer&, SWServerRegistration&, const URL&, const ScriptBuffer&, const CertificateInfo&, const ContentSecurityPolicyResponseHeaders&, const CrossOriginEmbedderPolicy&, String&& referrerPolicy, WorkerType, ServiceWorkerIdentifier, HashMap<URL, ServiceWorkerContextData::ImportedScript>&&);

    void callWhenActivatedHandler(bool success);

    void startTermination(CompletionHandler<void()>&&);
    void terminationCompleted();
    void terminationTimerFired();
    void callTerminationCallbacks();

    WeakPtr<SWServer> m_server;
    ServiceWorkerRegistrationKey m_registrationKey;
    WeakPtr<SWServerRegistration> m_registration;
    ServiceWorkerData m_data;
    ScriptBuffer m_script;
    CertificateInfo m_certificateInfo;
    ContentSecurityPolicyResponseHeaders m_contentSecurityPolicy;
    CrossOriginEmbedderPolicy m_crossOriginEmbedderPolicy;
    String m_referrerPolicy;
    bool m_hasPendingEvents { false };
    State m_state { State::NotRunning };
    mutable std::optional<ClientOrigin> m_origin;
    RegistrableDomain m_registrableDomain;
    bool m_isSkipWaitingFlagSet { false };
    Vector<CompletionHandler<void(bool)>> m_whenActivatedHandlers;
    HashMap<URL, ServiceWorkerContextData::ImportedScript> m_scriptResourceMap;
    bool m_shouldSkipHandleFetch { false };
    bool m_hasTimedOutAnyFetchTasks { false };
    Vector<CompletionHandler<void()>> m_terminationCallbacks;
    Timer m_terminationTimer;
    LastNavigationWasAppInitiated m_lastNavigationWasAppInitiated;
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
