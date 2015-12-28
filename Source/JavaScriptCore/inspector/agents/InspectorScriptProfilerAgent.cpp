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
#include "InspectorScriptProfilerAgent.h"

#include "InspectorEnvironment.h"
#include "LegacyProfiler.h"
#include <wtf/RunLoop.h>
#include <wtf/Stopwatch.h>

using namespace JSC;

namespace Inspector {

InspectorScriptProfilerAgent::InspectorScriptProfilerAgent(AgentContext& context)
    : InspectorAgentBase(ASCIILiteral("ScriptProfiler"))
    , m_frontendDispatcher(std::make_unique<ScriptProfilerFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(ScriptProfilerBackendDispatcher::create(context.backendDispatcher, this))
    , m_environment(context.environment)
{
}

InspectorScriptProfilerAgent::~InspectorScriptProfilerAgent()
{
}

void InspectorScriptProfilerAgent::didCreateFrontendAndBackend(FrontendRouter*, BackendDispatcher*)
{
}

void InspectorScriptProfilerAgent::willDestroyFrontendAndBackend(DisconnectReason)
{
    ErrorString ignored;
    stopTracking(ignored);
}

void InspectorScriptProfilerAgent::startTracking(ErrorString&, const bool* profile)
{
    if (m_tracking)
        return;

    m_tracking = true;

    if (profile && *profile)
        m_enableLegacyProfiler = true;

    m_environment.scriptDebugServer().setProfilingClient(this);

    m_frontendDispatcher->trackingStart(m_environment.executionStopwatch()->elapsedTime());
}

void InspectorScriptProfilerAgent::stopTracking(ErrorString&)
{
    if (!m_tracking)
        return;

    m_tracking = false;
    m_enableLegacyProfiler = false;
    m_activeEvaluateScript = false;

    m_environment.scriptDebugServer().setProfilingClient(nullptr);

    trackingComplete();
}

bool InspectorScriptProfilerAgent::isAlreadyProfiling() const
{
    return m_activeEvaluateScript;
}

double InspectorScriptProfilerAgent::willEvaluateScript(JSGlobalObject& globalObject)
{
    m_activeEvaluateScript = true;

    if (m_enableLegacyProfiler)
        LegacyProfiler::profiler()->startProfiling(globalObject.globalExec(), ASCIILiteral("ScriptProfiler"), m_environment.executionStopwatch());

    return m_environment.executionStopwatch()->elapsedTime();
}

void InspectorScriptProfilerAgent::didEvaluateScript(JSGlobalObject& globalObject, double startTime, ProfilingReason reason)
{
    m_activeEvaluateScript = false;

    if (m_enableLegacyProfiler)
        m_profiles.append(LegacyProfiler::profiler()->stopProfiling(globalObject.globalExec(), ASCIILiteral("ScriptProfiler")));

    double endTime = m_environment.executionStopwatch()->elapsedTime();

    addEvent(startTime, endTime, reason);
}

static Inspector::Protocol::ScriptProfiler::EventType toProtocol(ProfilingReason reason)
{
    switch (reason) {
    case ProfilingReason::API:
        return Inspector::Protocol::ScriptProfiler::EventType::API;
    case ProfilingReason::Microtask:
        return Inspector::Protocol::ScriptProfiler::EventType::Microtask;
    case ProfilingReason::Other:
        return Inspector::Protocol::ScriptProfiler::EventType::Other;
    }

    ASSERT_NOT_REACHED();
    return Inspector::Protocol::ScriptProfiler::EventType::Other;
}

void InspectorScriptProfilerAgent::addEvent(double startTime, double endTime, ProfilingReason reason)
{
    ASSERT(endTime >= startTime);

    auto event = Inspector::Protocol::ScriptProfiler::Event::create()
        .setStartTime(startTime)
        .setEndTime(endTime)
        .setType(toProtocol(reason))
        .release();

    m_frontendDispatcher->trackingUpdate(WTF::move(event));
}

static Ref<Protocol::Timeline::CPUProfileNodeAggregateCallInfo> buildAggregateCallInfoInspectorObject(const JSC::ProfileNode* node)
{
    double startTime = node->calls()[0].startTime();
    double endTime = node->calls().last().startTime() + node->calls().last().elapsedTime();

    double totalTime = 0;
    for (const JSC::ProfileNode::Call& call : node->calls())
        totalTime += call.elapsedTime();

    return Protocol::Timeline::CPUProfileNodeAggregateCallInfo::create()
        .setCallCount(node->calls().size())
        .setStartTime(startTime)
        .setEndTime(endTime)
        .setTotalTime(totalTime)
        .release();
}

static Ref<Protocol::Timeline::CPUProfileNode> buildInspectorObject(const JSC::ProfileNode* node)
{
    auto result = Protocol::Timeline::CPUProfileNode::create()
        .setId(node->id())
        .setCallInfo(buildAggregateCallInfoInspectorObject(node))
        .release();

    if (!node->functionName().isEmpty())
        result->setFunctionName(node->functionName());

    if (!node->url().isEmpty()) {
        result->setUrl(node->url());
        result->setLineNumber(node->lineNumber());
        result->setColumnNumber(node->columnNumber());
    }

    if (!node->children().isEmpty()) {
        auto children = Protocol::Array<Protocol::Timeline::CPUProfileNode>::create();
        for (RefPtr<JSC::ProfileNode> profileNode : node->children())
            children->addItem(buildInspectorObject(profileNode.get()));
        result->setChildren(WTF::move(children));
    }

    return result;
}

static Ref<Protocol::Timeline::CPUProfile> buildProfileInspectorObject(const JSC::Profile* profile)
{
    auto rootNodes = Protocol::Array<Protocol::Timeline::CPUProfileNode>::create();
    for (RefPtr<JSC::ProfileNode> profileNode : profile->rootNode()->children())
        rootNodes->addItem(buildInspectorObject(profileNode.get()));

    return Protocol::Timeline::CPUProfile::create()
        .setRootNodes(WTF::move(rootNodes))
        .release();
}

void InspectorScriptProfilerAgent::trackingComplete()
{
    RefPtr<Inspector::Protocol::Array<InspectorValue>> profiles = Inspector::Protocol::Array<InspectorValue>::create();
    for (auto& profile : m_profiles) {
        Ref<InspectorValue> value = buildProfileInspectorObject(profile.get());
        profiles->addItem(WTF::move(value));
    }

    m_frontendDispatcher->trackingComplete(profiles);

    m_profiles.clear();
}

} // namespace Inspector
