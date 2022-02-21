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

#include "NotificationClient.h"
#include "ScriptExecutionContextIdentifier.h"
#include "ServiceWorkerContextData.h"
#include "ServiceWorkerRegistration.h"
#include "WorkerGlobalScope.h"
#include <wtf/URLHash.h>

namespace WebCore {

class DeferredPromise;
class ExtendableEvent;
class Page;
class ServiceWorkerClient;
class ServiceWorkerClients;
class ServiceWorkerThread;

enum class NotificationEventType : bool;

struct ServiceWorkerClientData;

class ServiceWorkerGlobalScope final : public WorkerGlobalScope {
    WTF_MAKE_ISO_ALLOCATED(ServiceWorkerGlobalScope);
public:
    static Ref<ServiceWorkerGlobalScope> create(ServiceWorkerContextData&&, ServiceWorkerData&&, const WorkerParameters&, Ref<SecurityOrigin>&&, ServiceWorkerThread&, Ref<SecurityOrigin>&& topOrigin, IDBClient::IDBConnectionProxy*, SocketProvider*, std::unique_ptr<NotificationClient>&&, PAL::SessionID);

    ~ServiceWorkerGlobalScope();

    bool isServiceWorkerGlobalScope() const final { return true; }

    ServiceWorkerClients& clients() { return m_clients.get(); }
    ServiceWorkerRegistration& registration() { return m_registration.get(); }
    ServiceWorker& serviceWorker() { return m_serviceWorker.get(); }
    
    void skipWaiting(Ref<DeferredPromise>&&);

    EventTargetInterface eventTargetInterface() const final;

    ServiceWorkerThread& thread();

    ServiceWorkerClient* serviceWorkerClient(ScriptExecutionContextIdentifier);
    void addServiceWorkerClient(ServiceWorkerClient&);
    void removeServiceWorkerClient(ServiceWorkerClient&);

    void updateExtendedEventsSet(ExtendableEvent* newEvent = nullptr);

    const ServiceWorkerContextData::ImportedScript* scriptResource(const URL&) const;
    void setScriptResource(const URL&, ServiceWorkerContextData::ImportedScript&&);

    void didSaveScriptsToDisk(ScriptBuffer&&, HashMap<URL, ScriptBuffer>&& importedScripts);

    const ServiceWorkerContextData& contextData() const { return m_contextData; }
    const CertificateInfo& certificateInfo() const { return m_contextData.certificateInfo; }

    FetchOptions::Destination destination() const final { return FetchOptions::Destination::Serviceworker; }

    WEBCORE_EXPORT Page* serviceWorkerPage();

#if ENABLE(NOTIFICATION_EVENT)
    void postTaskToFireNotificationEvent(NotificationEventType, Notification&, const String& action);
#endif

private:
    ServiceWorkerGlobalScope(ServiceWorkerContextData&&, ServiceWorkerData&&, const WorkerParameters&, Ref<SecurityOrigin>&&, ServiceWorkerThread&, Ref<SecurityOrigin>&& topOrigin, IDBClient::IDBConnectionProxy*, SocketProvider*, std::unique_ptr<NotificationClient>&&, PAL::SessionID);
    void notifyServiceWorkerPageOfCreationIfNecessary();

    Type type() const final { return Type::ServiceWorker; }
    bool hasPendingEvents() const { return !m_extendedEvents.isEmpty(); }

    NotificationClient* notificationClient() final { return m_notificationClient.get(); }
    std::optional<PAL::SessionID> sessionID() const final { return m_sessionID; }

    ServiceWorkerContextData m_contextData;
    Ref<ServiceWorkerRegistration> m_registration;
    Ref<ServiceWorker> m_serviceWorker;
    Ref<ServiceWorkerClients> m_clients;
    HashMap<ScriptExecutionContextIdentifier, ServiceWorkerClient*> m_clientMap;
    Vector<Ref<ExtendableEvent>> m_extendedEvents;

    uint64_t m_lastRequestIdentifier { 0 };
    HashMap<uint64_t, RefPtr<DeferredPromise>> m_pendingSkipWaitingPromises;
    PAL::SessionID m_sessionID;
    std::unique_ptr<NotificationClient> m_notificationClient;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ServiceWorkerGlobalScope)
    static bool isType(const WebCore::ScriptExecutionContext& context) { return is<WebCore::WorkerGlobalScope>(context) && downcast<WebCore::WorkerGlobalScope>(context).type() == WebCore::WorkerGlobalScope::Type::ServiceWorker; }
    static bool isType(const WebCore::WorkerGlobalScope& context) { return context.type() == WebCore::WorkerGlobalScope::Type::ServiceWorker; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(SERVICE_WORKER)
