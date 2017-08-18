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

#include "ServiceWorkerJobClient.h"
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>

namespace WebCore {

class DeferredPromise;
class Exception;
enum class ServiceWorkerJobType;
struct ServiceWorkerJobData;
struct ServiceWorkerRegistrationParameters;

class ServiceWorkerJob : public ThreadSafeRefCounted<ServiceWorkerJob> {
public:
    static Ref<ServiceWorkerJob> createRegisterJob(ServiceWorkerJobClient& client, Ref<DeferredPromise>&& promise, ServiceWorkerRegistrationParameters&& parameters)
    {
        return adoptRef(*new ServiceWorkerJob(client, WTFMove(promise), WTFMove(parameters)));
    }

    WEBCORE_EXPORT ~ServiceWorkerJob();

    WEBCORE_EXPORT void failedWithException(const Exception&);

    uint64_t identifier() const { return m_identifier; }

    ServiceWorkerJobData data() const;

private:
    ServiceWorkerJob(ServiceWorkerJobClient&, Ref<DeferredPromise>&&, ServiceWorkerRegistrationParameters&&);

    Ref<ServiceWorkerJobClient> m_client;
    std::unique_ptr<ServiceWorkerRegistrationParameters> m_registrationParameters;
    Ref<DeferredPromise> m_promise;

    bool m_completed { false };
    uint64_t m_identifier;

    ServiceWorkerJobType m_type;

    Ref<RunLoop> m_runLoop { RunLoop::current() };

#if !ASSERT_DISABLED
    ThreadIdentifier m_creationThread { currentThread() };
#endif
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)

