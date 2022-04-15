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

#include "ExceptionOr.h"
#include "NavigationPreloadState.h"
#include "NotificationData.h"
#include "PushPermissionState.h"
#include "PushSubscriptionData.h"
#include "ScriptExecutionContextIdentifier.h"
#include "ServiceWorkerJob.h"
#include "ServiceWorkerTypes.h"
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class ResourceError;
class SecurityOrigin;
class ScriptExecutionContext;
class SerializedScriptValue;
class ServiceWorkerContainer;
class ServiceWorkerRegistration;
enum class ServiceWorkerRegistrationState : uint8_t;
enum class ServiceWorkerState : uint8_t;
enum class ShouldNotifyWhenResolved : bool;
struct ClientOrigin;
struct ExceptionData;
struct MessageWithMessagePorts;
struct NotificationData;
struct ServiceWorkerClientData;
struct ServiceWorkerData;
struct ServiceWorkerRegistrationData;
struct WorkerFetchResult;

class SWClientConnection : public RefCounted<SWClientConnection> {
public:
    WEBCORE_EXPORT virtual ~SWClientConnection();

    using RegistrationCallback = CompletionHandler<void(std::optional<ServiceWorkerRegistrationData>&&)>;
    virtual void matchRegistration(SecurityOriginData&& topOrigin, const URL& clientURL, RegistrationCallback&&) = 0;

    using GetRegistrationsCallback = CompletionHandler<void(Vector<ServiceWorkerRegistrationData>&&)>;
    virtual void getRegistrations(SecurityOriginData&& topOrigin, const URL& clientURL, GetRegistrationsCallback&&) = 0;

    using WhenRegistrationReadyCallback = Function<void(ServiceWorkerRegistrationData&&)>;
    virtual void whenRegistrationReady(const SecurityOriginData& topOrigin, const URL& clientURL, WhenRegistrationReadyCallback&&) = 0;

    virtual void addServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier) = 0;
    virtual void removeServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier) = 0;
    virtual void scheduleUnregisterJobInServer(ServiceWorkerRegistrationIdentifier, ServiceWorkerOrClientIdentifier, CompletionHandler<void(ExceptionOr<bool>&&)>&&) = 0;

    WEBCORE_EXPORT virtual void scheduleJob(ServiceWorkerOrClientIdentifier, const ServiceWorkerJobData&);

    virtual void didResolveRegistrationPromise(const ServiceWorkerRegistrationKey&) = 0;

    virtual void postMessageToServiceWorker(ServiceWorkerIdentifier destination, MessageWithMessagePorts&&, const ServiceWorkerOrClientIdentifier& source) = 0;

    virtual SWServerConnectionIdentifier serverConnectionIdentifier() const = 0;
    virtual bool mayHaveServiceWorkerRegisteredForOrigin(const SecurityOriginData&) const = 0;

    virtual void registerServiceWorkerClient(const ClientOrigin&, ServiceWorkerClientData&&, const std::optional<ServiceWorkerRegistrationIdentifier>&, String&& userAgent) = 0;
    virtual void unregisterServiceWorkerClient(ScriptExecutionContextIdentifier) = 0;

    virtual void finishFetchingScriptInServer(const ServiceWorkerJobDataIdentifier&, ServiceWorkerRegistrationKey&&, WorkerFetchResult&&) = 0;

    virtual void storeRegistrationsOnDiskForTesting(CompletionHandler<void()>&& callback) { callback(); }
    virtual void whenServiceWorkerIsTerminatedForTesting(ServiceWorkerIdentifier, CompletionHandler<void()>&& callback) { callback(); }
    
    using SubscribeToPushServiceCallback = CompletionHandler<void(ExceptionOr<PushSubscriptionData>&&)>;
    virtual void subscribeToPushService(ServiceWorkerRegistrationIdentifier, const Vector<uint8_t>& applicationServerKey, SubscribeToPushServiceCallback&&) = 0;
    
    using UnsubscribeFromPushServiceCallback = CompletionHandler<void(ExceptionOr<bool>&&)>;
    virtual void unsubscribeFromPushService(ServiceWorkerRegistrationIdentifier, PushSubscriptionIdentifier, UnsubscribeFromPushServiceCallback&&) = 0;
    
    using GetPushSubscriptionCallback = CompletionHandler<void(ExceptionOr<std::optional<PushSubscriptionData>>&&)>;
    virtual void getPushSubscription(ServiceWorkerRegistrationIdentifier, GetPushSubscriptionCallback&&) = 0;

    using GetPushPermissionStateCallback = CompletionHandler<void(ExceptionOr<PushPermissionState>&&)>;
    virtual void getPushPermissionState(ServiceWorkerRegistrationIdentifier, GetPushPermissionStateCallback&&) = 0;

    using GetNotificationsCallback = CompletionHandler<void(ExceptionOr<Vector<NotificationData>>&&)>;
    virtual void getNotifications(const URL&, const String&, GetNotificationsCallback&&) = 0;

    using ExceptionOrVoidCallback = CompletionHandler<void(ExceptionOr<void>&&)>;
    virtual void enableNavigationPreload(ServiceWorkerRegistrationIdentifier, ExceptionOrVoidCallback&&) = 0;
    virtual void disableNavigationPreload(ServiceWorkerRegistrationIdentifier, ExceptionOrVoidCallback&&) = 0;
    virtual void setNavigationPreloadHeaderValue(ServiceWorkerRegistrationIdentifier, String&&, ExceptionOrVoidCallback&&) = 0;
    using ExceptionOrNavigationPreloadStateCallback = CompletionHandler<void(ExceptionOr<NavigationPreloadState>&&)>;
    virtual void getNavigationPreloadState(ServiceWorkerRegistrationIdentifier, ExceptionOrNavigationPreloadStateCallback&&) = 0;

    WEBCORE_EXPORT void registerServiceWorkerClients();
    bool isClosed() const { return m_isClosed; }

protected:
    WEBCORE_EXPORT SWClientConnection();

    WEBCORE_EXPORT void jobRejectedInServer(ServiceWorkerJobIdentifier, ExceptionData&&);
    WEBCORE_EXPORT void registrationJobResolvedInServer(ServiceWorkerJobIdentifier, ServiceWorkerRegistrationData&&, ShouldNotifyWhenResolved);
    WEBCORE_EXPORT void startScriptFetchForServer(ServiceWorkerJobIdentifier, ServiceWorkerRegistrationKey&&, FetchOptions::Cache);
    WEBCORE_EXPORT void postMessageToServiceWorkerClient(ScriptExecutionContextIdentifier destinationContextIdentifier, MessageWithMessagePorts&&, ServiceWorkerData&& source, String&& sourceOrigin);
    WEBCORE_EXPORT void updateRegistrationState(ServiceWorkerRegistrationIdentifier, ServiceWorkerRegistrationState, const std::optional<ServiceWorkerData>&);
    WEBCORE_EXPORT void updateWorkerState(ServiceWorkerIdentifier, ServiceWorkerState);
    WEBCORE_EXPORT void fireUpdateFoundEvent(ServiceWorkerRegistrationIdentifier);
    WEBCORE_EXPORT void setRegistrationLastUpdateTime(ServiceWorkerRegistrationIdentifier, WallTime);
    WEBCORE_EXPORT void setRegistrationUpdateViaCache(ServiceWorkerRegistrationIdentifier, ServiceWorkerUpdateViaCache);
    WEBCORE_EXPORT void notifyClientsOfControllerChange(const HashSet<ScriptExecutionContextIdentifier>& contextIdentifiers, ServiceWorkerData&& newController);

    WEBCORE_EXPORT void clearPendingJobs();
    void setIsClosed() { m_isClosed = true; }

private:
    virtual void scheduleJobInServer(const ServiceWorkerJobData&) = 0;

    enum class IsJobComplete { No, Yes };
    bool postTaskForJob(ServiceWorkerJobIdentifier, IsJobComplete, Function<void(ServiceWorkerJob&)>&&);

    bool m_isClosed { false };
    HashMap<ServiceWorkerJobIdentifier, ServiceWorkerOrClientIdentifier> m_scheduledJobSources;
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
