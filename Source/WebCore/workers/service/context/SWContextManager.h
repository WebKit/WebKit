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
#include "ServiceWorkerClientData.h"
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
        WTF_MAKE_FAST_ALLOCATED;
    public:
        virtual ~Connection() { }

        virtual void establishConnection(CompletionHandler<void()>&&) = 0;
        virtual void postMessageToServiceWorkerClient(const ServiceWorkerClientIdentifier& destinationIdentifier, const MessageWithMessagePorts&, ServiceWorkerIdentifier source, const String& sourceOrigin) = 0;
        virtual void serviceWorkerStarted(Optional<ServiceWorkerJobDataIdentifier>, ServiceWorkerIdentifier, bool doesHandleFetch) = 0;
        virtual void serviceWorkerFailedToStart(Optional<ServiceWorkerJobDataIdentifier>, ServiceWorkerIdentifier, const String& message) = 0;
        virtual void didFinishInstall(Optional<ServiceWorkerJobDataIdentifier>, ServiceWorkerIdentifier, bool wasSuccessful) = 0;
        virtual void didFinishActivation(ServiceWorkerIdentifier) = 0;
        virtual void setServiceWorkerHasPendingEvents(ServiceWorkerIdentifier, bool) = 0;
        virtual void workerTerminated(ServiceWorkerIdentifier) = 0;
        virtual void skipWaiting(ServiceWorkerIdentifier, CompletionHandler<void()>&&) = 0;
        virtual void setScriptResource(ServiceWorkerIdentifier, const URL&, const ServiceWorkerContextData::ImportedScript&) = 0;

        using FindClientByIdentifierCallback = CompletionHandler<void(ExceptionOr<Optional<ServiceWorkerClientData>>&&)>;
        virtual void findClientByIdentifier(ServiceWorkerIdentifier, ServiceWorkerClientIdentifier, FindClientByIdentifierCallback&&) = 0;
        virtual void matchAll(ServiceWorkerIdentifier, const ServiceWorkerClientQueryOptions&, ServiceWorkerClientsMatchAllCallback&&) = 0;
        virtual void claim(ServiceWorkerIdentifier, CompletionHandler<void()>&&) = 0;

        virtual void didFailHeartBeatCheck(ServiceWorkerIdentifier) = 0;

        virtual bool isThrottleable() const = 0;

        bool isClosed() const { return m_isClosed; }
        bool shouldUseShortTimeout() const { return m_shouldUseShortTimeout; }

    protected:
        void setAsClosed() { m_isClosed = true; }
        void setShouldUseShortTimeout(bool value) { m_shouldUseShortTimeout = value; }

    private:
        bool m_isClosed { false };
        bool m_shouldUseShortTimeout { false };
    };

    WEBCORE_EXPORT void setConnection(std::unique_ptr<Connection>&&);
    WEBCORE_EXPORT Connection* connection() const;

    WEBCORE_EXPORT void registerServiceWorkerThreadForInstall(Ref<ServiceWorkerThreadProxy>&&);
    WEBCORE_EXPORT ServiceWorkerThreadProxy* serviceWorkerThreadProxy(ServiceWorkerIdentifier) const;
    WEBCORE_EXPORT void postMessageToServiceWorker(ServiceWorkerIdentifier destination, MessageWithMessagePorts&&, ServiceWorkerOrClientData&& sourceData);
    WEBCORE_EXPORT void fireInstallEvent(ServiceWorkerIdentifier);
    WEBCORE_EXPORT void fireActivateEvent(ServiceWorkerIdentifier);
    WEBCORE_EXPORT void terminateWorker(ServiceWorkerIdentifier, Seconds timeout, Function<void()>&&);

    void forEachServiceWorkerThread(const WTF::Function<void(ServiceWorkerThreadProxy&)>&);

    WEBCORE_EXPORT bool postTaskToServiceWorker(ServiceWorkerIdentifier, WTF::Function<void(ServiceWorkerGlobalScope&)>&&);

    using ServiceWorkerCreationCallback = void(uint64_t);
    void setServiceWorkerCreationCallback(ServiceWorkerCreationCallback* callback) { m_serviceWorkerCreationCallback = callback; }

    ServiceWorkerThreadProxy* workerByID(ServiceWorkerIdentifier identifier) { return m_workerMap.get(identifier); }

    WEBCORE_EXPORT void stopAllServiceWorkers();

    static constexpr Seconds workerTerminationTimeout { 10_s };
    static constexpr Seconds syncWorkerTerminationTimeout { 100_ms }; // Only used by layout tests.

private:
    SWContextManager() = default;

    void startedServiceWorker(Optional<ServiceWorkerJobDataIdentifier>, ServiceWorkerIdentifier, const String& exceptionMessage, bool doesHandleFetch);
    NO_RETURN_DUE_TO_CRASH void serviceWorkerFailedToTerminate(ServiceWorkerIdentifier);

    void stopWorker(ServiceWorkerThreadProxy&, Seconds, Function<void()>&&);

    HashMap<ServiceWorkerIdentifier, Ref<ServiceWorkerThreadProxy>> m_workerMap;
    std::unique_ptr<Connection> m_connection;
    ServiceWorkerCreationCallback* m_serviceWorkerCreationCallback { nullptr };

    class ServiceWorkerTerminationRequest {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        ServiceWorkerTerminationRequest(SWContextManager&, ServiceWorkerIdentifier, Seconds timeout);

    private:
        Timer m_timeoutTimer;
    };
    HashMap<ServiceWorkerIdentifier, std::unique_ptr<ServiceWorkerTerminationRequest>> m_pendingServiceWorkerTerminationRequests;
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
