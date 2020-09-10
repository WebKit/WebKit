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
#include "WorkerNetworkAgent.h"

#include "WorkerDebuggerProxy.h"
#include "WorkerGlobalScope.h"
#include "WorkerThread.h"

namespace WebCore {

using namespace Inspector;

WorkerNetworkAgent::WorkerNetworkAgent(WorkerAgentContext& context)
    : InspectorNetworkAgent(context)
    , m_workerGlobalScope(context.workerGlobalScope)
{
    ASSERT(context.workerGlobalScope.isContextThread());
}

WorkerNetworkAgent::~WorkerNetworkAgent() = default;

Protocol::Network::LoaderId WorkerNetworkAgent::loaderIdentifier(DocumentLoader*)
{
    return { };
}

Protocol::Network::FrameId WorkerNetworkAgent::frameIdentifier(DocumentLoader*)
{
    return { };
}

Vector<WebSocket*> WorkerNetworkAgent::activeWebSockets(const LockHolder&)
{
    // FIXME: <https://webkit.org/b/168475> Web Inspector: Correctly display worker's WebSockets
    return { };
}

void WorkerNetworkAgent::setResourceCachingDisabledInternal(bool disabled)
{
    m_workerGlobalScope.thread().workerDebuggerProxy().setResourceCachingDisabledByWebInspector(disabled);
}

ScriptExecutionContext* WorkerNetworkAgent::scriptExecutionContext(Protocol::ErrorString&, const Protocol::Network::FrameId&)
{
    return &m_workerGlobalScope;
}

} // namespace WebCore
