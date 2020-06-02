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

#include "ServiceWorkerContextData.h"
#include "ServiceWorkerFetch.h"
#include "ServiceWorkerIdentifier.h"
#include "Timer.h"
#include "WorkerThread.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class CacheStorageProvider;
class ContentSecurityPolicyResponseHeaders;
class ExtendableEvent;
class MessagePortChannel;
class SerializedScriptValue;
class WorkerObjectProxy;
struct MessageWithMessagePorts;
struct ServiceWorkerClientIdentifier;

class ServiceWorkerThread : public WorkerThread, public CanMakeWeakPtr<ServiceWorkerThread, WeakPtrFactoryInitialization::Eager> {
public:
    template<typename... Args> static Ref<ServiceWorkerThread> create(Args&&... args)
    {
        return adoptRef(*new ServiceWorkerThread(std::forward<Args>(args)...));
    }
    virtual ~ServiceWorkerThread();

    WorkerObjectProxy& workerObjectProxy() const { return m_workerObjectProxy; }

    void start(Function<void(const String&, bool)>&&);

    void willPostTaskToFireInstallEvent();
    void willPostTaskToFireActivateEvent();
    void willPostTaskToFireMessageEvent();

    void queueTaskToFireFetchEvent(Ref<ServiceWorkerFetch::Client>&&, Optional<ServiceWorkerClientIdentifier>&&, ResourceRequest&&, String&& referrer, FetchOptions&&);
    void queueTaskToPostMessage(MessageWithMessagePorts&&, ServiceWorkerOrClientData&& sourceData);
    void queueTaskToFireInstallEvent();
    void queueTaskToFireActivateEvent();

    const ServiceWorkerContextData& contextData() const { return m_data; }

    ServiceWorkerIdentifier identifier() const { return m_data.serviceWorkerIdentifier; }
    bool doesHandleFetch() const { return m_doesHandleFetch; }

    void startFetchEventMonitoring();
    void stopFetchEventMonitoring() { m_isHandlingFetchEvent = false; }

protected:
    Ref<WorkerGlobalScope> createWorkerGlobalScope(const WorkerParameters&, Ref<SecurityOrigin>&&, Ref<SecurityOrigin>&& topOrigin) final;
    void runEventLoop() override;

private:
    WEBCORE_EXPORT ServiceWorkerThread(const ServiceWorkerContextData&, String&& userAgent, WorkerLoaderProxy&, WorkerDebuggerProxy&, IDBClient::IDBConnectionProxy*, SocketProvider*);

    bool isServiceWorkerThread() const final { return true; }
    void finishedEvaluatingScript() final;

    void finishedFiringInstallEvent(bool hasRejectedAnyPromise);
    void finishedFiringActivateEvent();
    void finishedFiringMessageEvent();
    void finishedStarting();

    void startHeartBeatTimer();
    void heartBeatTimerFired();
    void installEventTimerFired();

    ServiceWorkerContextData m_data;
    WorkerObjectProxy& m_workerObjectProxy;
    bool m_doesHandleFetch { false };

    bool m_isHandlingFetchEvent { false };
    uint64_t m_messageEventCount { 0 };
    enum class State { Idle, Starting, Installing, Activating };
    State m_state { State::Idle };
    bool m_ongoingHeartBeatCheck { false };

    static constexpr Seconds heartBeatTimeout { 60_s };
    static constexpr Seconds heartBeatTimeoutForTest { 1_s };
    Seconds m_heartBeatTimeout { heartBeatTimeout };
    Timer m_heartBeatTimer;
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
