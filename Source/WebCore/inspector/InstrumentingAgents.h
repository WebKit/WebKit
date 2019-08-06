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
class InspectorDebuggerAgent;
class InspectorScriptProfilerAgent;
}

namespace WebCore {

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
class Page;
class PageDebuggerAgent;
class PageHeapAgent;
class PageRuntimeAgent;
class WebConsoleAgent;

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

    Inspector::InspectorAgent* inspectorAgent() const { return m_inspectorAgent; }
    void setInspectorAgent(Inspector::InspectorAgent* agent) { m_inspectorAgent = agent; }

    InspectorPageAgent* inspectorPageAgent() const { return m_inspectorPageAgent; }
    void setInspectorPageAgent(InspectorPageAgent* agent) { m_inspectorPageAgent = agent; }

    InspectorCanvasAgent* inspectorCanvasAgent() const { return m_inspectorCanvasAgent; }
    void setInspectorCanvasAgent(InspectorCanvasAgent* agent) { m_inspectorCanvasAgent = agent; }

    InspectorCSSAgent* inspectorCSSAgent() const { return m_inspectorCSSAgent; }
    void setInspectorCSSAgent(InspectorCSSAgent* agent) { m_inspectorCSSAgent = agent; }

    WebConsoleAgent* webConsoleAgent() const { return m_webConsoleAgent; }
    void setWebConsoleAgent(WebConsoleAgent* agent) { m_webConsoleAgent = agent; }

    InspectorDOMAgent* inspectorDOMAgent() const { return m_inspectorDOMAgent; }
    void setInspectorDOMAgent(InspectorDOMAgent* agent) { m_inspectorDOMAgent = agent; }

    InspectorNetworkAgent* inspectorNetworkAgent() const { return m_inspectorNetworkAgent; }
    void setInspectorNetworkAgent(InspectorNetworkAgent* agent) { m_inspectorNetworkAgent = agent; }

    PageRuntimeAgent* pageRuntimeAgent() const { return m_pageRuntimeAgent; }
    void setPageRuntimeAgent(PageRuntimeAgent* agent) { m_pageRuntimeAgent = agent; }

    Inspector::InspectorScriptProfilerAgent* inspectorScriptProfilerAgent() const { return m_inspectorScriptProfilerAgent; }
    void setInspectorScriptProfilerAgent(Inspector::InspectorScriptProfilerAgent* agent) { m_inspectorScriptProfilerAgent = agent; }

    InspectorTimelineAgent* inspectorTimelineAgent() const { return m_inspectorTimelineAgent; }
    void setInspectorTimelineAgent(InspectorTimelineAgent* agent) { m_inspectorTimelineAgent = agent; }

    InspectorTimelineAgent* trackingInspectorTimelineAgent() const { return m_trackingInspectorTimelineAgent; }
    void setTrackingInspectorTimelineAgent(InspectorTimelineAgent* agent) { m_trackingInspectorTimelineAgent = agent; }

    InspectorDOMStorageAgent* inspectorDOMStorageAgent() const { return m_inspectorDOMStorageAgent; }
    void setInspectorDOMStorageAgent(InspectorDOMStorageAgent* agent) { m_inspectorDOMStorageAgent = agent; }

#if ENABLE(RESOURCE_USAGE)
    InspectorCPUProfilerAgent* inspectorCPUProfilerAgent() const { return m_inspectorCPUProfilerAgent; }
    void setInspectorCPUProfilerAgent(InspectorCPUProfilerAgent* agent) { m_inspectorCPUProfilerAgent = agent; }

    InspectorMemoryAgent* inspectorMemoryAgent() const { return m_inspectorMemoryAgent; }
    void setInspectorMemoryAgent(InspectorMemoryAgent* agent) { m_inspectorMemoryAgent = agent; }
#endif

    InspectorDatabaseAgent* inspectorDatabaseAgent() const { return m_inspectorDatabaseAgent; }
    void setInspectorDatabaseAgent(InspectorDatabaseAgent* agent) { m_inspectorDatabaseAgent = agent; }

    InspectorApplicationCacheAgent* inspectorApplicationCacheAgent() const { return m_inspectorApplicationCacheAgent; }
    void setInspectorApplicationCacheAgent(InspectorApplicationCacheAgent* agent) { m_inspectorApplicationCacheAgent = agent; }

    Inspector::InspectorDebuggerAgent* inspectorDebuggerAgent() const { return m_inspectorDebuggerAgent; }
    void setInspectorDebuggerAgent(Inspector::InspectorDebuggerAgent* agent) { m_inspectorDebuggerAgent = agent; }

    PageDebuggerAgent* pageDebuggerAgent() const { return m_pageDebuggerAgent; }
    void setPageDebuggerAgent(PageDebuggerAgent* agent) { m_pageDebuggerAgent = agent; }

    PageHeapAgent* pageHeapAgent() const { return m_pageHeapAgent; }
    void setPageHeapAgent(PageHeapAgent* agent) { m_pageHeapAgent = agent; }

    InspectorDOMDebuggerAgent* inspectorDOMDebuggerAgent() const { return m_inspectorDOMDebuggerAgent; }
    void setInspectorDOMDebuggerAgent(InspectorDOMDebuggerAgent* agent) { m_inspectorDOMDebuggerAgent = agent; }

    InspectorLayerTreeAgent* inspectorLayerTreeAgent() const { return m_inspectorLayerTreeAgent; }
    void setInspectorLayerTreeAgent(InspectorLayerTreeAgent* agent) { m_inspectorLayerTreeAgent = agent; }

    InspectorWorkerAgent* inspectorWorkerAgent() const { return m_inspectorWorkerAgent; }
    void setInspectorWorkerAgent(InspectorWorkerAgent* agent) { m_inspectorWorkerAgent = agent; }

private:
    InstrumentingAgents(Inspector::InspectorEnvironment&);

    Inspector::InspectorEnvironment& m_environment;

    Inspector::InspectorAgent* m_inspectorAgent { nullptr };
    InspectorPageAgent* m_inspectorPageAgent { nullptr };
    InspectorCSSAgent* m_inspectorCSSAgent { nullptr };
    InspectorLayerTreeAgent* m_inspectorLayerTreeAgent { nullptr };
    InspectorWorkerAgent* m_inspectorWorkerAgent { nullptr };
    WebConsoleAgent* m_webConsoleAgent { nullptr };
    InspectorDOMAgent* m_inspectorDOMAgent { nullptr };
    InspectorNetworkAgent* m_inspectorNetworkAgent { nullptr };
    PageRuntimeAgent* m_pageRuntimeAgent { nullptr };
    Inspector::InspectorScriptProfilerAgent* m_inspectorScriptProfilerAgent { nullptr };
    InspectorTimelineAgent* m_inspectorTimelineAgent { nullptr };
    InspectorTimelineAgent* m_trackingInspectorTimelineAgent { nullptr };
    InspectorDOMStorageAgent* m_inspectorDOMStorageAgent { nullptr };
#if ENABLE(RESOURCE_USAGE)
    InspectorCPUProfilerAgent* m_inspectorCPUProfilerAgent { nullptr };
    InspectorMemoryAgent* m_inspectorMemoryAgent { nullptr };
#endif
    InspectorDatabaseAgent* m_inspectorDatabaseAgent { nullptr };
    InspectorApplicationCacheAgent* m_inspectorApplicationCacheAgent { nullptr };
    Inspector::InspectorDebuggerAgent* m_inspectorDebuggerAgent { nullptr };
    PageDebuggerAgent* m_pageDebuggerAgent { nullptr };
    PageHeapAgent* m_pageHeapAgent { nullptr };
    InspectorDOMDebuggerAgent* m_inspectorDOMDebuggerAgent { nullptr };
    InspectorCanvasAgent* m_inspectorCanvasAgent { nullptr };
};

} // namespace WebCore
