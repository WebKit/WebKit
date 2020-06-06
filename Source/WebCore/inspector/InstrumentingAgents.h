/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

#pragma once

#include <JavaScriptCore/InspectorEnvironment.h>
#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>

namespace Inspector {
class InspectorAgent;
class InspectorScriptProfilerAgent;
}

namespace WebCore {

class InspectorAnimationAgent;
class InspectorApplicationCacheAgent;
class InspectorCPUProfilerAgent;
class InspectorCSSAgent;
class InspectorCanvasAgent;
class InspectorDOMAgent;
class InspectorDOMDebuggerAgent;
class InspectorDOMStorageAgent;
class InspectorDatabaseAgent;
class InspectorLayerTreeAgent;
class InspectorMemoryAgent;
class InspectorNetworkAgent;
class InspectorPageAgent;
class InspectorTimelineAgent;
class InspectorWorkerAgent;
class PageDOMDebuggerAgent;
class PageDebuggerAgent;
class PageHeapAgent;
class PageRuntimeAgent;
class WebConsoleAgent;
class WebDebuggerAgent;

#define DEFINE_INSPECTOR_AGENT(macro, Class, Name, Getter, Setter) macro(Class, Name, Getter, Setter)

#define DEFINE_INSPECTOR_AGENT_Animation(macro, Getter, Setter) DEFINE_INSPECTOR_AGENT(macro, InspectorAnimationAgent, AnimationAgent, Getter, Setter)
#define DEFINE_INSPECTOR_AGENT_ApplicationCache(macro, Getter, Setter) DEFINE_INSPECTOR_AGENT(macro, InspectorApplicationCacheAgent, ApplicationCacheAgent, Getter, Setter)
#define DEFINE_INSPECTOR_AGENT_Canvas(macro, Getter, Setter) DEFINE_INSPECTOR_AGENT(macro, InspectorCanvasAgent, CanvasAgent, Getter, Setter)
#define DEFINE_INSPECTOR_AGENT_CSS(macro, Getter, Setter) DEFINE_INSPECTOR_AGENT(macro, InspectorCSSAgent, CSSAgent, Getter, Setter)
#define DEFINE_INSPECTOR_AGENT_DOM(macro, Getter, Setter) DEFINE_INSPECTOR_AGENT(macro, InspectorDOMAgent, DOMAgent, Getter, Setter)
#define DEFINE_INSPECTOR_AGENT_DOMDebugger(macro, Getter, Setter) DEFINE_INSPECTOR_AGENT(macro, InspectorDOMDebuggerAgent, DOMDebuggerAgent, Getter, Setter)
#define DEFINE_INSPECTOR_AGENT_DOMDebugger_Page(macro, Getter, Setter) DEFINE_INSPECTOR_AGENT(macro, PageDOMDebuggerAgent, PageDOMDebuggerAgent, Getter, Setter)
#define DEFINE_INSPECTOR_AGENT_DOMStorage(macro, Getter, Setter) DEFINE_INSPECTOR_AGENT(macro, InspectorDOMStorageAgent, DOMStorageAgent, Getter, Setter)
#define DEFINE_INSPECTOR_AGENT_Database(macro, Getter, Setter) DEFINE_INSPECTOR_AGENT(macro, InspectorDatabaseAgent, DatabaseAgent, Getter, Setter)
#define DEFINE_INSPECTOR_AGENT_Debugger_Web(macro, Getter, Setter) DEFINE_INSPECTOR_AGENT(macro, WebDebuggerAgent, WebDebuggerAgent, Getter, Setter)
#define DEFINE_INSPECTOR_AGENT_Debugger_Page(macro, Getter, Setter) DEFINE_INSPECTOR_AGENT(macro, PageDebuggerAgent, PageDebuggerAgent, Getter, Setter)
#define DEFINE_INSPECTOR_AGENT_Heap_Page(macro, Getter, Setter) DEFINE_INSPECTOR_AGENT(macro, PageHeapAgent, PageHeapAgent, Getter, Setter)
#define DEFINE_INSPECTOR_AGENT_Inspector(macro, Getter, Setter) DEFINE_INSPECTOR_AGENT(macro, Inspector::InspectorAgent, InspectorAgent, Getter, Setter)
#define DEFINE_INSPECTOR_AGENT_LayerTree(macro, Getter, Setter) DEFINE_INSPECTOR_AGENT(macro, InspectorLayerTreeAgent, LayerTreeAgent, Getter, Setter)
#define DEFINE_INSPECTOR_AGENT_Network(macro, Getter, Setter) DEFINE_INSPECTOR_AGENT(macro, InspectorNetworkAgent, NetworkAgent, Getter, Setter)
#define DEFINE_INSPECTOR_AGENT_Page(macro, Getter, Setter) DEFINE_INSPECTOR_AGENT(macro, InspectorPageAgent, PageAgent, Getter, Setter)
#define DEFINE_INSPECTOR_AGENT_Runtime_Page(macro, Getter, Setter) DEFINE_INSPECTOR_AGENT(macro, PageRuntimeAgent, PageRuntimeAgent, Getter, Setter)
#define DEFINE_INSPECTOR_AGENT_ScriptProfiler(macro, Getter, Setter) DEFINE_INSPECTOR_AGENT(macro, Inspector::InspectorScriptProfilerAgent, ScriptProfilerAgent, Getter, Setter)
#define DEFINE_INSPECTOR_AGENT_Timeline(macro, Getter, Setter) DEFINE_INSPECTOR_AGENT(macro, InspectorTimelineAgent, TimelineAgent, Getter, Setter)
#define DEFINE_INSPECTOR_AGENT_Worker(macro, Getter, Setter) DEFINE_INSPECTOR_AGENT(macro, InspectorWorkerAgent, WorkerAgent, Getter, Setter)

#if ENABLE(RESOURCE_USAGE)
#define DEFINE_INSPECTOR_AGENT_CPUProfiler(macro, Getter, Setter) DEFINE_INSPECTOR_AGENT(macro, InspectorCPUProfilerAgent, CPUProfilerAgent, Getter, Setter)
#define DEFINE_INSPECTOR_AGENT_Memory(macro, Getter, Setter) DEFINE_INSPECTOR_AGENT(macro, InspectorMemoryAgent, MemoryAgent, Getter, Setter)
#else
#define DEFINE_INSPECTOR_AGENT_CPUProfiler(macro, Getter, Setter)
#define DEFINE_INSPECTOR_AGENT_Memory(macro, Getter, Setter)
#endif

// Set when Web Inspector is connected
#define DEFINE_PERSISTENT_INSPECTOR_AGENT(macro, Agent) DEFINE_INSPECTOR_AGENT_##Agent(macro, persistent, Persistent)

// Set when `enable`d, such as if the corresponding tab is visible
#define DEFINE_ENABLED_INSPECTOR_AGENT(macro, Agent) DEFINE_INSPECTOR_AGENT_##Agent(macro, enabled, Enabled)

// Set when part of a timeline recording.
#define DEFINE_TRACKING_INSPECTOR_AGENT(macro, Agent) DEFINE_INSPECTOR_AGENT_##Agent(macro, tracking, Tracking)

#define FOR_EACH_INSPECTOR_AGENT(macro) \
    DEFINE_INSPECTOR_AGENT(macro, WebConsoleAgent, ConsoleAgent, web, Web) \
    DEFINE_PERSISTENT_INSPECTOR_AGENT(macro, Animation) \
    DEFINE_PERSISTENT_INSPECTOR_AGENT(macro, CPUProfiler) \
    DEFINE_PERSISTENT_INSPECTOR_AGENT(macro, DOM) \
    DEFINE_PERSISTENT_INSPECTOR_AGENT(macro, Inspector) \
    DEFINE_PERSISTENT_INSPECTOR_AGENT(macro, Memory) \
    DEFINE_PERSISTENT_INSPECTOR_AGENT(macro, ScriptProfiler) \
    DEFINE_PERSISTENT_INSPECTOR_AGENT(macro, Worker) \
    DEFINE_ENABLED_INSPECTOR_AGENT(macro, Animation) \
    DEFINE_ENABLED_INSPECTOR_AGENT(macro, ApplicationCache) \
    DEFINE_ENABLED_INSPECTOR_AGENT(macro, Canvas) \
    DEFINE_ENABLED_INSPECTOR_AGENT(macro, CSS) \
    DEFINE_ENABLED_INSPECTOR_AGENT(macro, Database) \
    DEFINE_ENABLED_INSPECTOR_AGENT(macro, Debugger_Page) \
    DEFINE_ENABLED_INSPECTOR_AGENT(macro, Debugger_Web) \
    DEFINE_ENABLED_INSPECTOR_AGENT(macro, DOMDebugger) \
    DEFINE_ENABLED_INSPECTOR_AGENT(macro, DOMDebugger_Page) \
    DEFINE_ENABLED_INSPECTOR_AGENT(macro, DOMStorage) \
    DEFINE_ENABLED_INSPECTOR_AGENT(macro, Heap_Page) \
    DEFINE_ENABLED_INSPECTOR_AGENT(macro, LayerTree) \
    DEFINE_ENABLED_INSPECTOR_AGENT(macro, Memory) \
    DEFINE_ENABLED_INSPECTOR_AGENT(macro, Network) \
    DEFINE_ENABLED_INSPECTOR_AGENT(macro, Page) \
    DEFINE_ENABLED_INSPECTOR_AGENT(macro, Runtime_Page) \
    DEFINE_ENABLED_INSPECTOR_AGENT(macro, Timeline) \
    DEFINE_TRACKING_INSPECTOR_AGENT(macro, Animation) \
    DEFINE_TRACKING_INSPECTOR_AGENT(macro, Timeline) \

class InstrumentingAgents : public RefCounted<InstrumentingAgents> {
    WTF_MAKE_NONCOPYABLE(InstrumentingAgents);
    WTF_MAKE_FAST_ALLOCATED;
public:
    // FIXME: InstrumentingAgents could be uniquely owned by InspectorController if instrumentation
    // cookies kept only a weak reference to InstrumentingAgents. Then, reset() would be unnecessary.
    static Ref<InstrumentingAgents> create(Inspector::InspectorEnvironment& environment)
    {
        return adoptRef(*new InstrumentingAgents(environment));
    }
    ~InstrumentingAgents() = default;
    void reset();

    Inspector::InspectorEnvironment& inspectorEnvironment() const { return m_environment; }

#define DECLARE_GETTER_SETTER_FOR_INSPECTOR_AGENT(Class, Name, Getter, Setter) \
    Class* Getter##Name() const { return m_##Getter##Name; } \
    void set##Setter##Name(Class* agent) { m_##Getter##Name = agent; } \

FOR_EACH_INSPECTOR_AGENT(DECLARE_GETTER_SETTER_FOR_INSPECTOR_AGENT)
#undef DECLARE_GETTER_SETTER_FOR_INSPECTOR_AGENT

private:
    InstrumentingAgents(Inspector::InspectorEnvironment&);

    Inspector::InspectorEnvironment& m_environment;

#define DECLARE_MEMBER_VARIABLE_FOR_INSPECTOR_AGENT(Class, Name, Getter, Setter) \
    Class* m_##Getter##Name { nullptr }; \

FOR_EACH_INSPECTOR_AGENT(DECLARE_MEMBER_VARIABLE_FOR_INSPECTOR_AGENT)
#undef DECLARE_MEMBER_VARIABLE_FOR_INSPECTOR_AGENT
};

} // namespace WebCore
