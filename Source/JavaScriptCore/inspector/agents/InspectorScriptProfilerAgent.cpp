/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#include "DeferGC.h"
#include "HeapInlines.h"
#include "InspectorEnvironment.h"
#include "SamplingProfiler.h"
#include "ScriptDebugServer.h"
#include <wtf/Stopwatch.h>

using namespace JSC;

namespace Inspector {

InspectorScriptProfilerAgent::InspectorScriptProfilerAgent(AgentContext& context)
    : InspectorAgentBase("ScriptProfiler"_s)
    , m_frontendDispatcher(makeUnique<ScriptProfilerFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(ScriptProfilerBackendDispatcher::create(context.backendDispatcher, this))
    , m_environment(context.environment)
{
}

InspectorScriptProfilerAgent::~InspectorScriptProfilerAgent() = default;

void InspectorScriptProfilerAgent::didCreateFrontendAndBackend(FrontendRouter*, BackendDispatcher*)
{
}

void InspectorScriptProfilerAgent::willDestroyFrontendAndBackend(DisconnectReason)
{
    // Stop tracking without sending results.
    if (m_tracking) {
        m_tracking = false;
        m_activeEvaluateScript = false;
        m_environment.scriptDebugServer().setProfilingClient(nullptr);

        // Stop sampling without processing the samples.
        stopSamplingWhenDisconnecting();
    }
}

void InspectorScriptProfilerAgent::startTracking(ErrorString&, const bool* includeSamples)
{
    if (m_tracking)
        return;

    m_tracking = true;

#if ENABLE(SAMPLING_PROFILER)
    if (includeSamples && *includeSamples) {
        VM& vm = m_environment.scriptDebugServer().vm();
        SamplingProfiler& samplingProfiler = vm.ensureSamplingProfiler(m_environment.executionStopwatch());

        LockHolder locker(samplingProfiler.getLock());
        samplingProfiler.setStopWatch(locker, m_environment.executionStopwatch());
        samplingProfiler.noticeCurrentThreadAsJSCExecutionThread(locker);
        samplingProfiler.start(locker);
        m_enabledSamplingProfiler = true;
    }
#else
    UNUSED_PARAM(includeSamples);
#endif // ENABLE(SAMPLING_PROFILER)

    m_environment.scriptDebugServer().setProfilingClient(this);

    m_frontendDispatcher->trackingStart(m_environment.executionStopwatch()->elapsedTime().seconds());
}

void InspectorScriptProfilerAgent::stopTracking(ErrorString&)
{
    if (!m_tracking)
        return;

    m_tracking = false;
    m_activeEvaluateScript = false;

    m_environment.scriptDebugServer().setProfilingClient(nullptr);

    trackingComplete();
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
        SamplingProfiler* samplingProfiler = m_environment.scriptDebugServer().vm().samplingProfiler();
        RELEASE_ASSERT(samplingProfiler);
        samplingProfiler->noticeCurrentThreadAsJSCExecutionThread();
    }
#endif

    return m_environment.executionStopwatch()->elapsedTime();
}

void InspectorScriptProfilerAgent::didEvaluateScript(Seconds startTime, ProfilingReason reason)
{
    m_activeEvaluateScript = false;

    Seconds endTime = m_environment.executionStopwatch()->elapsedTime();

    addEvent(startTime, endTime, reason);
}

static Protocol::ScriptProfiler::EventType toProtocol(ProfilingReason reason)
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

    m_frontendDispatcher->trackingUpdate(WTFMove(event));
}

#if ENABLE(SAMPLING_PROFILER)
static Ref<Protocol::ScriptProfiler::Samples> buildSamples(VM& vm, Vector<SamplingProfiler::StackTrace>&& samplingProfilerStackTraces)
{
    auto stackTraces = JSON::ArrayOf<Protocol::ScriptProfiler::StackTrace>::create();
    for (SamplingProfiler::StackTrace& stackTrace : samplingProfilerStackTraces) {
        auto frames = JSON::ArrayOf<Protocol::ScriptProfiler::StackFrame>::create();
        for (SamplingProfiler::StackFrame& stackFrame : stackTrace.frames) {
            auto frameObject = Protocol::ScriptProfiler::StackFrame::create()
                .setSourceID(String::number(stackFrame.sourceID()))
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
                frameObject->setExpressionLocation(WTFMove(expressionLocation));
            }

            frames->addItem(WTFMove(frameObject));
        }
        Ref<Protocol::ScriptProfiler::StackTrace> inspectorStackTrace = Protocol::ScriptProfiler::StackTrace::create()
            .setTimestamp(stackTrace.timestamp.seconds())
            .setStackFrames(WTFMove(frames))
            .release();
        stackTraces->addItem(WTFMove(inspectorStackTrace));
    }

    return Protocol::ScriptProfiler::Samples::create()
        .setStackTraces(WTFMove(stackTraces))
        .release();
}
#endif // ENABLE(SAMPLING_PROFILER)

void InspectorScriptProfilerAgent::trackingComplete()
{
    auto timestamp = m_environment.executionStopwatch()->elapsedTime().seconds();

#if ENABLE(SAMPLING_PROFILER)
    if (m_enabledSamplingProfiler) {
        VM& vm = m_environment.scriptDebugServer().vm();
        JSLockHolder lock(vm);
        DeferGC deferGC(vm.heap); // This is required because we will have raw pointers into the heap after we releaseStackTraces().
        SamplingProfiler* samplingProfiler = vm.samplingProfiler();
        RELEASE_ASSERT(samplingProfiler);

        LockHolder locker(samplingProfiler->getLock());
        samplingProfiler->pause(locker);
        Vector<SamplingProfiler::StackTrace> stackTraces = samplingProfiler->releaseStackTraces(locker);
        locker.unlockEarly();

        Ref<Protocol::ScriptProfiler::Samples> samples = buildSamples(vm, WTFMove(stackTraces));

        m_enabledSamplingProfiler = false;

        m_frontendDispatcher->trackingComplete(timestamp, WTFMove(samples));
    } else
        m_frontendDispatcher->trackingComplete(timestamp, nullptr);
#else
    m_frontendDispatcher->trackingComplete(timestamp, nullptr);
#endif // ENABLE(SAMPLING_PROFILER)
}

void InspectorScriptProfilerAgent::stopSamplingWhenDisconnecting()
{
#if ENABLE(SAMPLING_PROFILER)
    if (!m_enabledSamplingProfiler)
        return;

    VM& vm = m_environment.scriptDebugServer().vm();
    JSLockHolder lock(vm);
    SamplingProfiler* samplingProfiler = vm.samplingProfiler();
    RELEASE_ASSERT(samplingProfiler);
    LockHolder locker(samplingProfiler->getLock());
    samplingProfiler->pause(locker);
    samplingProfiler->clearData(locker);

    m_enabledSamplingProfiler = false;
#endif
}

} // namespace Inspector
