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

#include "ServiceWorkerIdentifier.h"
#include "ServiceWorkerTypes.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class SWServer;
struct ServiceWorkerClientData;
struct ServiceWorkerClientIdentifier;
struct ServiceWorkerContextData;
struct ServiceWorkerJobDataIdentifier;

class SWServerToContextConnection : public RefCounted<SWServerToContextConnection> {
public:
    WEBCORE_EXPORT virtual ~SWServerToContextConnection();

    SWServerToContextConnectionIdentifier identifier() const { return m_identifier; }

    // Messages to the SW host process
    virtual void installServiceWorkerContext(const ServiceWorkerContextData&) = 0;
    virtual void fireInstallEvent(ServiceWorkerIdentifier) = 0;
    virtual void fireActivateEvent(ServiceWorkerIdentifier) = 0;
    virtual void terminateWorker(ServiceWorkerIdentifier) = 0;
    virtual void syncTerminateWorker(ServiceWorkerIdentifier) = 0;
    virtual void findClientByIdentifierCompleted(uint64_t requestIdentifier, const std::optional<ServiceWorkerClientData>&, bool hasSecurityError) = 0;

    // Messages back from the SW host process
    WEBCORE_EXPORT void scriptContextFailedToStart(const std::optional<ServiceWorkerJobDataIdentifier>&, ServiceWorkerIdentifier, const String& message);
    WEBCORE_EXPORT void scriptContextStarted(const std::optional<ServiceWorkerJobDataIdentifier>&, ServiceWorkerIdentifier);
    WEBCORE_EXPORT void didFinishInstall(const std::optional<ServiceWorkerJobDataIdentifier>&, ServiceWorkerIdentifier, bool wasSuccessful);
    WEBCORE_EXPORT void didFinishActivation(ServiceWorkerIdentifier);
    WEBCORE_EXPORT void setServiceWorkerHasPendingEvents(ServiceWorkerIdentifier, bool hasPendingEvents);
    WEBCORE_EXPORT void workerTerminated(ServiceWorkerIdentifier);
    WEBCORE_EXPORT void findClientByIdentifier(uint64_t clientIdRequestIdentifier, ServiceWorkerIdentifier, ServiceWorkerClientIdentifier);

    static SWServerToContextConnection* connectionForIdentifier(SWServerToContextConnectionIdentifier);

    // FIXME: While we only ever have one SW context process this method makes sense.
    // Once we have multiple ones this method should go away forcing use of connectionForIdentifier()
    WEBCORE_EXPORT static SWServerToContextConnection* globalServerToContextConnection();

protected:
    WEBCORE_EXPORT SWServerToContextConnection();

private:
    SWServerToContextConnectionIdentifier m_identifier;
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
