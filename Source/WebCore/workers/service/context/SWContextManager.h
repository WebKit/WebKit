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
#include "ServiceWorkerClientInformation.h"
#include "ServiceWorkerClientQueryOptions.h"
#include "ServiceWorkerIdentifier.h"
#include "ServiceWorkerThreadProxy.h"
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>

namespace WebCore {

class SerializedScriptValue;
class ServiceWorkerGlobalScope;

class SWContextManager {
public:
    WEBCORE_EXPORT static SWContextManager& singleton();

    class Connection {
    public:
        virtual ~Connection() { }

        virtual void postMessageToServiceWorkerClient(const ServiceWorkerClientIdentifier& destinationIdentifier, Ref<SerializedScriptValue>&& message, ServiceWorkerIdentifier source, const String& sourceOrigin) = 0;
        virtual void serviceWorkerStartedWithMessage(std::optional<ServiceWorkerJobDataIdentifier>, ServiceWorkerIdentifier, const String& exceptionMessage) = 0;
        virtual void didFinishInstall(std::optional<ServiceWorkerJobDataIdentifier>, ServiceWorkerIdentifier, bool wasSuccessful) = 0;
        virtual void didFinishActivation(ServiceWorkerIdentifier) = 0;
        virtual void setServiceWorkerHasPendingEvents(ServiceWorkerIdentifier, bool) = 0;
        virtual void workerTerminated(ServiceWorkerIdentifier) = 0;
        virtual void skipWaiting(ServiceWorkerIdentifier, WTF::Function<void()>&& callback) = 0;

        using FindClientByIdentifierCallback = WTF::CompletionHandler<void(ExceptionOr<std::optional<ServiceWorkerClientData>>&&)>;
        virtual void findClientByIdentifier(ServiceWorkerIdentifier, ServiceWorkerClientIdentifier, FindClientByIdentifierCallback&&) = 0;
        virtual void matchAll(ServiceWorkerIdentifier, const ServiceWorkerClientQueryOptions&, ServiceWorkerClientsMatchAllCallback&&) = 0;
    };

    WEBCORE_EXPORT void setConnection(std::unique_ptr<Connection>&&);
    WEBCORE_EXPORT Connection* connection() const;

    WEBCORE_EXPORT void registerServiceWorkerThreadForInstall(Ref<ServiceWorkerThreadProxy>&&);
    WEBCORE_EXPORT ServiceWorkerThreadProxy* serviceWorkerThreadProxy(ServiceWorkerIdentifier) const;
    WEBCORE_EXPORT void postMessageToServiceWorker(ServiceWorkerIdentifier destination, Ref<SerializedScriptValue>&& message, ServiceWorkerClientIdentifier sourceIdentifier, ServiceWorkerClientData&& sourceData);
    WEBCORE_EXPORT void postMessageToServiceWorker(ServiceWorkerIdentifier destination, Ref<SerializedScriptValue>&& message, ServiceWorkerData&& sourceData);
    WEBCORE_EXPORT void fireInstallEvent(ServiceWorkerIdentifier);
    WEBCORE_EXPORT void fireActivateEvent(ServiceWorkerIdentifier);
    WEBCORE_EXPORT void terminateWorker(ServiceWorkerIdentifier, Function<void()>&&);

    void forEachServiceWorkerThread(const WTF::Function<void(ServiceWorkerThreadProxy&)>&);

    WEBCORE_EXPORT void postTaskToServiceWorker(ServiceWorkerIdentifier, WTF::Function<void(ServiceWorkerGlobalScope&)>&&);

private:
    SWContextManager() = default;

    HashMap<ServiceWorkerIdentifier, RefPtr<ServiceWorkerThreadProxy>> m_workerMap;
    std::unique_ptr<Connection> m_connection;
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
