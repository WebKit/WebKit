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
#include "ServiceWorkerAgent.h"

#if ENABLE(SERVICE_WORKER)

#include "SecurityOrigin.h"
#include "ServiceWorkerGlobalScope.h"
#include "ServiceWorkerThread.h"

namespace WebCore {

using namespace Inspector;

ServiceWorkerAgent::ServiceWorkerAgent(WorkerAgentContext& context)
    : InspectorAgentBase("ServiceWorker"_s, context)
    , m_serviceWorkerGlobalScope(downcast<ServiceWorkerGlobalScope>(context.workerGlobalScope))
    , m_backendDispatcher(Inspector::ServiceWorkerBackendDispatcher::create(context.backendDispatcher, this))
{
    ASSERT(context.workerGlobalScope.isContextThread());
}

ServiceWorkerAgent::~ServiceWorkerAgent() = default;

void ServiceWorkerAgent::didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*)
{
}

void ServiceWorkerAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
}

Protocol::ErrorStringOr<Ref<Protocol::ServiceWorker::Configuration>> ServiceWorkerAgent::getInitializationInfo()
{
    return Protocol::ServiceWorker::Configuration::create()
        .setTargetId(m_serviceWorkerGlobalScope.identifier())
        .setSecurityOrigin(m_serviceWorkerGlobalScope.securityOrigin()->toRawString())
        .setUrl(m_serviceWorkerGlobalScope.thread().contextData().scriptURL.string())
        .setContent(m_serviceWorkerGlobalScope.thread().contextData().script)
        .release();
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
