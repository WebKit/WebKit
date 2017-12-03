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
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class ResourceError;
class SecurityOrigin;
class SerializedScriptValue;
class ServiceWorkerContainer;
class ServiceWorkerRegistration;
class SharedBuffer;
enum class ServiceWorkerRegistrationState;
enum class ServiceWorkerState;
enum class ShouldNotifyWhenResolved;
struct ExceptionData;
struct ServiceWorkerClientData;
struct ServiceWorkerData;
struct ServiceWorkerFetchResult;
struct ServiceWorkerRegistrationData;

class SWClientConnection : public ThreadSafeRefCounted<SWClientConnection> {
public:
    WEBCORE_EXPORT virtual ~SWClientConnection();

    using RegistrationCallback = WTF::CompletionHandler<void(std::optional<ServiceWorkerRegistrationData>&&)>;
    virtual void matchRegistration(const SecurityOrigin& topOrigin, const URL& clientURL, RegistrationCallback&&) = 0;

    using GetRegistrationsCallback = WTF::CompletionHandler<void(Vector<ServiceWorkerRegistrationData>&&)>;
    virtual void getRegistrations(const SecurityOrigin& topOrigin, const URL& clientURL, GetRegistrationsCallback&&) = 0;

    virtual void addServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier) = 0;
    virtual void removeServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier) = 0;

    void scheduleJob(ServiceWorkerJob&);
    void finishedFetchingScript(ServiceWorkerJob&, const String&);
    void failedFetchingScript(ServiceWorkerJob&, const ResourceError&);

    virtual void didResolveRegistrationPromise(const ServiceWorkerRegistrationKey&) = 0;

    virtual void postMessageToServiceWorkerGlobalScope(ServiceWorkerIdentifier destinationIdentifier, Ref<SerializedScriptValue>&&, DocumentIdentifier sourceContextIdentifier, ServiceWorkerClientData&& source) = 0;
    virtual SWServerConnectionIdentifier serverConnectionIdentifier() const = 0;
    virtual bool mayHaveServiceWorkerRegisteredForOrigin(const SecurityOrigin&) const = 0;
    virtual void syncTerminateWorker(ServiceWorkerIdentifier) = 0;

    virtual void serviceWorkerStartedControllingClient(ServiceWorkerIdentifier, ServiceWorkerRegistrationIdentifier, DocumentIdentifier) = 0;
    virtual void serviceWorkerStoppedControllingClient(ServiceWorkerIdentifier, ServiceWorkerRegistrationIdentifier, DocumentIdentifier) = 0;

    virtual void registerServiceWorkerClient(const SecurityOrigin& topOrigin, DocumentIdentifier, const ServiceWorkerClientData&) = 0;
    virtual void unregisterServiceWorkerClient(DocumentIdentifier) = 0;

protected:
    WEBCORE_EXPORT SWClientConnection();

    WEBCORE_EXPORT void jobRejectedInServer(const ServiceWorkerJobDataIdentifier&, const ExceptionData&);
    WEBCORE_EXPORT void registrationJobResolvedInServer(const ServiceWorkerJobDataIdentifier&, ServiceWorkerRegistrationData&&, ShouldNotifyWhenResolved);
    WEBCORE_EXPORT void unregistrationJobResolvedInServer(const ServiceWorkerJobDataIdentifier&, bool unregistrationResult);
    WEBCORE_EXPORT void startScriptFetchForServer(const ServiceWorkerJobDataIdentifier&);
    WEBCORE_EXPORT void postMessageToServiceWorkerClient(DocumentIdentifier destinationContextIdentifier, Ref<SerializedScriptValue>&& message, ServiceWorkerData&& source, const String& sourceOrigin);
    WEBCORE_EXPORT void updateRegistrationState(ServiceWorkerRegistrationIdentifier, ServiceWorkerRegistrationState, const std::optional<ServiceWorkerData>&);
    WEBCORE_EXPORT void updateWorkerState(ServiceWorkerIdentifier, ServiceWorkerState);
    WEBCORE_EXPORT void fireUpdateFoundEvent(ServiceWorkerRegistrationIdentifier);
    WEBCORE_EXPORT void notifyClientsOfControllerChange(const HashSet<DocumentIdentifier>& contextIdentifiers, ServiceWorkerData&& newController);

    WEBCORE_EXPORT void clearPendingJobs();
    WEBCORE_EXPORT void postTaskTo(const DocumentOrWorkerIdentifier&, WTF::Function<void()>&&);

private:
    virtual void scheduleJobInServer(const ServiceWorkerJobData&) = 0;
    virtual void finishFetchingScriptInServer(const ServiceWorkerFetchResult&) = 0;

    HashMap<ServiceWorkerJobIdentifier, RefPtr<ServiceWorkerJob>> m_scheduledJobs;
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
