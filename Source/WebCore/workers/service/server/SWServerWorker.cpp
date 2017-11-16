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
#include "SWServerWorker.h"

#if ENABLE(SERVICE_WORKER)

#include <wtf/NeverDestroyed.h>

namespace WebCore {

static HashMap<ServiceWorkerIdentifier, SWServerWorker*>& allWorkers()
{
    static NeverDestroyed<HashMap<ServiceWorkerIdentifier, SWServerWorker*>> workers;
    return workers;
}

SWServerWorker* SWServerWorker::existingWorkerForIdentifier(ServiceWorkerIdentifier identifier)
{
    return allWorkers().get(identifier);
}

SWServerWorker::SWServerWorker(SWServer& server, const ServiceWorkerRegistrationKey& registrationKey, SWServerToContextConnectionIdentifier contextConnectionIdentifier, const URL& scriptURL, const String& script, WorkerType type, ServiceWorkerIdentifier identifier)
    : m_server(server)
    , m_registrationKey(registrationKey)
    , m_contextConnectionIdentifier(contextConnectionIdentifier)
    , m_data { identifier, scriptURL, ServiceWorkerState::Redundant, type }
    , m_script(script)
{
    auto result = allWorkers().add(identifier, this);
    ASSERT_UNUSED(result, result.isNewEntry);
}

SWServerWorker::~SWServerWorker()
{
    auto taken = allWorkers().take(identifier());
    ASSERT_UNUSED(taken, taken == this);
}

void SWServerWorker::terminate()
{
    m_server.terminateWorker(*this);
}

void SWServerWorker::scriptContextFailedToStart(const String& message)
{
    m_server.scriptContextFailedToStart(*this, message);
}

void SWServerWorker::scriptContextStarted()
{
    m_server.scriptContextStarted(*this);
}

void SWServerWorker::didFinishInstall(bool wasSuccessful)
{
    m_server.didFinishInstall(*this, wasSuccessful);
}

void SWServerWorker::didFinishActivation()
{
    m_server.didFinishActivation(*this);
}

void SWServerWorker::contextTerminated()
{
    m_server.workerContextTerminated(*this);
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
