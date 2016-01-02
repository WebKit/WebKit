/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorHeapAgent.h"

#include "InspectorEnvironment.h"
#include "VM.h"
#include <wtf/RunLoop.h>
#include <wtf/Stopwatch.h>

using namespace JSC;

namespace Inspector {

InspectorHeapAgent::InspectorHeapAgent(AgentContext& context)
    : InspectorAgentBase(ASCIILiteral("Heap"))
    , m_frontendDispatcher(std::make_unique<HeapFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(HeapBackendDispatcher::create(context.backendDispatcher, this))
    , m_environment(context.environment)
{
}

InspectorHeapAgent::~InspectorHeapAgent()
{
}

void InspectorHeapAgent::didCreateFrontendAndBackend(FrontendRouter*, BackendDispatcher*)
{
}

void InspectorHeapAgent::willDestroyFrontendAndBackend(DisconnectReason)
{
    ErrorString ignored;
    disable(ignored);
}

void InspectorHeapAgent::enable(ErrorString&)
{
    if (m_enabled)
        return;

    m_enabled = true;

    m_environment.vm().heap.addObserver(this);
}

void InspectorHeapAgent::disable(ErrorString&)
{
    if (!m_enabled)
        return;

    m_enabled = false;

    m_environment.vm().heap.removeObserver(this);
}

void InspectorHeapAgent::gc(ErrorString&)
{
    VM& vm = m_environment.vm();
    JSLockHolder lock(vm);
    sanitizeStackForVM(&vm);
    vm.heap.collectAllGarbage();
}

static Inspector::Protocol::Heap::GarbageCollection::Type protocolTypeForHeapOperation(HeapOperation operation)
{
    switch (operation) {
    case FullCollection:
        return Inspector::Protocol::Heap::GarbageCollection::Type::Full;
    case EdenCollection:
        return Inspector::Protocol::Heap::GarbageCollection::Type::Partial;
    default:
        ASSERT_NOT_REACHED();
        return Inspector::Protocol::Heap::GarbageCollection::Type::Full;
    }
}

void InspectorHeapAgent::willGarbageCollect()
{
    ASSERT(m_enabled);
    ASSERT(std::isnan(m_gcStartTime));

    m_gcStartTime = m_environment.executionStopwatch()->elapsedTime();
}

void InspectorHeapAgent::didGarbageCollect(HeapOperation operation)
{
    ASSERT(m_enabled);
    ASSERT(!std::isnan(m_gcStartTime));

    // FIXME: Include number of bytes freed by collection.

    double startTime = m_gcStartTime;
    double endTime = m_environment.executionStopwatch()->elapsedTime();

    // Dispatch the event asynchronously because this method may be
    // called between collection and sweeping and we don't want to
    // create unexpected JavaScript allocations that the Sweeper does
    // not expect to encounter. JavaScript allocations could happen
    // with WebKitLegacy's in process inspector which shares the same
    // VM as the inspected page.

    RunLoop::current().dispatch([this, startTime, endTime, operation]() {
        auto collection = Inspector::Protocol::Heap::GarbageCollection::create()
            .setType(protocolTypeForHeapOperation(operation))
            .setStartTime(startTime)
            .setEndTime(endTime)
            .release();

        m_frontendDispatcher->garbageCollected(WTFMove(collection));
    });

    m_gcStartTime = NAN;
}

} // namespace Inspector
