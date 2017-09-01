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

#include "ResourceResponse.h"
#include "ServiceWorkerJobClient.h"
#include "ServiceWorkerJobData.h"
#include "ThreadableLoader.h"
#include "ThreadableLoaderClient.h"
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>

namespace WebCore {

class DeferredPromise;
class Exception;
class ScriptExecutionContext;
enum class ServiceWorkerJobType;
struct ServiceWorkerRegistrationData;

class ServiceWorkerJob : public ThreadSafeRefCounted<ServiceWorkerJob>, public ThreadableLoaderClient {
public:
    static Ref<ServiceWorkerJob> create(ServiceWorkerJobClient& client, Ref<DeferredPromise>&& promise, ServiceWorkerJobData&& jobData)
    {
        return adoptRef(*new ServiceWorkerJob(client, WTFMove(promise), WTFMove(jobData)));
    }

    WEBCORE_EXPORT ~ServiceWorkerJob();

    void failedWithException(const Exception&);
    void resolvedWithRegistration(const ServiceWorkerRegistrationData&);
    void startScriptFetch();

    ServiceWorkerJobData data() const { return m_jobData; }
    DeferredPromise& promise() { return m_promise.get(); }

    void fetchScriptWithContext(ScriptExecutionContext&);

private:
    ServiceWorkerJob(ServiceWorkerJobClient&, Ref<DeferredPromise>&&, ServiceWorkerJobData&&);

    // ThreadableLoaderClient
    void didReceiveResponse(unsigned long identifier, const ResourceResponse&) final;
    void didReceiveData(const char* data, int length) final;
    void didFinishLoading(unsigned long identifier) final;
    void didFail(const ResourceError&) final;

    Ref<ServiceWorkerJobClient> m_client;
    ServiceWorkerJobData m_jobData;
    Ref<DeferredPromise> m_promise;

    bool m_completed { false };

    Ref<RunLoop> m_runLoop { RunLoop::current() };
    RefPtr<ThreadableLoader> m_loader;
    ResourceResponse m_lastResponse;
    std::optional<Ref<SharedBuffer>> m_scriptData;

#if !ASSERT_DISABLED
    ThreadIdentifier m_creationThread { currentThread() };
#endif
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)

