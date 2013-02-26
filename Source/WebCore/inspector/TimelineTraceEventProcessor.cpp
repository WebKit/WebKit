/*
* Copyright (C) 2013 Google Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
*     * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
* copyright notice, this list of conditions and the following disclaimer
* in the documentation and/or other materials provided with the
* distribution.
*     * Neither the name of Google Inc. nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"

#if ENABLE(INSPECTOR)

#include "TimelineTraceEventProcessor.h"

#include "InspectorClient.h"
#include "InspectorInstrumentation.h"
#include "InspectorTimelineAgent.h"
#include "TimelineRecordFactory.h"

#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/Vector.h>

namespace WebCore {

namespace {

class TraceEventDispatcher {
    WTF_MAKE_NONCOPYABLE(TraceEventDispatcher);
public:
    static TraceEventDispatcher* instance()
    {
        DEFINE_STATIC_LOCAL(TraceEventDispatcher, instance, ());
        return &instance;
    }

    void addProcessor(TimelineTraceEventProcessor* processor, InspectorClient* client)
    {
        MutexLocker locker(m_mutex);

        m_processors.append(processor);
        if (m_processors.size() == 1)
            client->setTraceEventCallback(dispatchEventOnAnyThread);
    }

    void removeProcessor(TimelineTraceEventProcessor* processor, InspectorClient* client)
    {
        MutexLocker locker(m_mutex);

        size_t index = m_processors.find(processor);
        if (index == notFound) {
            ASSERT_NOT_REACHED();
            return;
        }
        m_processors[index] = m_processors.last();
        m_processors.removeLast();
        if (m_processors.isEmpty())
            client->setTraceEventCallback(0);
    }

private:
    TraceEventDispatcher() { }

    static void dispatchEventOnAnyThread(char phase, const unsigned char*, const char* name, unsigned long long id,
        int numArgs, const char* const* argNames, const unsigned char* argTypes, const unsigned long long* argValues,
        unsigned char flags)
    {
        TraceEventDispatcher* self = instance();
        Vector<RefPtr<TimelineTraceEventProcessor> > processors;
        {
            MutexLocker locker(self->m_mutex);
            processors = self->m_processors;
        }
        for (int i = 0, size = processors.size(); i < size; ++i) {
            processors[i]->processEventOnAnyThread(static_cast<TimelineTraceEventProcessor::TraceEventPhase>(phase),
                name, id, numArgs, argNames, argTypes, argValues, flags);
        }
    }

    Mutex m_mutex;
    Vector<RefPtr<TimelineTraceEventProcessor> > m_processors;
};

} // namespce
TimelineTraceEventProcessor::TimelineTraceEventProcessor(WeakPtr<InspectorTimelineAgent> timelineAgent, InspectorClient *client)
    : m_timelineAgent(timelineAgent)
    , m_inspectorClient(client)
    , m_pageId(reinterpret_cast<unsigned long long>(m_timelineAgent.get()->page()))
{
    TraceEventDispatcher::instance()->addProcessor(this, m_inspectorClient);
}

TimelineTraceEventProcessor::~TimelineTraceEventProcessor()
{
    TraceEventDispatcher::instance()->removeProcessor(this, m_inspectorClient);
}

const TimelineTraceEventProcessor::TraceValueUnion& TimelineTraceEventProcessor::TraceEvent::parameter(const char* name, TraceValueTypes expectedType) const
{
    static TraceValueUnion missingValue;

    for (int i = 0; i < m_argumentCount; ++i) {
        if (!strcmp(name, m_argumentNames[i])) {
            if (m_argumentTypes[i] != expectedType) {
                ASSERT_NOT_REACHED();
                return missingValue;
            }
            return *reinterpret_cast<const TraceValueUnion*>(m_argumentValues + i);
        }
    }
    ASSERT_NOT_REACHED();
    return missingValue;
}

void TimelineTraceEventProcessor::processEventOnAnyThread(TraceEventPhase phase, const char* name, unsigned long long,
    int numArgs, const char* const* argNames, const unsigned char* argTypes, const unsigned long long* argValues,
    unsigned char)
{
    HashMap<String, EventTypeEntry>::iterator it = m_handlersByType.find(name);
    if (it == m_handlersByType.end())
        return;

    TraceEvent event(WTF::monotonicallyIncreasingTime(), phase, name, currentThread(), numArgs, argNames, argTypes, argValues);

    if (!isMainThread()) {
        MutexLocker locker(m_backgroundEventsMutex);
        m_backgroundEvents.append(event);
        return;
    }

    processEvent(it->value, event);
}

void TimelineTraceEventProcessor::processEvent(const EventTypeEntry& eventTypeEntry, const TraceEvent& event)
{
    TraceEventHandler handler = 0;
    switch (event.phase()) {
    case TracePhaseBegin:
        handler = eventTypeEntry.m_begin;
        break;
    case TracePhaseEnd:
        handler = eventTypeEntry.m_end;
        break;
    case TracePhaseInstant:
        handler = eventTypeEntry.m_instant;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    if (!handler) {
        ASSERT_NOT_REACHED();
        return;
    }
    (this->*handler)(event);
}

void TimelineTraceEventProcessor::sendTimelineRecord(PassRefPtr<InspectorObject> data, const String& recordType, double startTime, double endTime, const String& thread)
{
    InspectorTimelineAgent* timelineAgent = m_timelineAgent.get();
    if (!timelineAgent)
        return;
    timelineAgent->appendBackgroundThreadRecord(data, recordType, startTime, endTime, thread);
}

void TimelineTraceEventProcessor::processBackgroundEvents()
{
    Vector<TraceEvent> events;
    {
        MutexLocker locker(m_backgroundEventsMutex);
        events.reserveCapacity(m_backgroundEvents.capacity());
        m_backgroundEvents.swap(events);
    }
    for (size_t i = 0, size = events.size(); i < size; ++i) {
        const TraceEvent& event = events[i];
        processEvent(m_handlersByType.find(event.name())->value, event);
    }
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
