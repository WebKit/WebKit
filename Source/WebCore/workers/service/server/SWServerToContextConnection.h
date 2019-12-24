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

#include "RegistrableDomain.h"
#include "ServiceWorkerClientQueryOptions.h"
#include "ServiceWorkerContextData.h"
#include "ServiceWorkerIdentifier.h"
#include "ServiceWorkerTypes.h"

namespace WebCore {

class SWServer;
struct ServiceWorkerClientData;
struct ServiceWorkerClientIdentifier;
struct ServiceWorkerContextData;
struct ServiceWorkerJobDataIdentifier;

class SWServerToContextConnection {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT virtual ~SWServerToContextConnection();

    SWServerToContextConnectionIdentifier identifier() const { return m_identifier; }

    // Messages to the SW host process
    virtual void installServiceWorkerContext(const ServiceWorkerContextData&, const String& userAgent) = 0;
    virtual void fireInstallEvent(ServiceWorkerIdentifier) = 0;
    virtual void fireActivateEvent(ServiceWorkerIdentifier) = 0;
    virtual void terminateWorker(ServiceWorkerIdentifier) = 0;
    virtual void syncTerminateWorker(ServiceWorkerIdentifier) = 0;
    virtual void findClientByIdentifierCompleted(uint64_t requestIdentifier, const Optional<ServiceWorkerClientData>&, bool hasSecurityError) = 0;
    virtual void matchAllCompleted(uint64_t requestIdentifier, const Vector<ServiceWorkerClientData>&) = 0;
    virtual void claimCompleted(uint64_t requestIdentifier) = 0;

    // Messages back from the SW host process
    WEBCORE_EXPORT void scriptContextFailedToStart(const Optional<ServiceWorkerJobDataIdentifier>&, ServiceWorkerIdentifier, const String& message);
    WEBCORE_EXPORT void scriptContextStarted(const Optional<ServiceWorkerJobDataIdentifier>&, ServiceWorkerIdentifier, bool doesHandleFetch);
    WEBCORE_EXPORT void didFinishInstall(const Optional<ServiceWorkerJobDataIdentifier>&, ServiceWorkerIdentifier, bool wasSuccessful);
    WEBCORE_EXPORT void didFinishActivation(ServiceWorkerIdentifier);
    WEBCORE_EXPORT void setServiceWorkerHasPendingEvents(ServiceWorkerIdentifier, bool hasPendingEvents);
    WEBCORE_EXPORT void skipWaiting(ServiceWorkerIdentifier, CompletionHandler<void()>&&);
    WEBCORE_EXPORT void workerTerminated(ServiceWorkerIdentifier);
    WEBCORE_EXPORT void findClientByIdentifier(uint64_t clientIdRequestIdentifier, ServiceWorkerIdentifier, ServiceWorkerClientIdentifier);
    WEBCORE_EXPORT void matchAll(uint64_t requestIdentifier, ServiceWorkerIdentifier, const ServiceWorkerClientQueryOptions&);
    WEBCORE_EXPORT void claim(uint64_t requestIdentifier, ServiceWorkerIdentifier);
    WEBCORE_EXPORT void setScriptResource(ServiceWorkerIdentifier, URL&& scriptURL, String&& script, URL&& responseURL, String&& mimeType);
    WEBCORE_EXPORT void didFailHeartBeatCheck(ServiceWorkerIdentifier);

    const RegistrableDomain& registrableDomain() const { return m_registrableDomain; }

    virtual void connectionIsNoLongerNeeded() = 0;

protected:
    WEBCORE_EXPORT explicit SWServerToContextConnection(RegistrableDomain&&);

private:
    SWServerToContextConnectionIdentifier m_identifier;
    RegistrableDomain m_registrableDomain;
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
