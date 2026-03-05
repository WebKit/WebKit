/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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

#include <WebCore/ResourceLoaderIdentifier.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <WebCore/ServiceWorkerJobClient.h>
#include <WebCore/ServiceWorkerJobData.h>
#include <WebCore/ServiceWorkerTypes.h>
#include <WebCore/WorkerScriptLoader.h>
#include <WebCore/WorkerScriptLoaderClient.h>
#include <wtf/CompletionHandler.h>
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>

namespace WebCore {

class DeferredPromise;
class Exception;
class ScriptExecutionContext;
struct ServiceWorkerRegistrationData;

class ServiceWorkerJob final : public RefCounted<ServiceWorkerJob>, public WorkerScriptLoaderClient {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(ServiceWorkerJob, WEBCORE_EXPORT);
public:
    static Ref<ServiceWorkerJob> create(ServiceWorkerJobClient&, Ref<DeferredPromise>&&, ServiceWorkerJobData&&);
    WEBCORE_EXPORT ~ServiceWorkerJob();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void failedWithException(const Exception&);
    void resolvedWithRegistration(ServiceWorkerRegistrationData&&, ShouldNotifyWhenResolved);
    void resolvedWithUnregistrationResult(bool);
    void startScriptFetch(FetchOptions::Cache);

    using Identifier = ServiceWorkerJobIdentifier;
    Identifier identifier() const { return m_jobData.identifier().jobIdentifier; }

    const ServiceWorkerJobData& data() const { return m_jobData; }
    Ref<DeferredPromise> takePromise();

    void fetchScriptWithContext(ScriptExecutionContext&, FetchOptions::Cache);

    const ServiceWorkerOrClientIdentifier& contextIdentifier() { return m_contextIdentifier; }

    bool cancelPendingLoad();

    WEBCORE_EXPORT static ResourceError validateServiceWorkerResponse(const ServiceWorkerJobData&, const ResourceResponse&);

    bool isRegistering() const;

private:
    ServiceWorkerJob(ServiceWorkerJobClient&, Ref<DeferredPromise>&&, ServiceWorkerJobData&&);

    // WorkerScriptLoaderClient
    void didReceiveResponse(ScriptExecutionContextIdentifier, std::optional<ResourceLoaderIdentifier>, const ResourceResponse&) final;
    void notifyFinished(std::optional<ScriptExecutionContextIdentifier>) final;

    WeakPtr<ServiceWorkerJobClient> m_client;
    ServiceWorkerJobData m_jobData;
    Ref<DeferredPromise> m_promise;

    bool m_completed { false };

    ServiceWorkerOrClientIdentifier m_contextIdentifier;
    RefPtr<WorkerScriptLoader> m_scriptLoader;

#if ASSERT_ENABLED
    const Ref<Thread> m_creationThread { Thread::currentSingleton() };
#endif
};

} // namespace WebCore
