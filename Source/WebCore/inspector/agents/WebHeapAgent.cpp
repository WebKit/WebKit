/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include "WebHeapAgent.h"

#include "InstrumentingAgents.h"
#include "WebConsoleAgent.h"
#include <wtf/RunLoop.h>

namespace WebCore {

using namespace Inspector;

struct GarbageCollectionData {
    Inspector::Protocol::Heap::GarbageCollection::Type type;
    Seconds startTime;
    Seconds endTime;
};

class SendGarbageCollectionEventsTask final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SendGarbageCollectionEventsTask(WebHeapAgent&);
    void addGarbageCollection(GarbageCollectionData&&);
    void reset();
private:
    void timerFired();

    WebHeapAgent& m_agent;
    Vector<GarbageCollectionData> m_collections;
    RunLoop::Timer<SendGarbageCollectionEventsTask> m_timer;
    Lock m_mutex;
};

SendGarbageCollectionEventsTask::SendGarbageCollectionEventsTask(WebHeapAgent& agent)
    : m_agent(agent)
    , m_timer(RunLoop::main(), this, &SendGarbageCollectionEventsTask::timerFired)
{
}

void SendGarbageCollectionEventsTask::addGarbageCollection(GarbageCollectionData&& collection)
{
    {
        auto locker = holdLock(m_mutex);
        m_collections.append(WTFMove(collection));
    }

    if (!m_timer.isActive())
        m_timer.startOneShot(0_s);
}

void SendGarbageCollectionEventsTask::reset()
{
    {
        auto locker = holdLock(m_mutex);
        m_collections.clear();
    }

    m_timer.stop();
}

void SendGarbageCollectionEventsTask::timerFired()
{
    Vector<GarbageCollectionData> collectionsToSend;

    {
        auto locker = holdLock(m_mutex);
        m_collections.swap(collectionsToSend);
    }

    m_agent.dispatchGarbageCollectionEventsAfterDelay(WTFMove(collectionsToSend));
}

WebHeapAgent::WebHeapAgent(WebAgentContext& context)
    : InspectorHeapAgent(context)
    , m_instrumentingAgents(context.instrumentingAgents)
    , m_sendGarbageCollectionEventsTask(makeUnique<SendGarbageCollectionEventsTask>(*this))
{
}

WebHeapAgent::~WebHeapAgent() = default;

void WebHeapAgent::enable(ErrorString& errorString)
{
    InspectorHeapAgent::enable(errorString);

    if (auto* consoleAgent = m_instrumentingAgents.webConsoleAgent())
        consoleAgent->setHeapAgent(this);
}

void WebHeapAgent::disable(ErrorString& errorString)
{
    m_sendGarbageCollectionEventsTask->reset();

    if (auto* consoleAgent = m_instrumentingAgents.webConsoleAgent())
        consoleAgent->setHeapAgent(nullptr);

    InspectorHeapAgent::disable(errorString);
}

void WebHeapAgent::dispatchGarbageCollectedEvent(Inspector::Protocol::Heap::GarbageCollection::Type type, Seconds startTime, Seconds endTime)
{
    // Dispatch the event asynchronously because this method may be
    // called between collection and sweeping and we don't want to
    // create unexpected JavaScript allocations that the Sweeper does
    // not expect to encounter. JavaScript allocations could happen
    // with WebKitLegacy's in process inspector which shares the same
    // VM as the inspected page.

    GarbageCollectionData data = {type, startTime, endTime};
    m_sendGarbageCollectionEventsTask->addGarbageCollection(WTFMove(data));
}

void WebHeapAgent::dispatchGarbageCollectionEventsAfterDelay(Vector<GarbageCollectionData>&& collections)
{
    for (auto& collection : collections)
        InspectorHeapAgent::dispatchGarbageCollectedEvent(collection.type, collection.startTime, collection.endTime);
}

} // namespace WebCore
