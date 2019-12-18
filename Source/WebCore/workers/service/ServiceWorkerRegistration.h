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

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "SWClientConnection.h"
#include "ServiceWorkerRegistrationData.h"
#include "Timer.h"

namespace WebCore {

class DeferredPromise;
class ScriptExecutionContext;
class ServiceWorker;
class ServiceWorkerContainer;

class ServiceWorkerRegistration final : public RefCounted<ServiceWorkerRegistration>, public EventTargetWithInlineData, public ActiveDOMObject {
    WTF_MAKE_ISO_ALLOCATED(ServiceWorkerRegistration);
public:
    static Ref<ServiceWorkerRegistration> getOrCreate(ScriptExecutionContext&, Ref<ServiceWorkerContainer>&&, ServiceWorkerRegistrationData&&);

    ~ServiceWorkerRegistration();

    ServiceWorkerRegistrationIdentifier identifier() const { return m_registrationData.identifier; }

    ServiceWorker* installing();
    ServiceWorker* waiting();
    ServiceWorker* active();

    ServiceWorker* getNewestWorker() const;

    const String& scope() const;

    ServiceWorkerUpdateViaCache updateViaCache() const;
    void setUpdateViaCache(ServiceWorkerUpdateViaCache);

    WallTime lastUpdateTime() const;
    void setLastUpdateTime(WallTime);

    bool needsUpdate() const { return lastUpdateTime() && (WallTime::now() - lastUpdateTime()) > 86400_s; }

    void update(Ref<DeferredPromise>&&);
    void unregister(Ref<DeferredPromise>&&);

    using RefCounted::ref;
    using RefCounted::deref;
    
    const ServiceWorkerRegistrationData& data() const { return m_registrationData; }

    void updateStateFromServer(ServiceWorkerRegistrationState, RefPtr<ServiceWorker>&&);
    void queueTaskToFireUpdateFoundEvent();

    // ActiveDOMObject.
    bool hasPendingActivity() const final;

private:
    ServiceWorkerRegistration(ScriptExecutionContext&, Ref<ServiceWorkerContainer>&&, ServiceWorkerRegistrationData&&);

    EventTargetInterface eventTargetInterface() const final;
    ScriptExecutionContext* scriptExecutionContext() const final;
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    // ActiveDOMObject.
    const char* activeDOMObjectName() const final;
    void stop() final;

    ServiceWorkerRegistrationData m_registrationData;
    Ref<ServiceWorkerContainer> m_container;

    RefPtr<ServiceWorker> m_installingWorker;
    RefPtr<ServiceWorker> m_waitingWorker;
    RefPtr<ServiceWorker> m_activeWorker;

    bool m_isStopped { false };
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
