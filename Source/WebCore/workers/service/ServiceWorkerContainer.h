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
#include "AddEventListenerOptions.h"
#include "EventTarget.h"
#include "IDLTypes.h"
#include "JSDOMPromiseDeferred.h"
#include "SWClientConnection.h"
#include "SWServer.h"
#include "ServiceWorkerJobClient.h"
#include "ServiceWorkerRegistration.h"
#include "ServiceWorkerRegistrationOptions.h"
#include "WorkerType.h"
#include <wtf/Threading.h>

namespace WebCore {

class DeferredPromise;
class NavigatorBase;
class ServiceWorker;

enum class ServiceWorkerUpdateViaCache : uint8_t;
enum class WorkerType : uint8_t;

template<typename IDLType> class DOMPromiseProxy;

class ServiceWorkerContainer final : public EventTargetWithInlineData, public ActiveDOMObject, public ServiceWorkerJobClient {
    WTF_MAKE_NONCOPYABLE(ServiceWorkerContainer);
    WTF_MAKE_ISO_ALLOCATED(ServiceWorkerContainer);
public:
    ServiceWorkerContainer(ScriptExecutionContext*, NavigatorBase&);
    ~ServiceWorkerContainer();

    ServiceWorker* controller() const;

    using ReadyPromise = DOMPromiseProxy<IDLInterface<ServiceWorkerRegistration>>;
    ReadyPromise& ready();

    using RegistrationOptions = ServiceWorkerRegistrationOptions;
    void addRegistration(const String& scriptURL, const RegistrationOptions&, Ref<DeferredPromise>&&);
    void unregisterRegistration(ServiceWorkerRegistrationIdentifier, DOMPromiseDeferred<IDLBoolean>&&);
    void updateRegistration(const URL& scopeURL, const URL& scriptURL, WorkerType, RefPtr<DeferredPromise>&&);

    void getRegistration(const String& clientURL, Ref<DeferredPromise>&&);
    void updateRegistrationState(ServiceWorkerRegistrationIdentifier, ServiceWorkerRegistrationState, const Optional<ServiceWorkerData>&);
    void updateWorkerState(ServiceWorkerIdentifier, ServiceWorkerState);
    void queueTaskToFireUpdateFoundEvent(ServiceWorkerRegistrationIdentifier);
    void queueTaskToDispatchControllerChangeEvent();

    void postMessage(MessageWithMessagePorts&&, ServiceWorkerData&& sourceData, String&& sourceOrigin);

    void getRegistrations(Ref<DeferredPromise>&&);

    ServiceWorkerRegistration* registration(ServiceWorkerRegistrationIdentifier identifier) const { return m_registrations.get(identifier); }

    void addRegistration(ServiceWorkerRegistration&);
    void removeRegistration(ServiceWorkerRegistration&);

    ServiceWorkerJob* job(ServiceWorkerJobIdentifier);

    void startMessages();

    bool isStopped() const { return m_isStopped; };

    bool isAlwaysOnLoggingAllowed() const;

    NavigatorBase* navigator() { return &m_navigator; }

private:
    bool addEventListener(const AtomString& eventType, Ref<EventListener>&&, const AddEventListenerOptions& = { }) final;

    void scheduleJob(std::unique_ptr<ServiceWorkerJob>&&);

    void jobFailedWithException(ServiceWorkerJob&, const Exception&) final;
    void jobResolvedWithRegistration(ServiceWorkerJob&, ServiceWorkerRegistrationData&&, ShouldNotifyWhenResolved) final;
    void jobResolvedWithUnregistrationResult(ServiceWorkerJob&, bool unregistrationResult) final;
    void startScriptFetchForJob(ServiceWorkerJob&, FetchOptions::Cache) final;
    void jobFinishedLoadingScript(ServiceWorkerJob&, const String& script, const CertificateInfo&, const ContentSecurityPolicyResponseHeaders&, const String& referrerPolicy) final;
    void jobFailedLoadingScript(ServiceWorkerJob&, const ResourceError&, Exception&&) final;

    void notifyFailedFetchingScript(ServiceWorkerJob&, const ResourceError&);
    void destroyJob(ServiceWorkerJob&);

    DocumentOrWorkerIdentifier contextIdentifier() final;

    SWClientConnection& ensureSWClientConnection();

    // ActiveDOMObject.
    const char* activeDOMObjectName() const final;
    
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
    EventTargetInterface eventTargetInterface() const final { return ServiceWorkerContainerEventTargetInterfaceType; }
    void refEventTarget() final;
    void derefEventTarget() final;
    void stop() final;

    void notifyRegistrationIsSettled(const ServiceWorkerRegistrationKey&);

    std::unique_ptr<ReadyPromise> m_readyPromise;

    NavigatorBase& m_navigator;

    RefPtr<SWClientConnection> m_swConnection;

    struct OngoingJob {
        std::unique_ptr<ServiceWorkerJob> job;
        RefPtr<PendingActivity<ServiceWorkerContainer>> pendingActivity;
    };
    HashMap<ServiceWorkerJobIdentifier, OngoingJob> m_jobMap;

    bool m_isStopped { false };
    HashMap<ServiceWorkerRegistrationIdentifier, ServiceWorkerRegistration*> m_registrations;

#ifndef NDEBUG
    Ref<Thread> m_creationThread { Thread::current() };
#endif

    uint64_t m_lastOngoingSettledRegistrationIdentifier { 0 };
    HashMap<uint64_t, ServiceWorkerRegistrationKey> m_ongoingSettledRegistrations;
    bool m_shouldDeferMessageEvents { false };
    Vector<Ref<Event>> m_deferredMessageEvents;
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
