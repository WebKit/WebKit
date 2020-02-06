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

#include "DocumentIdentifier.h"
#include "ServiceWorkerJob.h"
#include "ServiceWorkerTypes.h"
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class ResourceError;
class SecurityOrigin;
class SerializedScriptValue;
class ServiceWorkerContainer;
class ServiceWorkerRegistration;
class SharedBuffer;
enum class ServiceWorkerRegistrationState : uint8_t;
enum class ServiceWorkerState : uint8_t;
enum class ShouldNotifyWhenResolved : bool;
struct ExceptionData;
struct MessageWithMessagePorts;
struct ServiceWorkerClientData;
struct ServiceWorkerClientIdentifier;
struct ServiceWorkerData;
struct ServiceWorkerFetchResult;
struct ServiceWorkerRegistrationData;

class SWClientConnection : public RefCounted<SWClientConnection> {
public:
    WEBCORE_EXPORT virtual ~SWClientConnection();

    using RegistrationCallback = CompletionHandler<void(Optional<ServiceWorkerRegistrationData>&&)>;
    virtual void matchRegistration(SecurityOriginData&& topOrigin, const URL& clientURL, RegistrationCallback&&) = 0;

    using GetRegistrationsCallback = CompletionHandler<void(Vector<ServiceWorkerRegistrationData>&&)>;
    virtual void getRegistrations(SecurityOriginData&& topOrigin, const URL& clientURL, GetRegistrationsCallback&&) = 0;

    using WhenRegistrationReadyCallback = Function<void(ServiceWorkerRegistrationData&&)>;
    virtual void whenRegistrationReady(const SecurityOriginData& topOrigin, const URL& clientURL, WhenRegistrationReadyCallback&&) = 0;

    virtual void addServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier) = 0;
    virtual void removeServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier) = 0;

    WEBCORE_EXPORT virtual void scheduleJob(DocumentOrWorkerIdentifier, const ServiceWorkerJobData&);

    virtual void didResolveRegistrationPromise(const ServiceWorkerRegistrationKey&) = 0;

    virtual void postMessageToServiceWorker(ServiceWorkerIdentifier destination, MessageWithMessagePorts&&, const ServiceWorkerOrClientIdentifier& source) = 0;

    virtual SWServerConnectionIdentifier serverConnectionIdentifier() const = 0;
    virtual bool mayHaveServiceWorkerRegisteredForOrigin(const SecurityOriginData&) const = 0;
    virtual void syncTerminateWorker(ServiceWorkerIdentifier) = 0;

    virtual void registerServiceWorkerClient(const SecurityOrigin& topOrigin, const ServiceWorkerClientData&, const Optional<ServiceWorkerRegistrationIdentifier>&, const String& userAgent) = 0;
    virtual void unregisterServiceWorkerClient(DocumentIdentifier) = 0;

    virtual void finishFetchingScriptInServer(const ServiceWorkerFetchResult&) = 0;

    virtual void storeRegistrationsOnDiskForTesting(CompletionHandler<void()>&& callback) { callback(); }
    virtual void isServiceWorkerRunning(ServiceWorkerIdentifier, CompletionHandler<void(bool)>&& callback) { callback(false); }

    WEBCORE_EXPORT void registerServiceWorkerClients();

protected:
    WEBCORE_EXPORT SWClientConnection();

    WEBCORE_EXPORT void jobRejectedInServer(ServiceWorkerJobIdentifier, const ExceptionData&);
    WEBCORE_EXPORT void registrationJobResolvedInServer(ServiceWorkerJobIdentifier, ServiceWorkerRegistrationData&&, ShouldNotifyWhenResolved);
    WEBCORE_EXPORT void unregistrationJobResolvedInServer(ServiceWorkerJobIdentifier, bool unregistrationResult);
    WEBCORE_EXPORT void startScriptFetchForServer(ServiceWorkerJobIdentifier, const ServiceWorkerRegistrationKey&, FetchOptions::Cache);
    WEBCORE_EXPORT void postMessageToServiceWorkerClient(DocumentIdentifier destinationContextIdentifier, MessageWithMessagePorts&&, ServiceWorkerData&& source, String&& sourceOrigin);
    WEBCORE_EXPORT void updateRegistrationState(ServiceWorkerRegistrationIdentifier, ServiceWorkerRegistrationState, const Optional<ServiceWorkerData>&);
    WEBCORE_EXPORT void updateWorkerState(ServiceWorkerIdentifier, ServiceWorkerState);
    WEBCORE_EXPORT void fireUpdateFoundEvent(ServiceWorkerRegistrationIdentifier);
    WEBCORE_EXPORT void setRegistrationLastUpdateTime(ServiceWorkerRegistrationIdentifier, WallTime);
    WEBCORE_EXPORT void setRegistrationUpdateViaCache(ServiceWorkerRegistrationIdentifier, ServiceWorkerUpdateViaCache);
    WEBCORE_EXPORT void notifyClientsOfControllerChange(const HashSet<DocumentIdentifier>& contextIdentifiers, ServiceWorkerData&& newController);

    WEBCORE_EXPORT void clearPendingJobs();

private:
    virtual void scheduleJobInServer(const ServiceWorkerJobData&) = 0;

    enum class IsJobComplete { No, Yes };
    bool postTaskForJob(ServiceWorkerJobIdentifier, IsJobComplete, Function<void(ServiceWorkerJob&)>&&);

    HashMap<ServiceWorkerJobIdentifier, DocumentOrWorkerIdentifier> m_scheduledJobSources;
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
