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

#include "CookieStore.h"
#include "NotificationClient.h"
#include "ScriptExecutionContextIdentifier.h"
#include "ServiceWorkerContextData.h"
#include "ServiceWorkerRegistration.h"
#include "WorkerGlobalScope.h"
#include <wtf/MonotonicTime.h>
#include <wtf/URLHash.h>

namespace WebCore {

class DeferredPromise;
class ExtendableEvent;
class Page;
class PushEvent;
class ServiceWorkerClient;
class ServiceWorkerClients;
class ServiceWorkerThread;

#if ENABLE(DECLARATIVE_WEB_PUSH)
class PushNotificationEvent;
#endif

enum class NotificationEventType : bool;

struct ServiceWorkerClientData;

class ServiceWorkerGlobalScope final : public WorkerGlobalScope {
    WTF_MAKE_ISO_ALLOCATED(ServiceWorkerGlobalScope);
public:
    static Ref<ServiceWorkerGlobalScope> create(ServiceWorkerContextData&&, ServiceWorkerData&&, const WorkerParameters&, Ref<SecurityOrigin>&&, ServiceWorkerThread&, Ref<SecurityOrigin>&& topOrigin, IDBClient::IDBConnectionProxy*, SocketProvider*, std::unique_ptr<NotificationClient>&&);

    ~ServiceWorkerGlobalScope();

    bool isServiceWorkerGlobalScope() const final { return true; }

    ServiceWorkerClients& clients() { return m_clients.get(); }
    ServiceWorkerRegistration& registration() { return m_registration.get(); }
    ServiceWorker& serviceWorker() { return m_serviceWorker.get(); }
    
    void skipWaiting(Ref<DeferredPromise>&&);

    EventTargetInterface eventTargetInterface() const final;

    ServiceWorkerThread& thread();

    void updateExtendedEventsSet(ExtendableEvent* newEvent = nullptr);

    const ServiceWorkerContextData::ImportedScript* scriptResource(const URL&) const;
    void setScriptResource(const URL&, ServiceWorkerContextData::ImportedScript&&);

    void didSaveScriptsToDisk(ScriptBuffer&&, HashMap<URL, ScriptBuffer>&& importedScripts);

    const ServiceWorkerContextData& contextData() const { return m_contextData; }
    const CertificateInfo& certificateInfo() const { return m_contextData.certificateInfo; }

    FetchOptions::Destination destination() const final { return FetchOptions::Destination::Serviceworker; }

    WEBCORE_EXPORT Page* serviceWorkerPage();

    void dispatchPushEvent(PushEvent&);
    PushEvent* pushEvent() { return m_pushEvent.get(); }

#if ENABLE(DECLARATIVE_WEB_PUSH)
    void dispatchPushNotificationEvent(PushNotificationEvent&);
    PushNotificationEvent* pushNotificationEvent() { return m_pushNotificationEvent.get(); }
    void clearPushNotificationEvent();
#endif

    bool hasPendingSilentPushEvent() const { return m_hasPendingSilentPushEvent; }
    void setHasPendingSilentPushEvent(bool value) { m_hasPendingSilentPushEvent = value; }

    constexpr static Seconds userGestureLifetime  { 2_s };
    bool isProcessingUserGesture() const { return m_isProcessingUserGesture; }
    void recordUserGesture();
    void setIsProcessingUserGestureForTesting(bool value) { m_isProcessingUserGesture = value; }

    bool didFirePushEventRecently() const;

    WEBCORE_EXPORT void addConsoleMessage(MessageSource, MessageLevel, const String& message, unsigned long requestIdentifier) final;
    void enableConsoleMessageReporting() { m_consoleMessageReportingEnabled = true; }

    CookieStore& cookieStore();

private:
    ServiceWorkerGlobalScope(ServiceWorkerContextData&&, ServiceWorkerData&&, const WorkerParameters&, Ref<SecurityOrigin>&&, ServiceWorkerThread&, Ref<SecurityOrigin>&& topOrigin, IDBClient::IDBConnectionProxy*, SocketProvider*, std::unique_ptr<NotificationClient>&&);
    void notifyServiceWorkerPageOfCreationIfNecessary();

    void prepareForDestruction() final;

    Type type() const final { return Type::ServiceWorker; }
    bool hasPendingEvents() const { return !m_extendedEvents.isEmpty(); }

    NotificationClient* notificationClient() final { return m_notificationClient.get(); }

    void resetUserGesture() { m_isProcessingUserGesture = false; }

    ServiceWorkerContextData m_contextData;
    Ref<ServiceWorkerRegistration> m_registration;
    Ref<ServiceWorker> m_serviceWorker;
    Ref<ServiceWorkerClients> m_clients;
    Vector<Ref<ExtendableEvent>> m_extendedEvents;

    uint64_t m_lastRequestIdentifier { 0 };
    HashMap<uint64_t, RefPtr<DeferredPromise>> m_pendingSkipWaitingPromises;
    std::unique_ptr<NotificationClient> m_notificationClient;
    bool m_hasPendingSilentPushEvent { false };
    bool m_isProcessingUserGesture { false };
    Timer m_userGestureTimer;
    RefPtr<PushEvent> m_pushEvent;
#if ENABLE(DECLARATIVE_WEB_PUSH)
    RefPtr<PushNotificationEvent> m_pushNotificationEvent;
#endif
    MonotonicTime m_lastPushEventTime;
    bool m_consoleMessageReportingEnabled { false };
    RefPtr<CookieStore> m_cookieStore;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ServiceWorkerGlobalScope)
    static bool isType(const WebCore::ScriptExecutionContext& context) { return is<WebCore::WorkerGlobalScope>(context) && downcast<WebCore::WorkerGlobalScope>(context).type() == WebCore::WorkerGlobalScope::Type::ServiceWorker; }
    static bool isType(const WebCore::WorkerGlobalScope& context) { return context.type() == WebCore::WorkerGlobalScope::Type::ServiceWorker; }
SPECIALIZE_TYPE_TRAITS_END()
