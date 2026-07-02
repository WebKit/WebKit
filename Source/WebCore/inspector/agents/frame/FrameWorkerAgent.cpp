/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "FrameWorkerAgent.h"

#include "Document.h"
#include "DocumentPage.h"
#include "FrameDestructionObserverInlines.h"
#include "InstrumentingAgents.h"
#include "LocalFrame.h"
#include "Page.h"
#include "WorkerInspectorProxy.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameWorkerAgent);

FrameWorkerAgent::FrameWorkerAgent(FrameAgentContext& context)
    : InspectorWorkerAgent(context)
    , m_inspectedFrame(context.inspectedFrame)
{
}

FrameWorkerAgent::~FrameWorkerAgent() = default;

void FrameWorkerAgent::didCreateFrontendAndBackend()
{
    Ref { m_instrumentingAgents.get() }->setPersistentWorkerAgent(this);
}

void FrameWorkerAgent::willDestroyFrontendAndBackend(DisconnectReason reason)
{
    Ref { m_instrumentingAgents.get() }->setPersistentWorkerAgent(nullptr);

    InspectorWorkerAgent::willDestroyFrontendAndBackend(reason);
}

void FrameWorkerAgent::connectToAllWorkerInspectorProxies()
{
    RefPtr frame = m_inspectedFrame.get();
    if (!frame)
        return;

    RefPtr page = frame->page();
    if (!page || !page->identifier())
        return;

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=312381 SI WorkerInspectorProxy should support proxiesForFrame()
    for (Ref proxy : WorkerInspectorProxy::proxiesForPage(*page->identifier())) {
        RefPtr context = proxy->scriptExecutionContext();
        if (!context)
            continue;

        RefPtr document = dynamicDowncast<Document>(context.get());
        if (!document || document->frame() != frame.get())
            continue;

        connectToWorkerInspectorProxy(proxy);
    }
}

} // namespace WebCore
