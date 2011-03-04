/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef InstrumentingAgents_h
#define InstrumentingAgents_h

#include <wtf/FastAllocBase.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class InspectorApplicationCacheAgent;
class InspectorBrowserDebuggerAgent;
class InspectorConsoleAgent;
class InspectorDOMAgent;
class InspectorDOMStorageAgent;
class InspectorDatabaseAgent;
class InspectorDebuggerAgent;
class InspectorProfilerAgent;
class InspectorResourceAgent;
class InspectorRuntimeAgent;
class InspectorStorageAgent;
class InspectorTimelineAgent;

class InstrumentingAgents {
    WTF_MAKE_NONCOPYABLE(InstrumentingAgents);
    WTF_MAKE_FAST_ALLOCATED;
public:
    InstrumentingAgents()
        : m_inspectorBrowserDebuggerAgent(0)
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        , m_inspectorApplicationCacheAgent(0)
#endif
        , m_inspectorConsoleAgent(0)
        , m_inspectorDOMAgent(0)
        , m_inspectorDOMStorageAgent(0)
        , m_inspectorDatabaseAgent(0)
        , m_inspectorDebuggerAgent(0)
        , m_inspectorProfilerAgent(0)
        , m_inspectorResourceAgent(0)
        , m_inspectorRuntimeAgent(0)
        , m_inspectorStorageAgent(0)
        , m_inspectorTimelineAgent(0)
    { }
    ~InstrumentingAgents() { }

    InspectorBrowserDebuggerAgent* inspectorBrowserDebuggerAgent() const { return m_inspectorBrowserDebuggerAgent; }
    void setInspectorBrowserDebuggerAgent(InspectorBrowserDebuggerAgent* agent) { m_inspectorBrowserDebuggerAgent = agent; }

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    InspectorApplicationCacheAgent* inspectorApplicationCacheAgent() const { return m_inspectorApplicationCacheAgent; }
    void setInspectorApplicationCacheAgent(InspectorApplicationCacheAgent* agent) { m_inspectorApplicationCacheAgent = agent; }
#endif

    InspectorConsoleAgent* inspectorConsoleAgent() const { return m_inspectorConsoleAgent; }
    void setInspectorConsoleAgent(InspectorConsoleAgent* agent) { m_inspectorConsoleAgent = agent; }

    InspectorDOMAgent* inspectorDOMAgent() const { return m_inspectorDOMAgent; }
    void setInspectorDOMAgent(InspectorDOMAgent* agent) { m_inspectorDOMAgent = agent; }

    InspectorDOMStorageAgent* inspectorDOMStorageAgent() const { return m_inspectorDOMStorageAgent; }
    void setInspectorDOMStorageAgent(InspectorDOMStorageAgent* agent) { m_inspectorDOMStorageAgent = agent; }

    InspectorDatabaseAgent* inspectorDatabaseAgent() const { return m_inspectorDatabaseAgent; }
    void setInspectorDatabaseAgent(InspectorDatabaseAgent* agent) { m_inspectorDatabaseAgent = agent; }

    InspectorDebuggerAgent* inspectorDebuggerAgent() const { return m_inspectorDebuggerAgent; }
    void setInspectorDebuggerAgent(InspectorDebuggerAgent* agent) { m_inspectorDebuggerAgent = agent; }

    InspectorProfilerAgent* inspectorProfilerAgent() const { return m_inspectorProfilerAgent; }
    void setInspectorProfilerAgent(InspectorProfilerAgent* agent) { m_inspectorProfilerAgent = agent; }

    InspectorResourceAgent* inspectorResourceAgent() const { return m_inspectorResourceAgent; }
    void setInspectorResourceAgent(InspectorResourceAgent* agent) { m_inspectorResourceAgent = agent; }

    InspectorRuntimeAgent* inspectorRuntimeAgent() const { return m_inspectorRuntimeAgent; }
    void setInspectorRuntimeAgent(InspectorRuntimeAgent* agent) { m_inspectorRuntimeAgent = agent; }

    InspectorStorageAgent* inspectorStorageAgent() const { return m_inspectorStorageAgent; }
    void setInspectorStorageAgent(InspectorStorageAgent* agent) { m_inspectorStorageAgent = agent; }

    InspectorTimelineAgent* inspectorTimelineAgent() const { return m_inspectorTimelineAgent; }
    void setInspectorTimelineAgent(InspectorTimelineAgent* agent) { m_inspectorTimelineAgent = agent; }

private:
    InspectorBrowserDebuggerAgent* m_inspectorBrowserDebuggerAgent;
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    InspectorApplicationCacheAgent* m_inspectorApplicationCacheAgent;
#endif
    InspectorConsoleAgent* m_inspectorConsoleAgent;
    InspectorDOMAgent* m_inspectorDOMAgent;
    InspectorDOMStorageAgent* m_inspectorDOMStorageAgent;
    InspectorDatabaseAgent* m_inspectorDatabaseAgent;
    InspectorDebuggerAgent* m_inspectorDebuggerAgent;
    InspectorProfilerAgent* m_inspectorProfilerAgent;
    InspectorResourceAgent* m_inspectorResourceAgent;
    InspectorRuntimeAgent* m_inspectorRuntimeAgent;
    InspectorStorageAgent* m_inspectorStorageAgent;
    InspectorTimelineAgent* m_inspectorTimelineAgent;
};

}

#endif // !defined(InstrumentingAgents_h)
