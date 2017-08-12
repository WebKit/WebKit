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

#include "config.h"
#include "ServiceWorkerJob.h"

#if ENABLE(SERVICE_WORKER)

#include "JSDOMPromiseDeferred.h"
#include "ServiceWorkerRegistrationParameters.h"

namespace WebCore {

static std::atomic<uint64_t> currentIdentifier;

ServiceWorkerJob::ServiceWorkerJob(ServiceWorkerJobClient& client, Ref<DeferredPromise>&& promise, ServiceWorkerRegistrationParameters&& parameters)
    : m_client(client)
    , m_promise(WTFMove(promise))
    , m_identifier(++currentIdentifier)
{
    m_registrationParameters = std::make_unique<ServiceWorkerRegistrationParameters>(WTFMove(parameters));
}

ServiceWorkerJob::~ServiceWorkerJob()
{
    ASSERT(currentThread() == m_creationThread);
}

void ServiceWorkerJob::failedWithException(Exception&& exception)
{
    ASSERT(currentThread() == m_creationThread);

    ASSERT(!m_completed);
    m_promise->reject(WTFMove(exception));
    m_completed = true;

    // Can cause this to be deleted.
    m_client->jobDidFinish(*this);
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
