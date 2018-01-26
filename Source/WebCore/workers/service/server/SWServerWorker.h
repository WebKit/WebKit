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

#include "ServiceWorkerClientData.h"
#include "ServiceWorkerData.h"
#include "ServiceWorkerIdentifier.h"
#include "ServiceWorkerRegistrationKey.h"
#include "ServiceWorkerTypes.h"
#include "URL.h"
#include <wtf/RefCounted.h>

namespace WebCore {

struct ClientOrigin;
class SWServer;
class SWServerRegistration;
struct ServiceWorkerClientData;
struct ServiceWorkerClientIdentifier;
struct ServiceWorkerClientQueryOptions;
struct ServiceWorkerContextData;
struct ServiceWorkerJobDataIdentifier;
enum class WorkerType;

class SWServerWorker : public RefCounted<SWServerWorker> {
public:
    template <typename... Args> static Ref<SWServerWorker> create(Args&&... args)
    {
        return adoptRef(*new SWServerWorker(std::forward<Args>(args)...));
    }
    
    SWServerWorker(const SWServerWorker&) = delete;
    WEBCORE_EXPORT ~SWServerWorker();

    void terminate();

    WEBCORE_EXPORT void whenActivated(WTF::Function<void(bool)>&&);

    enum class State {
        Running,
        Terminating,
        NotRunning,
    };
    bool isRunning() const { return m_state == State::Running; }
    bool isTerminating() const { return m_state == State::Terminating; }
    void setState(State);

    SWServer& server() { return m_server; }
    const ServiceWorkerRegistrationKey& registrationKey() const { return m_registrationKey; }
    const URL& scriptURL() const { return m_data.scriptURL; }
    const String& script() const { return m_script; }
    WorkerType type() const { return m_data.type; }

    ServiceWorkerIdentifier identifier() const { return m_data.identifier; }

    std::optional<SWServerToContextConnectionIdentifier> contextConnectionIdentifier() const { return m_contextConnectionIdentifier; }
    void setContextConnectionIdentifier(std::optional<SWServerToContextConnectionIdentifier> identifier) { m_contextConnectionIdentifier = identifier; }

    ServiceWorkerState state() const { return m_data.state; }
    void setState(ServiceWorkerState);

    bool hasPendingEvents() const { return m_hasPendingEvents; }
    void setHasPendingEvents(bool);

    void scriptContextFailedToStart(const std::optional<ServiceWorkerJobDataIdentifier>&, const String& message);
    void scriptContextStarted(const std::optional<ServiceWorkerJobDataIdentifier>&);
    void didFinishInstall(const std::optional<ServiceWorkerJobDataIdentifier>&, bool wasSuccessful);
    void didFinishActivation();
    void contextTerminated();
    WEBCORE_EXPORT std::optional<ServiceWorkerClientData> findClientByIdentifier(const ServiceWorkerClientIdentifier&) const;
    void matchAll(const ServiceWorkerClientQueryOptions&, ServiceWorkerClientsMatchAllCallback&&);
    void claim();

    void skipWaiting();
    bool isSkipWaitingFlagSet() const { return m_isSkipWaitingFlagSet; }

    WEBCORE_EXPORT static SWServerWorker* existingWorkerForIdentifier(ServiceWorkerIdentifier);

    const ServiceWorkerData& data() const { return m_data; }
    ServiceWorkerContextData contextData() const;
    const ClientOrigin& origin() const;

private:
    SWServerWorker(SWServer&, SWServerRegistration&, std::optional<SWServerToContextConnectionIdentifier>, const URL&, const String& script, const ContentSecurityPolicyResponseHeaders&,  WorkerType, ServiceWorkerIdentifier);

    void callWhenActivatedHandler(bool success);

    SWServer& m_server;
    ServiceWorkerRegistrationKey m_registrationKey;
    std::optional<SWServerToContextConnectionIdentifier> m_contextConnectionIdentifier;
    ServiceWorkerData m_data;
    String m_script;
    ContentSecurityPolicyResponseHeaders m_contentSecurityPolicy;
    bool m_hasPendingEvents { false };
    State m_state { State::NotRunning };
    mutable std::optional<ClientOrigin> m_origin;
    bool m_isSkipWaitingFlagSet { false };
    Vector<WTF::Function<void(bool)>> m_whenActivatedHandlers;
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
