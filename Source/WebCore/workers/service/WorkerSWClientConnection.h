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

#include "SWClientConnection.h"

namespace WebCore {

class WorkerGlobalScope;
class WorkerThread;

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
    void getNotifications(const URL&, const String&, GetNotificationsCallback&&) final;

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

    Ref<WorkerThread> m_thread;

    uint64_t m_lastRequestIdentifier { 0 };
    HashMap<uint64_t, RegistrationCallback> m_matchRegistrationRequests;
    HashMap<uint64_t, GetRegistrationsCallback> m_getRegistrationsRequests;
    HashMap<uint64_t, WhenRegistrationReadyCallback> m_whenRegistrationReadyRequests;
    HashMap<uint64_t, CompletionHandler<void(ExceptionOr<bool>&&)>> m_unregisterRequests;
    HashMap<uint64_t, SubscribeToPushServiceCallback> m_subscribeToPushServiceRequests;
    HashMap<uint64_t, UnsubscribeFromPushServiceCallback> m_unsubscribeFromPushServiceRequests;
    HashMap<uint64_t, GetPushSubscriptionCallback> m_getPushSubscriptionRequests;
    HashMap<uint64_t, GetPushPermissionStateCallback> m_getPushPermissionStateCallbacks;
    HashMap<uint64_t, ExceptionOrVoidCallback> m_voidCallbacks;
    HashMap<uint64_t, ExceptionOrNavigationPreloadStateCallback> m_navigationPreloadStateCallbacks;
    HashMap<uint64_t, GetNotificationsCallback> m_getNotificationsCallbacks;
    HashMap<uint64_t, ExceptionOrBackgroundFetchInformationCallback> m_backgroundFetchInformationCallbacks;
    HashMap<uint64_t, BackgroundFetchIdentifiersCallback> m_backgroundFetchIdentifiersCallbacks;
    HashMap<uint64_t, AbortBackgroundFetchCallback> m_abortBackgroundFetchCallbacks;
    HashMap<uint64_t, MatchBackgroundFetchCallback> m_matchBackgroundFetchCallbacks;
    HashMap<uint64_t, RetrieveRecordResponseCallback> m_retrieveRecordResponseCallbacks;
    HashMap<uint64_t, RetrieveRecordResponseBodyCallback> m_retrieveRecordResponseBodyCallbacks;
};

} // namespace WebCore
