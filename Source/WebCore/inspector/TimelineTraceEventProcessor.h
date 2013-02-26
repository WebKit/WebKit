/*
* Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef TimelineTraceEventProcessor_h
#define TimelineTraceEventProcessor_h

#if ENABLE(INSPECTOR)

#include "InspectorValues.h"

#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class InspectorClient;
class InspectorTimelineAgent;
class Page;

class TimelineTraceEventProcessor : public ThreadSafeRefCounted<TimelineTraceEventProcessor> {
public:
    // FIXME: re-use definitions in TraceEvent.h once it's promoted to all platforms.
    enum TraceEventPhase {
        TracePhaseBegin = 'B',
        TracePhaseEnd = 'E',
        TracePhaseInstant = 'I'
    };

    TimelineTraceEventProcessor(WeakPtr<InspectorTimelineAgent>, InspectorClient*);
    ~TimelineTraceEventProcessor();

    void processEventOnAnyThread(TraceEventPhase, const char* name, unsigned long long id,
        int numArgs, const char* const* argNames, const unsigned char* argTypes, const unsigned long long* argValues,
        unsigned char flags);
    
private:
    // FIXME: use the definition in TraceEvent.h once we expose the latter to all plaforms.
    union TraceValueUnion {
        bool m_bool;
        unsigned long long m_uint;
        long long m_int;
        double m_double;
        const void* m_pointer;
        const char* m_string;
    };

    enum TraceValueTypes {
        TypeBool = 1,
        TypeUInt = 2,
        TypeInt = 3,
        TypeDouble = 4,
        TypePointer = 5,
        TypeString = 6,
        TypeCopyString = 7
    };

    class TraceEvent {
    public:
        TraceEvent()
            : m_argumentCount(0)
        {
        }

        TraceEvent(double timestamp, TraceEventPhase phase, const char* name, ThreadIdentifier threadIdentifier,
            int argumentCount, const char* const* argumentNames, const unsigned char* argumentTypes, const unsigned long long* argumentValues)
            : m_timestamp(timestamp)
            , m_phase(phase)
            , m_name(name)
            , m_threadIdentifier(threadIdentifier)
            , m_argumentCount(argumentCount)
            , m_argumentNames(argumentNames)
            , m_argumentTypes(argumentTypes)
            , m_argumentValues(argumentValues)
        {
        }

        double timestamp() const { return m_timestamp; }
        TraceEventPhase phase() const { return m_phase; }
        const char* name() const { return m_name; }
        ThreadIdentifier threadIdentifier() const { return m_threadIdentifier; }
        int argumentCount() const { return m_argumentCount; }

        bool asBool(const char* name) const
        { 
            return parameter(name, TypeBool).m_bool;
        }
        long long asInt(const char* name) const
        { 
            return parameter(name, TypeInt).m_int;
        }
        unsigned long long asUInt(const char* name) const 
        { 
            return parameter(name, TypeUInt).m_uint;
        }
        double asDouble(const char* name) const
        { 
            return parameter(name, TypeDouble).m_double;
        }
        const char* asString(const char* name) const
        {
            return parameter(name, TypeString).m_string;
        }

    private:
        const TraceValueUnion& parameter(const char* name, TraceValueTypes expectedType) const;

        double m_timestamp;
        TraceEventPhase m_phase;
        const char* m_name;
        ThreadIdentifier m_threadIdentifier;
        int m_argumentCount;
        const char* const* m_argumentNames;
        const unsigned char* m_argumentTypes;
        const unsigned long long* m_argumentValues;
    };

    typedef void (TimelineTraceEventProcessor::*TraceEventHandler)(const TraceEvent&);

    struct EventTypeEntry {
        EventTypeEntry()
            : m_begin(0)
            , m_end(0)
            , m_instant(0)
        {
        }
        explicit EventTypeEntry(TraceEventHandler instant)
            : m_begin(0)
            , m_end(0)
            , m_instant(instant)
        {
        }
        EventTypeEntry(TraceEventHandler begin, TraceEventHandler end)
            : m_begin(begin)
            , m_end(end)
            , m_instant(0)
        {
        }

        TraceEventHandler m_begin;
        TraceEventHandler m_end;
        TraceEventHandler m_instant;
    };

    void processBackgroundEvents();
    void sendTimelineRecord(PassRefPtr<InspectorObject> data, const String& recordType, double startTime, double endTime, const String& thread);
    void processEvent(const EventTypeEntry&, const TraceEvent&);

    WeakPtr<InspectorTimelineAgent> m_timelineAgent;
    InspectorClient* m_inspectorClient;

    HashMap<String, EventTypeEntry> m_handlersByType;
    Mutex m_backgroundEventsMutex;
    Vector<TraceEvent> m_backgroundEvents;
    unsigned long long m_pageId;
};


} // namespace WebCore

#endif // ENABLE(INSPECTOR)
#endif // !defined(TimelineTraceEventProcessor_h)
