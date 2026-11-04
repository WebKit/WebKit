/*
 * Copyright (C) 2015-2023 Apple Inc. All rights reserved.
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

#include "Debugger.h"
#include "DeferGCInlines.h"
#include "HeapInlines.h"
#include "InspectorEnvironment.h"
#include "JSGlobalObject.h"
#include "JSLock.h"
#include "JSObject.h"
#include "SamplingProfiler.h"
#include "VMInlines.h"
#include <ranges>
#include <wtf/CheckedPtr.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Stopwatch.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>

namespace Inspector {

using namespace JSC;

WTF_MAKE_TZONE_ALLOCATED_IMPL(InspectorScriptProfilerAgent);

#if ENABLE(SAMPLING_PROFILER)
// A VM has one SamplingProfiler, shared by every agent that samples that VM (for example the agents of
// several frames in one web process). These are the agents sampling each VM, oldest first.
static Lock samplingAgentsLock;

static HashMap<const VM*, Vector<CheckedPtr<InspectorScriptProfilerAgent>>>& samplingAgentsByVM() WTF_REQUIRES_LOCK(samplingAgentsLock)
{
    static NeverDestroyed<HashMap<const VM*, Vector<CheckedPtr<InspectorScriptProfilerAgent>>>> samplingAgents;
    return samplingAgents;
}

static Vector<CheckedPtr<InspectorScriptProfilerAgent>> samplingAgentsForVM(const VM& vm)
{
    Locker locker { samplingAgentsLock };
    return samplingAgentsByVM().get(&vm);
}

static void addSamplingAgent(const VM& vm, InspectorScriptProfilerAgent& agent)
{
    Locker locker { samplingAgentsLock };
    samplingAgentsByVM().ensure(&vm, [] {
        return Vector<CheckedPtr<InspectorScriptProfilerAgent>> { };
    }).iterator->value.append(&agent);
}

static void removeSamplingAgent(InspectorScriptProfilerAgent& agent)
{
    Locker locker { samplingAgentsLock };
    samplingAgentsByVM().removeIf([&](auto& entry) {
        entry.value.removeFirst(&agent);
        return entry.value.isEmpty();
    });
}
#endif // ENABLE(SAMPLING_PROFILER)

InspectorScriptProfilerAgent::InspectorScriptProfilerAgent(AgentContext& context)
    : InspectorAgentBase("ScriptProfiler"_s)
    , m_frontendDispatcher(makeUniqueRef<ScriptProfilerFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(ScriptProfilerBackendDispatcher::create(protect(context.backendDispatcher), this))
    , m_environment(context.environment)
{
}

InspectorScriptProfilerAgent::~InspectorScriptProfilerAgent()
{
#if ENABLE(SAMPLING_PROFILER)
    if (m_enabledSamplingProfiler)
        removeSamplingAgent(*this);
#endif
}

void InspectorScriptProfilerAgent::didCreateFrontendAndBackend()
{
}

void InspectorScriptProfilerAgent::willDestroyFrontendAndBackend(DisconnectReason)
{
    // Stop tracking without sending results.
    if (m_tracking) {
        m_tracking = false;
        m_activeEvaluateScript = false;
        clearProfilingClientFromAllDebuggers();

        // Stop sampling without processing the samples.
        stopSamplingWhenDisconnecting();
    }
}

void InspectorScriptProfilerAgent::installProfilingClient()
{
    auto* debugger = protect(m_environment)->debugger();
    if (!debugger)
        return;

    // A debugger holds at most one profiling client. Nothing else installs one on this agent's
    // debugger, so this only guards against installing twice.
    if (debugger->hasProfilingClient())
        return;

    debugger->setProfilingClient(this);
}

// Uninstalling walks every debugger in the VM instead of only the environment's own, as a safety
// net: no debugger may keep a pointer to this agent after tracking stops, since this agent can be
// destroyed while the debugger lives on. The identity check keeps the walk from disturbing
// debuggers owned by another environment sharing this VM.
//
// No lock is needed for the walk: the VM's debugger list is only modified by the Debugger
// constructor and destructor, and like this agent they only run on a thread that already has
// exclusive use of the VM (the main thread for pages, the worker thread for workers, the JSLock
// holder for a JSContext).
void InspectorScriptProfilerAgent::clearProfilingClientFromAllDebuggers()
{
    protect(m_environment)->vm().forEachDebugger([&](JSC::Debugger& debugger) {
        if (debugger.hasProfilingClient(*this))
            debugger.setProfilingClient(nullptr);
    });
}

Protocol::ErrorStringOr<void> InspectorScriptProfilerAgent::startTracking(std::optional<bool>&& includeSamples)
{
    if (m_tracking)
        return { };

    m_tracking = true;

    auto& stopwatch = protect(m_environment)->executionStopwatch();

#if ENABLE(SAMPLING_PROFILER)
    if (includeSamples && *includeSamples)
        startSampling(stopwatch);
#else
    UNUSED_PARAM(includeSamples);
#endif // ENABLE(SAMPLING_PROFILER)

    installProfilingClient();

    m_frontendDispatcher->trackingStart(stopwatch.elapsedTime().seconds());

    return { };
}

Protocol::ErrorStringOr<void> InspectorScriptProfilerAgent::stopTracking()
{
    if (!m_tracking)
        return { };

    m_tracking = false;
    m_activeEvaluateScript = false;

    clearProfilingClientFromAllDebuggers();

    trackingComplete();

    return { };
}

bool InspectorScriptProfilerAgent::isAlreadyProfiling() const
{
    return m_activeEvaluateScript;
}

Seconds InspectorScriptProfilerAgent::willEvaluateScript()
{
    m_activeEvaluateScript = true;

#if ENABLE(SAMPLING_PROFILER)
    if (m_enabledSamplingProfiler) {
        SamplingProfiler* samplingProfiler = protect(m_environment)->vm().samplingProfiler();
        RELEASE_ASSERT(samplingProfiler);
        samplingProfiler->noticeCurrentThreadAsJSCExecutionThread();
    }
#endif

    return protect(m_environment)->executionStopwatch().elapsedTime();
}

void InspectorScriptProfilerAgent::didEvaluateScript(Seconds startTime, ProfilingReason reason)
{
    m_activeEvaluateScript = false;

    Seconds endTime = protect(m_environment)->executionStopwatch().elapsedTime();

    addEvent(startTime, endTime, reason);
}

NODELETE static Protocol::ScriptProfiler::EventType toProtocol(ProfilingReason reason)
{
    switch (reason) {
    case ProfilingReason::API:
        return Protocol::ScriptProfiler::EventType::API;
    case ProfilingReason::Microtask:
        return Protocol::ScriptProfiler::EventType::Microtask;
    case ProfilingReason::Other:
        return Protocol::ScriptProfiler::EventType::Other;
    }

    ASSERT_NOT_REACHED();
    return Protocol::ScriptProfiler::EventType::Other;
}

void InspectorScriptProfilerAgent::addEvent(Seconds startTime, Seconds endTime, ProfilingReason reason)
{
    ASSERT(endTime >= startTime);

    auto event = Protocol::ScriptProfiler::Event::create()
        .setStartTime(startTime.seconds())
        .setEndTime(endTime.seconds())
        .setType(toProtocol(reason))
        .release();

    m_frontendDispatcher->trackingUpdate(WTF::move(event));
}

#if ENABLE(SAMPLING_PROFILER)
static Ref<Protocol::ScriptProfiler::StackTrace> buildStackTrace(VM& vm, SamplingProfiler::StackTrace& stackTrace)
{
    auto frames = JSON::ArrayOf<Protocol::ScriptProfiler::StackFrame>::create();
    for (SamplingProfiler::StackFrame& stackFrame : stackTrace.frames) {
        auto frameObject = Protocol::ScriptProfiler::StackFrame::create()
            .setSourceID(String::number(std::get<1>(stackFrame.sourceProviderAndID())))
            .setName(stackFrame.displayName(vm))
            .setLine(stackFrame.functionStartLine())
            .setColumn(stackFrame.functionStartColumn())
            .setUrl(stackFrame.url())
            .release();

        if (stackFrame.hasExpressionInfo()) {
            Ref<Protocol::ScriptProfiler::ExpressionLocation> expressionLocation = Protocol::ScriptProfiler::ExpressionLocation::create()
                .setLine(stackFrame.lineNumber())
                .setColumn(stackFrame.columnNumber())
                .release();
            frameObject->setExpressionLocation(WTF::move(expressionLocation));
        }

        frames->addItem(WTF::move(frameObject));
    }

    return Protocol::ScriptProfiler::StackTrace::create()
        .setTimestamp(stackTrace.stopwatchTimestamp.seconds())
        .setStackFrames(WTF::move(frames))
        .release();
}

void InspectorScriptProfilerAgent::startSampling(Stopwatch& stopwatch)
{
    VM& vm = protect(m_environment)->vm();
    SamplingProfiler& samplingProfiler = vm.ensureSamplingProfiler(stopwatch);

    // The samples taken so far belong to the agents that were already sampling.
    auto samplingAgents = samplingAgentsForVM(vm);
    if (!samplingAgents.isEmpty())
        distributeStackTraces(vm, samplingAgents, ShouldPauseSampling::No);

    {
        Locker locker { samplingProfiler.getLock() };
        samplingProfiler.setStopWatch(stopwatch);
        samplingProfiler.noticeCurrentThreadAsJSCExecutionThreadWithLock();
        samplingProfiler.startWithLock();
    }

    m_stackTraces = JSON::ArrayOf<Protocol::ScriptProfiler::StackTrace>::create();
    addSamplingAgent(vm, *this);
    m_enabledSamplingProfiler = true;
}

Ref<Protocol::ScriptProfiler::Samples> InspectorScriptProfilerAgent::stopSampling()
{
    ASSERT(m_enabledSamplingProfiler);

    VM& vm = protect(m_environment)->vm();
    auto samplingAgents = samplingAgentsForVM(vm);
    ASSERT(samplingAgents.contains(this));
    removeSamplingAgent(*this);

    // Only the last agent pauses the profiler, since the others are still sampling.
    auto shouldPause = samplingAgents.size() == 1 ? ShouldPauseSampling::Yes : ShouldPauseSampling::No;
    distributeStackTraces(vm, samplingAgents, shouldPause);

    m_enabledSamplingProfiler = false;

    return Protocol::ScriptProfiler::Samples::create()
        .setStackTraces(std::exchange(m_stackTraces, nullptr).releaseNonNull())
        .release();
}

// A sample goes to the agent covering the global object of its outermost JavaScript frame, where the script was
// entered, because that is what the agent's ScriptProfiler events cover. Inner frames are tried if no agent covers
// it. A sample that no agent covers goes to the oldest agent that takes uncovered samples, or is dropped, so that a
// frame never gets another frame's samples.
void InspectorScriptProfilerAgent::distributeStackTraces(VM& vm, const SamplingAgents& samplingAgents, ShouldPauseSampling shouldPause)
{
    ASSERT(!samplingAgents.isEmpty());

    JSLockHolder lock(vm);
    DeferGC deferGC(vm); // This is required because we will have raw pointers into the heap after we releaseStackTraces().
    SamplingProfiler* samplingProfiler = vm.samplingProfiler();
    RELEASE_ASSERT(samplingProfiler);

    Locker locker { samplingProfiler->getLock() };
    if (shouldPause == ShouldPauseSampling::Yes)
        samplingProfiler->pause();
    Vector<SamplingProfiler::StackTrace> stackTraces = samplingProfiler->releaseStackTraces();
    locker.unlockEarly();

    CheckedPtr<InspectorScriptProfilerAgent> agentForUncoveredSamples;
    for (auto& agent : samplingAgents) {
        if (protect(agent->m_environment)->takesUncoveredSamples()) {
            agentForUncoveredSamples = agent;
            break;
        }
    }

    // There are far fewer global objects than samples, so remember which agent covers each one.
    HashMap<JSGlobalObject*, CheckedPtr<InspectorScriptProfilerAgent>> agentsForGlobalObjects;
    auto agentCoveringGlobalObject = [&](JSGlobalObject& globalObject) {
        return agentsForGlobalObjects.ensure(&globalObject, [&] -> CheckedPtr<InspectorScriptProfilerAgent> {
            for (auto& agent : samplingAgents) {
                if (protect(agent->m_environment)->coversGlobalObjectForSampling(globalObject))
                    return agent;
            }
            return nullptr;
        }).iterator->value;
    };

    // A lone agent that takes uncovered samples gets every sample, so there is nothing to look up.
    bool needsCoverage = samplingAgents.size() > 1 || !agentForUncoveredSamples;

    auto ownerOfStackTrace = [&](const SamplingProfiler::StackTrace& stackTrace) -> CheckedPtr<InspectorScriptProfilerAgent> {
        if (needsCoverage) {
            for (auto& stackFrame : stackTrace.frames | std::views::reverse) {
                if (!stackFrame.callee)
                    continue;

                auto* globalObject = stackFrame.callee->realmMayBeNull();
                if (!globalObject)
                    continue;

                if (auto agent = agentCoveringGlobalObject(*globalObject))
                    return agent;
            }
        }
        return agentForUncoveredSamples;
    };

    for (SamplingProfiler::StackTrace& stackTrace : stackTraces) {
        if (auto owner = ownerOfStackTrace(stackTrace))
            owner->m_stackTraces->addItem(buildStackTrace(vm, stackTrace));
    }
}
#endif // ENABLE(SAMPLING_PROFILER)

void InspectorScriptProfilerAgent::trackingComplete()
{
    auto stopwatchTimestamp = protect(m_environment)->executionStopwatch().elapsedTime().seconds();

#if ENABLE(SAMPLING_PROFILER)
    if (m_enabledSamplingProfiler) {
        m_frontendDispatcher->trackingComplete(stopwatchTimestamp, stopSampling());
        return;
    }
#endif

    m_frontendDispatcher->trackingComplete(stopwatchTimestamp, nullptr);
}

void InspectorScriptProfilerAgent::stopSamplingWhenDisconnecting()
{
#if ENABLE(SAMPLING_PROFILER)
    if (!m_enabledSamplingProfiler)
        return;

    VM& vm = protect(m_environment)->vm();
    auto samplingAgents = samplingAgentsForVM(vm);
    removeSamplingAgent(*this);

    // If other agents are still sampling, leave this agent's samples to their next distribution, where they are
    // handled like any other uncovered samples. This avoids heap work here, which can run while a frame is being
    // destroyed.
    if (samplingAgents.size() == 1) {
        JSLockHolder lock(vm);
        SamplingProfiler* samplingProfiler = vm.samplingProfiler();
        RELEASE_ASSERT(samplingProfiler);
        Locker locker { samplingProfiler->getLock() };
        samplingProfiler->pause();
        samplingProfiler->clearData();
    }

    m_stackTraces = nullptr;
    m_enabledSamplingProfiler = false;
#endif
}

} // namespace Inspector
