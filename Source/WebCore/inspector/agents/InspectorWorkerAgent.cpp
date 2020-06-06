/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "InspectorWorkerAgent.h"

#include "Document.h"
#include "InstrumentingAgents.h"


namespace WebCore {

using namespace Inspector;

InspectorWorkerAgent::InspectorWorkerAgent(PageAgentContext& context)
    : InspectorAgentBase("Worker"_s, context)
    , m_frontendDispatcher(makeUnique<Inspector::WorkerFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::WorkerBackendDispatcher::create(context.backendDispatcher, this))
    , m_page(context.inspectedPage)
{
}

InspectorWorkerAgent::~InspectorWorkerAgent() = default;

void InspectorWorkerAgent::didCreateFrontendAndBackend(FrontendRouter*, BackendDispatcher*)
{
    m_instrumentingAgents.setPersistentWorkerAgent(this);
}

void InspectorWorkerAgent::willDestroyFrontendAndBackend(DisconnectReason)
{
    m_instrumentingAgents.setPersistentWorkerAgent(nullptr);

    ErrorString ignored;
    disable(ignored);
}

void InspectorWorkerAgent::enable(ErrorString&)
{
    if (m_enabled)
        return;

    m_enabled = true;

    connectToAllWorkerInspectorProxiesForPage();
}

void InspectorWorkerAgent::disable(ErrorString&)
{
    if (!m_enabled)
        return;

    m_enabled = false;

    disconnectFromAllWorkerInspectorProxies();
}

void InspectorWorkerAgent::initialized(ErrorString& errorString, const String& workerId)
{
    WorkerInspectorProxy* proxy = m_connectedProxies.get(workerId);
    if (!proxy) {
        errorString = "Missing worker for given workerId"_s;
        return;
    }

    proxy->resumeWorkerIfPaused();
}

void InspectorWorkerAgent::sendMessageToWorker(ErrorString& errorString, const String& workerId, const String& message)
{
    if (!m_enabled) {
        errorString = "Worker domain must be enabled"_s;
        return;
    }

    WorkerInspectorProxy* proxy = m_connectedProxies.get(workerId);
    if (!proxy) {
        errorString = "Missing worker for given workerId"_s;
        return;
    }

    proxy->sendMessageToWorkerInspectorController(message);
}

void InspectorWorkerAgent::sendMessageFromWorkerToFrontend(WorkerInspectorProxy& proxy, const String& message)
{
    m_frontendDispatcher->dispatchMessageFromWorker(proxy.identifier(), message);
}

bool InspectorWorkerAgent::shouldWaitForDebuggerOnStart() const
{
    return m_enabled;
}

void InspectorWorkerAgent::workerStarted(WorkerInspectorProxy& proxy)
{
    if (!m_enabled)
        return;

    connectToWorkerInspectorProxy(proxy);
}

void InspectorWorkerAgent::workerTerminated(WorkerInspectorProxy& proxy)
{
    if (!m_enabled)
        return;

    disconnectFromWorkerInspectorProxy(proxy);
}

void InspectorWorkerAgent::connectToAllWorkerInspectorProxiesForPage()
{
    ASSERT(m_connectedProxies.isEmpty());

    for (auto* proxy : WorkerInspectorProxy::allWorkerInspectorProxies()) {
        if (!is<Document>(proxy->scriptExecutionContext()))
            continue;

        Document& document = downcast<Document>(*proxy->scriptExecutionContext());
        if (document.page() != &m_page)
            continue;

        connectToWorkerInspectorProxy(*proxy);
    }
}

void InspectorWorkerAgent::disconnectFromAllWorkerInspectorProxies()
{
    for (auto* proxy : copyToVector(m_connectedProxies.values()))
        proxy->disconnectFromWorkerInspectorController();

    m_connectedProxies.clear();
}

void InspectorWorkerAgent::connectToWorkerInspectorProxy(WorkerInspectorProxy& proxy)
{
    proxy.connectToWorkerInspectorController(*this);

    m_connectedProxies.set(proxy.identifier(), &proxy);

    m_frontendDispatcher->workerCreated(proxy.identifier(), proxy.url().string(), proxy.name());
}

void InspectorWorkerAgent::disconnectFromWorkerInspectorProxy(WorkerInspectorProxy& proxy)
{
    m_frontendDispatcher->workerTerminated(proxy.identifier());

    m_connectedProxies.remove(proxy.identifier());

    proxy.disconnectFromWorkerInspectorController();
}

} // namespace Inspector
