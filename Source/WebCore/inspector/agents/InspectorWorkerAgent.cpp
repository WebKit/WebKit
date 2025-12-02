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
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(InspectorWorkerAgent);

InspectorWorkerAgent::InspectorWorkerAgent(WebAgentContext& context)
    : InspectorAgentBase("Worker"_s, context)
    , m_pageChannel(PageChannel::create(*this))
    , m_frontendDispatcher(makeUniqueRef<Inspector::WorkerFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::WorkerBackendDispatcher::create(context.backendDispatcher, this))
{
}

InspectorWorkerAgent::~InspectorWorkerAgent()
{
    m_pageChannel->detachFromParentAgent();
}

void InspectorWorkerAgent::didCreateFrontendAndBackend()
{
    Ref { m_instrumentingAgents.get() }->setPersistentWorkerAgent(this);
}

void InspectorWorkerAgent::willDestroyFrontendAndBackend(DisconnectReason)
{
    Ref { m_instrumentingAgents.get() }->setPersistentWorkerAgent(nullptr);

    disable();
}

Inspector::Protocol::ErrorStringOr<void> InspectorWorkerAgent::enable()
{
    if (m_enabled)
        return { };

    m_enabled = true;

    connectToAllWorkerInspectorProxies();

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorWorkerAgent::disable()
{
    if (!m_enabled)
        return { };

    m_enabled = false;

    disconnectFromAllWorkerInspectorProxies();

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorWorkerAgent::initialized(const String& workerId)
{
    RefPtr proxy = m_connectedProxies.get(workerId);
    if (!proxy)
        return makeUnexpected("Missing worker for given workerId"_s);

    proxy->resumeWorkerIfPaused();

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorWorkerAgent::sendMessageToWorker(const String& workerId, const String& message)
{
    if (!m_enabled)
        return makeUnexpected("Worker domain must be enabled"_s);

    RefPtr proxy = m_connectedProxies.get(workerId);
    if (!proxy)
        return makeUnexpected("Missing worker for given workerId"_s);

    proxy->sendMessageToWorkerInspectorController(message);

    return { };
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

void InspectorWorkerAgent::disconnectFromAllWorkerInspectorProxies()
{
    for (auto& proxyWeakPtr : copyToVector(m_connectedProxies.values())) {
        RefPtr proxy = proxyWeakPtr.get();
        if (!proxy)
            continue;

        proxy->disconnectFromWorkerInspectorController();
    }

    m_connectedProxies.clear();
}

void InspectorWorkerAgent::connectToWorkerInspectorProxy(WorkerInspectorProxy& proxy)
{
    proxy.connectToWorkerInspectorController(m_pageChannel);

    m_connectedProxies.set(proxy.identifier(), proxy);

    m_frontendDispatcher->workerCreated(proxy.identifier(), proxy.url().string(), proxy.name());
}

void InspectorWorkerAgent::disconnectFromWorkerInspectorProxy(WorkerInspectorProxy& proxy)
{
    m_frontendDispatcher->workerTerminated(proxy.identifier());

    m_connectedProxies.remove(proxy.identifier());

    proxy.disconnectFromWorkerInspectorController();
}

Ref<InspectorWorkerAgent::PageChannel> InspectorWorkerAgent::PageChannel::create(InspectorWorkerAgent& parentAgent)
{
    return adoptRef(*new PageChannel(parentAgent));
}

InspectorWorkerAgent::PageChannel::PageChannel(InspectorWorkerAgent& parentAgent)
    : m_parentAgent(&parentAgent)
{
}

void InspectorWorkerAgent::PageChannel::detachFromParentAgent()
{
    Locker locker { m_parentAgentLock };

    m_parentAgent = nullptr;
}

void InspectorWorkerAgent::PageChannel::sendMessageFromWorkerToFrontend(WorkerInspectorProxy& proxy, String&& message)
{
    Locker locker { m_parentAgentLock };

    if (CheckedPtr parentAgent = m_parentAgent)
        parentAgent->frontendDispatcher().dispatchMessageFromWorker(proxy.identifier(), WTFMove(message));
}

} // namespace Inspector
