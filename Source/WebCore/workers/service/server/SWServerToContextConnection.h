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

#include "BackgroundFetchFailureReason.h"
#include "ExceptionData.h"
#include "NotificationEventType.h"
#include "PageIdentifier.h"
#include "RegistrableDomain.h"
#include "ScriptExecutionContextIdentifier.h"
#include "ServiceWorkerClientQueryOptions.h"
#include "ServiceWorkerContextData.h"
#include "ServiceWorkerIdentifier.h"
#include "ServiceWorkerTypes.h"
#include <wtf/CheckedPtr.h>
#include <wtf/URLHash.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

struct BackgroundFetchInformation;
struct NotificationData;
struct NotificationPayload;
class SWServer;
struct ServiceWorkerClientData;
struct ServiceWorkerContextData;
struct ServiceWorkerJobDataIdentifier;
enum class WorkerThreadMode : bool;

class SWServerToContextConnection: public CanMakeWeakPtr<SWServerToContextConnection>, public CanMakeCheckedPtr {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT virtual ~SWServerToContextConnection();

    WEBCORE_EXPORT SWServer* server() const;
    WEBCORE_EXPORT RefPtr<SWServer> protectedServer() const;

    SWServerToContextConnectionIdentifier identifier() const { return m_identifier; }

    // This flag gets set when the service worker process is no longer clean (because it has loaded several eTLD+1s).
    bool shouldTerminateWhenPossible() const { return m_shouldTerminateWhenPossible; }
    void terminateWhenPossible();

    // Messages to the SW host process
    virtual void installServiceWorkerContext(const ServiceWorkerContextData&, const ServiceWorkerData&, const String& userAgent, WorkerThreadMode) = 0;
    virtual void updateAppInitiatedValue(ServiceWorkerIdentifier, LastNavigationWasAppInitiated) = 0;
    virtual void fireInstallEvent(ServiceWorkerIdentifier) = 0;
    virtual void fireActivateEvent(ServiceWorkerIdentifier) = 0;
    virtual void terminateWorker(ServiceWorkerIdentifier) = 0;
    virtual void didSaveScriptsToDisk(ServiceWorkerIdentifier, const ScriptBuffer&, const MemoryCompactRobinHoodHashMap<URL, ScriptBuffer>& importedScripts) = 0;
    virtual void matchAllCompleted(uint64_t requestIdentifier, const Vector<ServiceWorkerClientData>&) = 0;
    virtual void firePushEvent(ServiceWorkerIdentifier, const std::optional<Vector<uint8_t>>&, std::optional<NotificationPayload>&&, CompletionHandler<void(bool, std::optional<NotificationPayload>&&)>&&) = 0;
    virtual void fireNotificationEvent(ServiceWorkerIdentifier, const NotificationData&, NotificationEventType, CompletionHandler<void(bool)>&&) = 0;
    virtual void fireBackgroundFetchEvent(ServiceWorkerIdentifier, const BackgroundFetchInformation&, CompletionHandler<void(bool)>&&) = 0;
    virtual void fireBackgroundFetchClickEvent(ServiceWorkerIdentifier, const BackgroundFetchInformation&, CompletionHandler<void(bool)>&&) = 0;
    virtual ProcessIdentifier webProcessIdentifier() const = 0;

    // Messages back from the SW host process
    WEBCORE_EXPORT void scriptContextFailedToStart(const std::optional<ServiceWorkerJobDataIdentifier>&, ServiceWorkerIdentifier, const String& message);
    WEBCORE_EXPORT void scriptContextStarted(const std::optional<ServiceWorkerJobDataIdentifier>&, ServiceWorkerIdentifier, bool doesHandleFetch);
    WEBCORE_EXPORT void didFinishInstall(const std::optional<ServiceWorkerJobDataIdentifier>&, ServiceWorkerIdentifier, bool wasSuccessful);
    WEBCORE_EXPORT void didFinishActivation(ServiceWorkerIdentifier);
    WEBCORE_EXPORT void setServiceWorkerHasPendingEvents(ServiceWorkerIdentifier, bool hasPendingEvents);
    WEBCORE_EXPORT void workerTerminated(ServiceWorkerIdentifier);
    WEBCORE_EXPORT void matchAll(uint64_t requestIdentifier, ServiceWorkerIdentifier, const ServiceWorkerClientQueryOptions&);
    WEBCORE_EXPORT void claim(ServiceWorkerIdentifier, CompletionHandler<void(std::optional<ExceptionData>&&)>&&);
    WEBCORE_EXPORT void setScriptResource(ServiceWorkerIdentifier, URL&& scriptURL, ServiceWorkerContextData::ImportedScript&&);
    WEBCORE_EXPORT void didFailHeartBeatCheck(ServiceWorkerIdentifier);
    WEBCORE_EXPORT void setAsInspected(ServiceWorkerIdentifier, bool);
    WEBCORE_EXPORT void findClientByVisibleIdentifier(ServiceWorkerIdentifier, const String& clientIdentifier, CompletionHandler<void(std::optional<WebCore::ServiceWorkerClientData>&&)>&&);

    using OpenWindowCallback = CompletionHandler<void(Expected<std::optional<ServiceWorkerClientData>, ExceptionData>&&)>;
    virtual void openWindow(ServiceWorkerIdentifier, const URL&, OpenWindowCallback&&) = 0;

    const RegistrableDomain& registrableDomain() const { return m_registrableDomain; }
    std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier() const { return m_serviceWorkerPageIdentifier; }

    virtual void connectionIsNoLongerNeeded() = 0;
    virtual void terminateDueToUnresponsiveness() = 0;

    virtual void setInspectable(ServiceWorkerIsInspectable) = 0;

protected:
    WEBCORE_EXPORT SWServerToContextConnection(SWServer&, RegistrableDomain&&, std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier);

    virtual void close() = 0;

private:
    WeakPtr<WebCore::SWServer> m_server;
    SWServerToContextConnectionIdentifier m_identifier;
    RegistrableDomain m_registrableDomain;
    std::optional<ScriptExecutionContextIdentifier> m_serviceWorkerPageIdentifier;
    bool m_shouldTerminateWhenPossible { false };
};

} // namespace WebCore
