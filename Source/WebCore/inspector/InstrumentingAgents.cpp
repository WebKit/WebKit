/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if ENABLE(INSPECTOR)

#include "InspectorController.h"
#include "Page.h"
#include "WorkerGlobalScope.h"
#include "WorkerInspectorController.h"
#include <wtf/MainThread.h>

using namespace Inspector;

namespace WebCore {

InstrumentingAgents::InstrumentingAgents(InspectorEnvironment& environment)
    : m_environment(environment)
    , m_inspectorAgent(nullptr)
    , m_inspectorPageAgent(nullptr)
    , m_inspectorCSSAgent(nullptr)
    , m_inspectorLayerTreeAgent(nullptr)
    , m_webConsoleAgent(nullptr)
    , m_inspectorDOMAgent(nullptr)
    , m_inspectorResourceAgent(nullptr)
    , m_pageRuntimeAgent(nullptr)
    , m_workerRuntimeAgent(nullptr)
    , m_inspectorTimelineAgent(nullptr)
    , m_persistentInspectorTimelineAgent(nullptr)
    , m_inspectorDOMStorageAgent(nullptr)
#if ENABLE(WEB_REPLAY)
    , m_inspectorReplayAgent(nullptr)
#endif
#if ENABLE(SQL_DATABASE)
    , m_inspectorDatabaseAgent(nullptr)
#endif
    , m_inspectorApplicationCacheAgent(nullptr)
    , m_inspectorDebuggerAgent(nullptr)
    , m_pageDebuggerAgent(nullptr)
    , m_inspectorDOMDebuggerAgent(nullptr)
    , m_inspectorProfilerAgent(nullptr)
    , m_inspectorWorkerAgent(nullptr)
{
}

void InstrumentingAgents::reset()
{
    m_inspectorAgent = nullptr;
    m_inspectorPageAgent = nullptr;
    m_inspectorCSSAgent = nullptr;
    m_inspectorLayerTreeAgent = nullptr;
    m_webConsoleAgent = nullptr;
    m_inspectorDOMAgent = nullptr;
    m_inspectorResourceAgent = nullptr;
    m_pageRuntimeAgent = nullptr;
    m_workerRuntimeAgent = nullptr;
    m_inspectorTimelineAgent = nullptr;
    m_persistentInspectorTimelineAgent = nullptr;
    m_inspectorDOMStorageAgent = nullptr;
#if ENABLE(WEB_REPLAY)
    m_inspectorReplayAgent = nullptr;
#endif
#if ENABLE(SQL_DATABASE)
    m_inspectorDatabaseAgent = nullptr;
#endif
    m_inspectorApplicationCacheAgent = nullptr;
    m_inspectorDebuggerAgent = nullptr;
    m_pageDebuggerAgent = nullptr;
    m_inspectorDOMDebuggerAgent = nullptr;
    m_inspectorProfilerAgent = nullptr;
    m_inspectorWorkerAgent = nullptr;
}

InstrumentingAgents* instrumentationForPage(Page* page)
{
    ASSERT(isMainThread());
    return page ? page->inspectorController().m_instrumentingAgents.get() : nullptr;
}

InstrumentingAgents* instrumentationForWorkerGlobalScope(WorkerGlobalScope* workerGlobalScope)
{
    return workerGlobalScope ? workerGlobalScope->workerInspectorController().m_instrumentingAgents.get() : nullptr;
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
