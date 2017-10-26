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
#include "DOMPromiseProxy.h"
#include "EventTarget.h"
#include "SWClientConnection.h"
#include "SWServer.h"
#include "ServiceWorkerJobClient.h"
#include "ServiceWorkerRegistration.h"
#include "ServiceWorkerRegistrationOptions.h"
#include <pal/SessionID.h>
#include <wtf/Threading.h>

namespace WebCore {

class DeferredPromise;
class NavigatorBase;
class ServiceWorker;

enum class ServiceWorkerUpdateViaCache;
enum class WorkerType;

class ServiceWorkerContainer final : public EventTargetWithInlineData, public ActiveDOMObject, public ServiceWorkerJobClient {
public:
    ServiceWorkerContainer(ScriptExecutionContext&, NavigatorBase&);
    ~ServiceWorkerContainer();

    typedef WebCore::RegistrationOptions RegistrationOptions;

    ServiceWorker* controller() const;

    using ReadyPromise = DOMPromiseProxy<IDLInterface<ServiceWorkerRegistration>>;
    ReadyPromise& ready() { return m_readyPromise; }

    void addRegistration(const String& scriptURL, const RegistrationOptions&, Ref<DeferredPromise>&&);
    void removeRegistration(const URL& scopeURL, Ref<DeferredPromise>&&);

    using RegistrationPromise = DOMPromiseDeferred<IDLNullable<IDLInterface<ServiceWorkerRegistration>>>;
    void getRegistration(const String& clientURL, RegistrationPromise&&);
    void getRegistrations(Ref<DeferredPromise>&&);

    void startMessages();

    void ref() final { refEventTarget(); }
    void deref() final { derefEventTarget(); }

private:
    void scheduleJob(Ref<ServiceWorkerJob>&&);

    void jobFailedWithException(ServiceWorkerJob&, const Exception&) final;
    void jobResolvedWithRegistration(ServiceWorkerJob&, ServiceWorkerRegistrationData&&) final;
    void jobResolvedWithUnregistrationResult(ServiceWorkerJob&, bool unregistrationResult) final;
    void startScriptFetchForJob(ServiceWorkerJob&) final;
    void jobFinishedLoadingScript(ServiceWorkerJob&, const String&) final;
    void jobFailedLoadingScript(ServiceWorkerJob&, const ResourceError&) final;

    void jobDidFinish(ServiceWorkerJob&);

    uint64_t connectionIdentifier() final;

    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
    EventTargetInterface eventTargetInterface() const final { return ServiceWorkerContainerEventTargetInterfaceType; }
    void refEventTarget() final;
    void derefEventTarget() final;

    ReadyPromise m_readyPromise;

    NavigatorBase& m_navigator;

    RefPtr<SWClientConnection> m_swConnection;
    HashMap<uint64_t, RefPtr<ServiceWorkerJob>> m_jobMap;
    mutable RefPtr<ServiceWorker> m_controller;

#ifndef NDEBUG
    ThreadIdentifier m_creationThread { currentThread() };
#endif
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
