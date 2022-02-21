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

#include "ExceptionData.h"
#include "RegistrableDomain.h"
#include "ScriptExecutionContextIdentifier.h"
#include "ServiceWorkerClientQueryOptions.h"
#include "ServiceWorkerContextData.h"
#include "ServiceWorkerIdentifier.h"
#include "ServiceWorkerTypes.h"
#include <wtf/URLHash.h>

namespace WebCore {

class SWServer;
struct ServiceWorkerClientData;
struct ServiceWorkerContextData;
struct ServiceWorkerJobDataIdentifier;
enum class WorkerThreadMode : bool;

class SWServerToContextConnection {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT virtual ~SWServerToContextConnection();

    SWServerToContextConnectionIdentifier identifier() const { return m_identifier; }

    // Messages to the SW host process
    virtual void installServiceWorkerContext(const ServiceWorkerContextData&, const ServiceWorkerData&, const String& userAgent, WorkerThreadMode) = 0;
    virtual void updateAppInitiatedValue(ServiceWorkerIdentifier, LastNavigationWasAppInitiated) = 0;
    virtual void fireInstallEvent(ServiceWorkerIdentifier) = 0;
    virtual void fireActivateEvent(ServiceWorkerIdentifier) = 0;
    virtual void terminateWorker(ServiceWorkerIdentifier) = 0;
    virtual void didSaveScriptsToDisk(ServiceWorkerIdentifier, const ScriptBuffer&, const HashMap<URL, ScriptBuffer>& importedScripts) = 0;
    virtual void matchAllCompleted(uint64_t requestIdentifier, const Vector<ServiceWorkerClientData>&) = 0;
    virtual void firePushEvent(ServiceWorkerIdentifier, const std::optional<Vector<uint8_t>>&, CompletionHandler<void(bool)>&& callback) = 0;

    // Messages back from the SW host process
    WEBCORE_EXPORT void scriptContextFailedToStart(const std::optional<ServiceWorkerJobDataIdentifier>&, ServiceWorkerIdentifier, const String& message);
    WEBCORE_EXPORT void scriptContextStarted(const std::optional<ServiceWorkerJobDataIdentifier>&, ServiceWorkerIdentifier, bool doesHandleFetch);
    WEBCORE_EXPORT void didFinishInstall(const std::optional<ServiceWorkerJobDataIdentifier>&, ServiceWorkerIdentifier, bool wasSuccessful);
    WEBCORE_EXPORT void didFinishActivation(ServiceWorkerIdentifier);
    WEBCORE_EXPORT void setServiceWorkerHasPendingEvents(ServiceWorkerIdentifier, bool hasPendingEvents);
    WEBCORE_EXPORT void skipWaiting(ServiceWorkerIdentifier, CompletionHandler<void()>&&);
    WEBCORE_EXPORT void workerTerminated(ServiceWorkerIdentifier);
    WEBCORE_EXPORT void matchAll(uint64_t requestIdentifier, ServiceWorkerIdentifier, const ServiceWorkerClientQueryOptions&);
    WEBCORE_EXPORT void claim(ServiceWorkerIdentifier, CompletionHandler<void(std::optional<ExceptionData>&&)>&&);
    WEBCORE_EXPORT void setScriptResource(ServiceWorkerIdentifier, URL&& scriptURL, ServiceWorkerContextData::ImportedScript&&);
    WEBCORE_EXPORT void didFailHeartBeatCheck(ServiceWorkerIdentifier);
    WEBCORE_EXPORT void findClientByVisibleIdentifier(ServiceWorkerIdentifier, const String& clientIdentifier, CompletionHandler<void(std::optional<WebCore::ServiceWorkerClientData>&&)>&&);

    const RegistrableDomain& registrableDomain() const { return m_registrableDomain; }
    std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier() const { return m_serviceWorkerPageIdentifier; }

    virtual void connectionIsNoLongerNeeded() = 0;
    virtual void terminateDueToUnresponsiveness() = 0;

protected:
    WEBCORE_EXPORT SWServerToContextConnection(RegistrableDomain&&, std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier);

private:
    SWServerToContextConnectionIdentifier m_identifier;
    RegistrableDomain m_registrableDomain;
    std::optional<ScriptExecutionContextIdentifier> m_serviceWorkerPageIdentifier;
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
