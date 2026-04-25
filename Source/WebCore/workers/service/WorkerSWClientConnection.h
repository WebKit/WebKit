/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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

#include "SWClientConnection.h"
#include <wtf/Forward.h>
#include <wtf/ObjectIdentifier.h>

namespace WebCore {

class WorkerGlobalScope;
class WorkerThread;

struct CookieChangeSubscription;

class WorkerSWClientConnection final : public SWClientConnection {
public:
    static Ref<WorkerSWClientConnection> create(WorkerGlobalScope& scope) { return adoptRef(*new WorkerSWClientConnection { scope }); }
    ~WorkerSWClientConnection();

    void registerServiceWorkerClient(const ClientOrigin&, ServiceWorkerClientData&&, const std::optional<ServiceWorkerRegistrationIdentifier>&, String&& userAgent) final;
    void unregisterServiceWorkerClient(ScriptExecutionContextIdentifier) final;

private:
    explicit WorkerSWClientConnection(WorkerGlobalScope&);

    void matchRegistration(SecurityOriginData&& topOrigin, const URL& clientURL, RegistrationCallback&&) final;
    void getRegistrations(SecurityOriginData&& topOrigin, const URL& clientURL, GetRegistrationsCallback&&) final;
    void whenRegistrationReady(const SecurityOriginData& topOrigin, const URL& clientURL, WhenRegistrationReadyCallback&&) final;
    void addServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier) final;
    void removeServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier) final;
    void registerServiceWorkerInServer(ServiceWorkerIdentifier) final;
    void unregisterServiceWorkerInServer(ServiceWorkerIdentifier) final;
    void didResolveRegistrationPromise(const ServiceWorkerRegistrationKey&) final;
    void postMessageToServiceWorker(ServiceWorkerIdentifier destination, MessageWithMessagePorts&&, const ServiceWorkerOrClientIdentifier& source) final;
    SWServerConnectionIdentifier serverConnectionIdentifier() const final;
    bool mayHaveServiceWorkerRegisteredForOrigin(const SecurityOriginData&) const final;
    void finishFetchingScriptInServer(const ServiceWorkerJobDataIdentifier&, ServiceWorkerRegistrationKey&&, WorkerFetchResult&&) final;
    void scheduleJobInServer(const ServiceWorkerJobData&) final;
    void scheduleJob(ServiceWorkerOrClientIdentifier, const ServiceWorkerJobData&) final;
    void scheduleUnregisterJobInServer(ServiceWorkerRegistrationIdentifier, ServiceWorkerOrClientIdentifier, CompletionHandler<void(ExceptionOr<bool>&&)>&&) final;
    void subscribeToPushService(ServiceWorkerRegistrationIdentifier, const Vector<uint8_t>& applicationServerKey, SubscribeToPushServiceCallback&&) final;
    void unsubscribeFromPushService(ServiceWorkerRegistrationIdentifier, PushSubscriptionIdentifier, UnsubscribeFromPushServiceCallback&&) final;
    void getPushSubscription(ServiceWorkerRegistrationIdentifier, GetPushSubscriptionCallback&&) final;
    void getPushPermissionState(ServiceWorkerRegistrationIdentifier, GetPushPermissionStateCallback&&) final;
#if ENABLE(NOTIFICATION_EVENT)
    void getNotifications(const URL&, const String&, GetNotificationsCallback&&) final;
#endif

    void enableNavigationPreload(ServiceWorkerRegistrationIdentifier, ExceptionOrVoidCallback&&) final;
    void disableNavigationPreload(ServiceWorkerRegistrationIdentifier, ExceptionOrVoidCallback&&) final;
    void setNavigationPreloadHeaderValue(ServiceWorkerRegistrationIdentifier, String&&, ExceptionOrVoidCallback&&) final;
    void getNavigationPreloadState(ServiceWorkerRegistrationIdentifier, ExceptionOrNavigationPreloadStateCallback&&) final;

    void startBackgroundFetch(ServiceWorkerRegistrationIdentifier, const String&, Vector<BackgroundFetchRequest>&&, BackgroundFetchOptions&&, ExceptionOrBackgroundFetchInformationCallback&&) final;
    void backgroundFetchInformation(ServiceWorkerRegistrationIdentifier, const String&, ExceptionOrBackgroundFetchInformationCallback&&) final;
    void backgroundFetchIdentifiers(ServiceWorkerRegistrationIdentifier, BackgroundFetchIdentifiersCallback&&) final;
    void abortBackgroundFetch(ServiceWorkerRegistrationIdentifier, const String&, AbortBackgroundFetchCallback&&) final;
    void matchBackgroundFetch(ServiceWorkerRegistrationIdentifier, const String&, RetrieveRecordsOptions&&, MatchBackgroundFetchCallback&&) final;
    void retrieveRecordResponse(BackgroundFetchRecordIdentifier, RetrieveRecordResponseCallback&&) final;
    void retrieveRecordResponseBody(BackgroundFetchRecordIdentifier, RetrieveRecordResponseBodyCallback&&) final;

    void addCookieChangeSubscriptions(ServiceWorkerRegistrationIdentifier, Vector<CookieChangeSubscription>&&, ExceptionOrVoidCallback&&) final;
    void removeCookieChangeSubscriptions(ServiceWorkerRegistrationIdentifier, Vector<CookieChangeSubscription>&&, ExceptionOrVoidCallback&&) final;
    void cookieChangeSubscriptions(ServiceWorkerRegistrationIdentifier, ExceptionOrCookieChangeSubscriptionsCallback&&) final;
    Ref<AddRoutePromise> addRoutes(ServiceWorkerRegistrationIdentifier, Vector<ServiceWorkerRoute>&&) final;

    const Ref<WorkerThread> m_thread;

    struct SWClientRequestIdentifierType;
    using SWClientRequestIdentifier = AtomicObjectIdentifier<SWClientRequestIdentifierType>;

    HashMap<SWClientRequestIdentifier, RegistrationCallback> m_matchRegistrationRequests;
    HashMap<SWClientRequestIdentifier, GetRegistrationsCallback> m_getRegistrationsRequests;
    HashMap<SWClientRequestIdentifier, WhenRegistrationReadyCallback> m_whenRegistrationReadyRequests;
    HashMap<SWClientRequestIdentifier, CompletionHandler<void(ExceptionOr<bool>&&)>> m_unregisterRequests;
    HashMap<SWClientRequestIdentifier, SubscribeToPushServiceCallback> m_subscribeToPushServiceRequests;
    HashMap<SWClientRequestIdentifier, UnsubscribeFromPushServiceCallback> m_unsubscribeFromPushServiceRequests;
    HashMap<SWClientRequestIdentifier, GetPushSubscriptionCallback> m_getPushSubscriptionRequests;
    HashMap<SWClientRequestIdentifier, GetPushPermissionStateCallback> m_getPushPermissionStateCallbacks;
    HashMap<SWClientRequestIdentifier, ExceptionOrVoidCallback> m_voidCallbacks;
    HashMap<SWClientRequestIdentifier, ExceptionOrNavigationPreloadStateCallback> m_navigationPreloadStateCallbacks;
#if ENABLE(NOTIFICATION_EVENT)
    HashMap<SWClientRequestIdentifier, GetNotificationsCallback> m_getNotificationsCallbacks;
#endif
    HashMap<SWClientRequestIdentifier, ExceptionOrBackgroundFetchInformationCallback> m_backgroundFetchInformationCallbacks;
    HashMap<SWClientRequestIdentifier, BackgroundFetchIdentifiersCallback> m_backgroundFetchIdentifiersCallbacks;
    HashMap<SWClientRequestIdentifier, AbortBackgroundFetchCallback> m_abortBackgroundFetchCallbacks;
    HashMap<SWClientRequestIdentifier, MatchBackgroundFetchCallback> m_matchBackgroundFetchCallbacks;
    HashMap<SWClientRequestIdentifier, RetrieveRecordResponseCallback> m_retrieveRecordResponseCallbacks;
    HashMap<SWClientRequestIdentifier, RetrieveRecordResponseBodyCallback> m_retrieveRecordResponseBodyCallbacks;
    HashMap<SWClientRequestIdentifier, ExceptionOrCookieChangeSubscriptionsCallback> m_cookieChangeSubscriptionsCallback;
};

} // namespace WebCore
