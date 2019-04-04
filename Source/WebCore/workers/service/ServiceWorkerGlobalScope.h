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

#include "ServiceWorkerClientIdentifier.h"
#include "ServiceWorkerContextData.h"
#include "ServiceWorkerRegistration.h"
#include "WorkerGlobalScope.h"

namespace WebCore {

class DeferredPromise;
class ExtendableEvent;
struct ServiceWorkerClientData;
class ServiceWorkerClient;
class ServiceWorkerClients;
class ServiceWorkerThread;

class ServiceWorkerGlobalScope final : public WorkerGlobalScope {
    WTF_MAKE_ISO_ALLOCATED(ServiceWorkerGlobalScope);
public:
    static Ref<ServiceWorkerGlobalScope> create(const ServiceWorkerContextData&, const URL&, Ref<SecurityOrigin>&&, const String& identifier, const String& userAgent, bool isOnline, ServiceWorkerThread&, const ContentSecurityPolicyResponseHeaders&, bool shouldBypassMainWorldContentSecurityPolicy, Ref<SecurityOrigin>&& topOrigin, MonotonicTime timeOrigin, IDBClient::IDBConnectionProxy*, SocketProvider*, PAL::SessionID);

    ~ServiceWorkerGlobalScope();

    bool isServiceWorkerGlobalScope() const final { return true; }

    ServiceWorkerClients& clients() { return m_clients.get(); }
    ServiceWorkerRegistration& registration() { return m_registration.get(); }
    
    void skipWaiting(Ref<DeferredPromise>&&);

    EventTargetInterface eventTargetInterface() const final;

    ServiceWorkerThread& thread();

    ServiceWorkerClient* serviceWorkerClient(ServiceWorkerClientIdentifier);
    void addServiceWorkerClient(ServiceWorkerClient&);
    void removeServiceWorkerClient(ServiceWorkerClient&);

    void updateExtendedEventsSet(ExtendableEvent* newEvent = nullptr);

    const ServiceWorkerContextData::ImportedScript* scriptResource(const URL&) const;
    void setScriptResource(const URL&, ServiceWorkerContextData::ImportedScript&&);

private:
    ServiceWorkerGlobalScope(const ServiceWorkerContextData&, const URL&, Ref<SecurityOrigin>&&, const String& identifier, const String& userAgent, bool isOnline, ServiceWorkerThread&, bool shouldBypassMainWorldContentSecurityPolicy, Ref<SecurityOrigin>&& topOrigin, MonotonicTime timeOrigin, IDBClient::IDBConnectionProxy*, SocketProvider*, PAL::SessionID);

    bool hasPendingEvents() const { return !m_extendedEvents.isEmpty(); }

    ServiceWorkerContextData m_contextData;
    Ref<ServiceWorkerRegistration> m_registration;
    Ref<ServiceWorkerClients> m_clients;
    HashMap<ServiceWorkerClientIdentifier, ServiceWorkerClient*> m_clientMap;
    Vector<Ref<ExtendableEvent>> m_extendedEvents;

    uint64_t m_lastRequestIdentifier { 0 };
    HashMap<uint64_t, RefPtr<DeferredPromise>> m_pendingSkipWaitingPromises;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ServiceWorkerGlobalScope)
    static bool isType(const WebCore::ScriptExecutionContext& context) { return is<WebCore::WorkerGlobalScope>(context) && downcast<WebCore::WorkerGlobalScope>(context).isServiceWorkerGlobalScope(); }
    static bool isType(const WebCore::WorkerGlobalScope& context) { return context.isServiceWorkerGlobalScope(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(SERVICE_WORKER)
