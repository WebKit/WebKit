/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "InspectorCPUProfilerAgent.h"

#if ENABLE(RESOURCE_USAGE)

#include "InstrumentingAgents.h"
#include "ResourceUsageThread.h"
#include <JavaScriptCore/InspectorEnvironment.h>
#include <wtf/Stopwatch.h>

namespace WebCore {

using namespace Inspector;

InspectorCPUProfilerAgent::InspectorCPUProfilerAgent(PageAgentContext& context)
    : InspectorAgentBase("CPUProfiler"_s, context)
    , m_frontendDispatcher(makeUnique<Inspector::CPUProfilerFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::CPUProfilerBackendDispatcher::create(context.backendDispatcher, this))
{
}

InspectorCPUProfilerAgent::~InspectorCPUProfilerAgent() = default;

void InspectorCPUProfilerAgent::didCreateFrontendAndBackend(FrontendRouter*, BackendDispatcher*)
{
    m_instrumentingAgents.setInspectorCPUProfilerAgent(this);
}

void InspectorCPUProfilerAgent::willDestroyFrontendAndBackend(DisconnectReason)
{
    ErrorString ignored;
    stopTracking(ignored);

    m_instrumentingAgents.setInspectorCPUProfilerAgent(nullptr);
}

void InspectorCPUProfilerAgent::startTracking(ErrorString&)
{
    if (m_tracking)
        return;

    ResourceUsageThread::addObserver(this, CPU, [this] (const ResourceUsageData& data) {
        collectSample(data);
    });

    m_tracking = true;

    m_frontendDispatcher->trackingStart(m_environment.executionStopwatch()->elapsedTime().seconds());
}

void InspectorCPUProfilerAgent::stopTracking(ErrorString&)
{
    if (!m_tracking)
        return;

    ResourceUsageThread::removeObserver(this);

    m_tracking = false;

    m_frontendDispatcher->trackingComplete(m_environment.executionStopwatch()->elapsedTime().seconds());
}

static Ref<Protocol::CPUProfiler::ThreadInfo> buildThreadInfo(const ThreadCPUInfo& thread)
{
    ASSERT(thread.cpu <= 100);

    auto threadInfo = Protocol::CPUProfiler::ThreadInfo::create()
        .setName(thread.name)
        .setUsage(thread.cpu)
        .release();

    if (thread.type == ThreadCPUInfo::Type::Main)
        threadInfo->setType(Protocol::CPUProfiler::ThreadInfo::Type::Main);
    else if (thread.type == ThreadCPUInfo::Type::WebKit)
        threadInfo->setType(Protocol::CPUProfiler::ThreadInfo::Type::WebKit);

    if (!thread.identifier.isEmpty())
        threadInfo->setTargetId(thread.identifier);

    return threadInfo;
}

void InspectorCPUProfilerAgent::collectSample(const ResourceUsageData& data)
{
    auto event = Protocol::CPUProfiler::Event::create()
        .setTimestamp(m_environment.executionStopwatch()->elapsedTimeSince(data.timestamp).seconds())
        .setUsage(data.cpuExcludingDebuggerThreads)
        .release();

    if (!data.cpuThreads.isEmpty()) {
        RefPtr<JSON::ArrayOf<Protocol::CPUProfiler::ThreadInfo>> threads = JSON::ArrayOf<Protocol::CPUProfiler::ThreadInfo>::create();
        for (auto& threadInfo : data.cpuThreads)
            threads->addItem(buildThreadInfo(threadInfo));
        event->setThreads(WTFMove(threads));
    }

    m_frontendDispatcher->trackingUpdate(WTFMove(event));
}

} // namespace WebCore

#endif // ENABLE(RESOURCE_USAGE)
