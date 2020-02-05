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

#include "config.h"
#include "InstrumentingAgents.h"


namespace WebCore {

using namespace Inspector;

InstrumentingAgents::InstrumentingAgents(InspectorEnvironment& environment)
    : m_environment(environment)
{
}

void InstrumentingAgents::reset()
{
    m_inspectorAgent = nullptr;
    m_inspectorPageAgent = nullptr;
    m_inspectorCSSAgent = nullptr;
    m_inspectorLayerTreeAgent = nullptr;
    m_inspectorWorkerAgent = nullptr;
    m_webConsoleAgent = nullptr;
    m_inspectorDOMAgent = nullptr;
    m_inspectorNetworkAgent = nullptr;
    m_pageRuntimeAgent = nullptr;
    m_inspectorScriptProfilerAgent = nullptr;
    m_inspectorTimelineAgent = nullptr;
    m_trackingInspectorTimelineAgent = nullptr;
    m_inspectorDOMStorageAgent = nullptr;
#if ENABLE(RESOURCE_USAGE)
    m_inspectorCPUProfilerAgent = nullptr;
    m_inspectorMemoryAgent = nullptr;
#endif
    m_inspectorDatabaseAgent = nullptr;
    m_inspectorApplicationCacheAgent = nullptr;
    m_webDebuggerAgent = nullptr;
    m_pageDebuggerAgent = nullptr;
    m_pageHeapAgent = nullptr;
    m_inspectorDOMDebuggerAgent = nullptr;
    m_pageDOMDebuggerAgent = nullptr;
    m_inspectorCanvasAgent = nullptr;
    m_persistentInspectorAnimationAgent = nullptr;
    m_enabledInspectorAnimationAgent = nullptr;
    m_trackingInspectorAnimationAgent = nullptr;
}

} // namespace WebCore
