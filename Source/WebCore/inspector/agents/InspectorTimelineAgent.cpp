/*
* Copyright (C) 2013 Google Inc. All rights reserved.
* Copyright (C) 2014 University of Washington.
* Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "InspectorTimelineAgent.h"

#include "Event.h"
#include "InspectorAnimationAgent.h"
#include "InspectorCPUProfilerAgent.h"
#include "InspectorMemoryAgent.h"
#include "InstrumentingAgents.h"
#include "JSExecState.h"
#include "TimelineRecordFactory.h"
#include "WebConsoleAgent.h"
#include "WebDebuggerAgent.h"
#include "WebHeapAgent.h"
#include <JavaScriptCore/Breakpoint.h>
#include <JavaScriptCore/ConsoleMessage.h>
#include <JavaScriptCore/Debugger.h>
#include <JavaScriptCore/InspectorScriptProfilerAgent.h>
#include <JavaScriptCore/ScriptArguments.h>
#include <wtf/SetForScope.h>
#include <wtf/Stopwatch.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(InspectorTimelineAgent);

InspectorTimelineAgent::InspectorTimelineAgent(WebAgentContext& context)
    : InspectorAgentBase("Timeline"_s, context)
    , m_frontendDispatcher(makeUniqueRef<Inspector::TimelineFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::TimelineBackendDispatcher::create(context.backendDispatcher, this))
{
}

InspectorTimelineAgent::~InspectorTimelineAgent() = default;

void InspectorTimelineAgent::didCreateFrontendAndBackend()
{
}

void InspectorTimelineAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    disable();
}

Inspector::Protocol::ErrorStringOr<void> InspectorTimelineAgent::enable()
{
    if (enabled())
        return makeUnexpected("Timeline domain already enabled"_s);

    internalEnable();

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorTimelineAgent::disable()
{
    if (!enabled())
        return makeUnexpected("Timeline domain already disabled"_s);

    internalDisable();

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorTimelineAgent::start(std::optional<int>&& maxCallStackDepth)
{
    m_trackingFromFrontend = true;

    if (!tracking())
        internalStart(WTFMove(maxCallStackDepth));

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorTimelineAgent::stop()
{
    if (tracking())
        internalStop();

    m_trackingFromFrontend = false;

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorTimelineAgent::setInstruments(Ref<JSON::Array>&& instruments)
{
    Vector<Inspector::Protocol::Timeline::Instrument> newInstruments;
    newInstruments.reserveCapacity(instruments->length());

    for (const auto& instrumentValue : instruments.get()) {
        auto instrumentString = instrumentValue->asString();
        if (!instrumentString)
            return makeUnexpected("Unexpected non-string value in given instruments"_s);

        auto instrument = Inspector::Protocol::Helpers::parseEnumValueFromString<Inspector::Protocol::Timeline::Instrument>(instrumentString);
        if (!instrument)
            return makeUnexpected(makeString("Unknown instrument: "_s, instrumentString));

        newInstruments.append(*instrument);
    }

    m_instruments.swap(newInstruments);

    return { };
}

bool InspectorTimelineAgent::enabled() const
{
    return m_instrumentingAgents.enabledTimelineAgent() == this;
}

void InspectorTimelineAgent::internalEnable()
{
    ASSERT(!enabled());

    m_instrumentingAgents.setEnabledTimelineAgent(this);
}

void InspectorTimelineAgent::internalDisable()
{
    m_instrumentingAgents.setEnabledTimelineAgent(nullptr);

    stop();

    m_instruments.clear();
}

bool InspectorTimelineAgent::tracking() const
{
    return m_instrumentingAgents.trackingTimelineAgent() == this;
}

void InspectorTimelineAgent::internalStart(std::optional<int>&& maxCallStackDepth)
{
    ASSERT(!tracking());

    if (maxCallStackDepth && *maxCallStackDepth > 0)
        m_maxCallStackDepth = *maxCallStackDepth;
    else
        m_maxCallStackDepth = 5;

    m_instrumentingAgents.setTrackingTimelineAgent(this);

    m_environment.debugger()->addObserver(*this);

    m_frontendDispatcher->recordingStarted(timestamp());
}

void InspectorTimelineAgent::internalStop()
{
    m_instrumentingAgents.setTrackingTimelineAgent(nullptr);

    m_environment.debugger()->removeObserver(*this, true);

    // Complete all pending records to prevent discarding events that are currently in progress.
    while (!m_recordStack.isEmpty())
        didCompleteCurrentRecord(m_recordStack.last().type);

    m_recordStack.clear();

    m_frontendDispatcher->recordingStopped(timestamp());
}

void InspectorTimelineAgent::autoCaptureStarted() const
{
    m_frontendDispatcher->autoCaptureStarted();
}

double InspectorTimelineAgent::timestamp()
{
    return m_environment.executionStopwatch().elapsedTime().seconds();
}

std::optional<double> InspectorTimelineAgent::timestampFromMonotonicTime(MonotonicTime time)
{
    auto stopwatchTime = m_environment.executionStopwatch().fromMonotonicTime(time);
    if (!stopwatchTime)
        return std::nullopt;
    return stopwatchTime->seconds();
}

void InspectorTimelineAgent::startFromConsole(const String& title)
{
    // Allow duplicate unnamed profiles. Disallow duplicate named profiles.
    if (!title.isEmpty()) {
        for (const TimelineRecordEntry& record : m_pendingConsoleProfileRecords) {
            auto recordTitle = record.data->getString("title"_s);
            if (recordTitle == title) {
                if (auto* consoleAgent = m_instrumentingAgents.webConsoleAgent()) {
                    // FIXME: Send an enum to the frontend for localization?
                    String warning = title.isEmpty() ? "Unnamed Profile already exists"_s : makeString("Profile \""_s, ScriptArguments::truncateStringForConsoleMessage(title), "\" already exists"_s);
                    consoleAgent->addMessageToConsole(makeUnique<ConsoleMessage>(MessageSource::ConsoleAPI, MessageType::Profile, MessageLevel::Warning, warning));
                }
                return;
            }
        }
    }

    if (!tracking() && m_pendingConsoleProfileRecords.isEmpty())
        startProgrammaticCapture();

    m_pendingConsoleProfileRecords.append(createRecordEntry(TimelineRecordFactory::createConsoleProfileData(title), TimelineRecordType::ConsoleProfile, true));
}

void InspectorTimelineAgent::stopFromConsole(const String& title)
{
    // Stop profiles in reverse order. If the title is empty, then stop the last profile.
    // Otherwise, match the title of the profile to stop.
    for (int i = m_pendingConsoleProfileRecords.size() - 1; i >= 0; --i) {
        const TimelineRecordEntry& record = m_pendingConsoleProfileRecords[i];

        auto recordTitle = record.data->getString("title"_s);
        if (title.isEmpty() || recordTitle == title) {
            didCompleteRecordEntry(record);
            m_pendingConsoleProfileRecords.removeAt(i);

            if (!m_trackingFromFrontend && m_pendingConsoleProfileRecords.isEmpty())
                stopProgrammaticCapture();

            return;
        }
    }

    if (auto* consoleAgent = m_instrumentingAgents.webConsoleAgent()) {
        // FIXME: Send an enum to the frontend for localization?
        String warning = title.isEmpty() ? "No profiles exist"_s : makeString("Profile \""_s, ScriptArguments::truncateStringForConsoleMessage(title), "\" does not exist"_s);
        consoleAgent->addMessageToConsole(makeUnique<ConsoleMessage>(MessageSource::ConsoleAPI, MessageType::ProfileEnd, MessageLevel::Warning, warning));
    }
}

void InspectorTimelineAgent::willCallFunction(const String& scriptName, int scriptLine, int scriptColumn)
{
    pushCurrentRecord(TimelineRecordFactory::createFunctionCallData(scriptName, scriptLine, scriptColumn), TimelineRecordType::FunctionCall, true);
}

void InspectorTimelineAgent::didCallFunction()
{
    didCompleteCurrentRecord(TimelineRecordType::FunctionCall);
}

void InspectorTimelineAgent::willDispatchEvent(const Event& event)
{
    pushCurrentRecord(TimelineRecordFactory::createEventDispatchData(event), TimelineRecordType::EventDispatch, false);
}

void InspectorTimelineAgent::didDispatchEvent(bool defaultPrevented)
{
    if (m_recordStack.isEmpty())
        return;

    auto& entry = m_recordStack.last();
    ASSERT(entry.type == TimelineRecordType::EventDispatch);
    entry.data->setBoolean("defaultPrevented"_s, defaultPrevented);

    didCompleteCurrentRecord(TimelineRecordType::EventDispatch);
}

void InspectorTimelineAgent::didInstallTimer(int timerId, Seconds timeout, bool singleShot)
{
    appendRecord(TimelineRecordFactory::createTimerInstallData(timerId, timeout, singleShot), TimelineRecordType::TimerInstall, true);
}

void InspectorTimelineAgent::didRemoveTimer(int timerId)
{
    appendRecord(TimelineRecordFactory::createGenericTimerData(timerId), TimelineRecordType::TimerRemove, true);
}

void InspectorTimelineAgent::willFireTimer(int timerId)
{
    pushCurrentRecord(TimelineRecordFactory::createGenericTimerData(timerId), TimelineRecordType::TimerFire, false);
}

void InspectorTimelineAgent::didFireTimer()
{
    didCompleteCurrentRecord(TimelineRecordType::TimerFire);
}

void InspectorTimelineAgent::willEvaluateScript(const String& url, int lineNumber, int columnNumber)
{
    pushCurrentRecord(TimelineRecordFactory::createEvaluateScriptData(url, lineNumber, columnNumber), TimelineRecordType::EvaluateScript, true);
}

void InspectorTimelineAgent::didEvaluateScript()
{
    didCompleteCurrentRecord(TimelineRecordType::EvaluateScript);
}

void InspectorTimelineAgent::didTimeStamp(const String& message)
{
    appendRecord(TimelineRecordFactory::createTimeStampData(message), TimelineRecordType::TimeStamp, true);
}

void InspectorTimelineAgent::time(const String& label)
{
    appendRecord(TimelineRecordFactory::createTimeStampData(label), TimelineRecordType::Time, true);
}

void InspectorTimelineAgent::timeEnd(const String& label)
{
    appendRecord(TimelineRecordFactory::createTimeStampData(label), TimelineRecordType::TimeEnd, true);
}

void InspectorTimelineAgent::didPerformanceMark(const String& label, std::optional<MonotonicTime> timeInMonotonicTime)
{
    std::optional<double> timestamp;
    if (timeInMonotonicTime) {
        timestamp = timestampFromMonotonicTime(*timeInMonotonicTime);
        if (!timestamp)
            return; // Stopwatch wasn't running at the time of timeInMonotonicTime.
    }
    appendRecord(TimelineRecordFactory::createTimeStampData(label), TimelineRecordType::TimeStamp, true, timestamp);
}

void InspectorTimelineAgent::startProgrammaticCapture()
{
    ASSERT(!tracking());

    // Disable breakpoints during programmatic capture.
    if (auto* webDebuggerAgent = m_instrumentingAgents.enabledWebDebuggerAgent()) {
        m_programmaticCaptureRestoreBreakpointActiveValue = webDebuggerAgent->breakpointsActive();
        if (m_programmaticCaptureRestoreBreakpointActiveValue)
            webDebuggerAgent->setBreakpointsActive(false);
    } else
        m_programmaticCaptureRestoreBreakpointActiveValue = false;

    toggleScriptProfilerInstrument(InstrumentState::Start); // Ensure JavaScript samping data.
    toggleTimelineInstrument(InstrumentState::Start); // Ensure Console Profile event records.
    toggleInstruments(InstrumentState::Start); // Any other instruments the frontend wants us to record.
}

void InspectorTimelineAgent::stopProgrammaticCapture()
{
    ASSERT(tracking());
    ASSERT(!m_trackingFromFrontend);

    toggleInstruments(InstrumentState::Stop);
    toggleTimelineInstrument(InstrumentState::Stop);
    toggleScriptProfilerInstrument(InstrumentState::Stop);

    // Re-enable breakpoints if they were enabled.
    if (m_programmaticCaptureRestoreBreakpointActiveValue) {
        if (auto* webDebuggerAgent = m_instrumentingAgents.enabledWebDebuggerAgent())
            webDebuggerAgent->setBreakpointsActive(true);
    }
}

void InspectorTimelineAgent::toggleInstruments(InstrumentState state)
{
    for (auto instrumentType : m_instruments) {
        switch (instrumentType) {
        case Inspector::Protocol::Timeline::Instrument::ScriptProfiler: {
            toggleScriptProfilerInstrument(state);
            break;
        }
        case Inspector::Protocol::Timeline::Instrument::Heap: {
            toggleHeapInstrument(state);
            break;
        }
        case Inspector::Protocol::Timeline::Instrument::CPU: {
            toggleCPUInstrument(state);
            break;
        }
        case Inspector::Protocol::Timeline::Instrument::Memory: {
            toggleMemoryInstrument(state);
            break;
        }
        case Inspector::Protocol::Timeline::Instrument::Timeline:
            toggleTimelineInstrument(state);
            break;
        case Inspector::Protocol::Timeline::Instrument::Animation:
            toggleAnimationInstrument(state);
            break;
        case Inspector::Protocol::Timeline::Instrument::Screenshot: {
            break;
        }
        }
    }
}

void InspectorTimelineAgent::toggleScriptProfilerInstrument(InstrumentState state)
{
    if (auto* scriptProfilerAgent = m_instrumentingAgents.persistentScriptProfilerAgent()) {
        if (state == InstrumentState::Start)
            scriptProfilerAgent->startTracking(true);
        else
            scriptProfilerAgent->stopTracking();
    }
}

void InspectorTimelineAgent::toggleHeapInstrument(InstrumentState state)
{
    if (auto* heapAgent = m_instrumentingAgents.persistentWebHeapAgent()) {
        if (state == InstrumentState::Start) {
            if (shouldStartHeapInstrument())
                heapAgent->startTracking();
        } else
            heapAgent->stopTracking();
    }
}

void InspectorTimelineAgent::toggleCPUInstrument(InstrumentState state)
{
#if ENABLE(RESOURCE_USAGE)
    if (auto* cpuProfilerAgent = m_instrumentingAgents.persistentCPUProfilerAgent()) {
        if (state == InstrumentState::Start)
            cpuProfilerAgent->startTracking();
        else
            cpuProfilerAgent->stopTracking();
    }
#else
    UNUSED_PARAM(state);
#endif
}

void InspectorTimelineAgent::toggleMemoryInstrument(InstrumentState state)
{
#if ENABLE(RESOURCE_USAGE)
    if (auto* memoryAgent = m_instrumentingAgents.persistentMemoryAgent()) {
        if (state == InstrumentState::Start)
            memoryAgent->startTracking();
        else
            memoryAgent->stopTracking();
    }
#else
    UNUSED_PARAM(state);
#endif
}

void InspectorTimelineAgent::toggleTimelineInstrument(InstrumentState state)
{
    if (state == InstrumentState::Start) {
        if (!tracking())
            internalStart(std::nullopt);
    } else {
        if (tracking())
            internalStop();
    }
}

void InspectorTimelineAgent::toggleAnimationInstrument(InstrumentState state)
{
    if (auto* animationAgent = m_instrumentingAgents.persistentAnimationAgent()) {
        if (state == InstrumentState::Start)
            animationAgent->startTracking();
        else
            animationAgent->stopTracking();
    }
}

void InspectorTimelineAgent::didRequestAnimationFrame(int callbackId)
{
    appendRecord(TimelineRecordFactory::createAnimationFrameData(callbackId), TimelineRecordType::RequestAnimationFrame, true);
}

void InspectorTimelineAgent::didCancelAnimationFrame(int callbackId)
{
    appendRecord(TimelineRecordFactory::createAnimationFrameData(callbackId), TimelineRecordType::CancelAnimationFrame, true);
}

void InspectorTimelineAgent::willFireAnimationFrame(int callbackId)
{
    pushCurrentRecord(TimelineRecordFactory::createAnimationFrameData(callbackId), TimelineRecordType::FireAnimationFrame, false);
}

void InspectorTimelineAgent::didFireAnimationFrame()
{
    didCompleteCurrentRecord(TimelineRecordType::FireAnimationFrame);
}

void InspectorTimelineAgent::willFireObserverCallback(const String& callbackType)
{
    pushCurrentRecord(TimelineRecordFactory::createObserverCallbackData(callbackType), TimelineRecordType::ObserverCallback, false);
}

void InspectorTimelineAgent::didFireObserverCallback()
{
    didCompleteCurrentRecord(TimelineRecordType::ObserverCallback);
}

void InspectorTimelineAgent::breakpointActionProbe(JSC::JSGlobalObject*, JSC::BreakpointActionID actionID, unsigned /*batchId*/, unsigned sampleId, JSC::JSValue)
{
    appendRecord(TimelineRecordFactory::createProbeSampleData(actionID, sampleId), TimelineRecordType::ProbeSample, false);
}

static Inspector::Protocol::Timeline::EventType toProtocol(TimelineRecordType type)
{
    switch (type) {
    case TimelineRecordType::EventDispatch:
        return Inspector::Protocol::Timeline::EventType::EventDispatch;
    case TimelineRecordType::ScheduleStyleRecalculation:
        return Inspector::Protocol::Timeline::EventType::ScheduleStyleRecalculation;
    case TimelineRecordType::RecalculateStyles:
        return Inspector::Protocol::Timeline::EventType::RecalculateStyles;
    case TimelineRecordType::InvalidateLayout:
        return Inspector::Protocol::Timeline::EventType::InvalidateLayout;
    case TimelineRecordType::Layout:
        return Inspector::Protocol::Timeline::EventType::Layout;
    case TimelineRecordType::Paint:
        return Inspector::Protocol::Timeline::EventType::Paint;
    case TimelineRecordType::Composite:
        return Inspector::Protocol::Timeline::EventType::Composite;
    case TimelineRecordType::RenderingFrame:
        return Inspector::Protocol::Timeline::EventType::RenderingFrame;

    case TimelineRecordType::TimerInstall:
        return Inspector::Protocol::Timeline::EventType::TimerInstall;
    case TimelineRecordType::TimerRemove:
        return Inspector::Protocol::Timeline::EventType::TimerRemove;
    case TimelineRecordType::TimerFire:
        return Inspector::Protocol::Timeline::EventType::TimerFire;

    case TimelineRecordType::EvaluateScript:
        return Inspector::Protocol::Timeline::EventType::EvaluateScript;

    case TimelineRecordType::TimeStamp:
        return Inspector::Protocol::Timeline::EventType::TimeStamp;
    case TimelineRecordType::Time:
        return Inspector::Protocol::Timeline::EventType::Time;
    case TimelineRecordType::TimeEnd:
        return Inspector::Protocol::Timeline::EventType::TimeEnd;

    case TimelineRecordType::FunctionCall:
        return Inspector::Protocol::Timeline::EventType::FunctionCall;
    case TimelineRecordType::ProbeSample:
        return Inspector::Protocol::Timeline::EventType::ProbeSample;
    case TimelineRecordType::ConsoleProfile:
        return Inspector::Protocol::Timeline::EventType::ConsoleProfile;

    case TimelineRecordType::RequestAnimationFrame:
        return Inspector::Protocol::Timeline::EventType::RequestAnimationFrame;
    case TimelineRecordType::CancelAnimationFrame:
        return Inspector::Protocol::Timeline::EventType::CancelAnimationFrame;
    case TimelineRecordType::FireAnimationFrame:
        return Inspector::Protocol::Timeline::EventType::FireAnimationFrame;

    case TimelineRecordType::ObserverCallback:
        return Inspector::Protocol::Timeline::EventType::ObserverCallback;

    case TimelineRecordType::Screenshot:
        return Inspector::Protocol::Timeline::EventType::Screenshot;
    }

    return Inspector::Protocol::Timeline::EventType::TimeStamp;
}

void InspectorTimelineAgent::addRecordToTimeline(Ref<JSON::Object>&& record, TimelineRecordType type)
{
    record->setString("type"_s, Inspector::Protocol::Helpers::getEnumConstantValue(toProtocol(type)));

    if (m_recordStack.isEmpty()) {
        // FIXME: runtimeCast is a hack. We do it because we can't build TimelineEvent directly now.
        auto recordObject = Inspector::Protocol::BindingTraits<Inspector::Protocol::Timeline::TimelineEvent>::runtimeCast(WTFMove(record));
        sendEvent(WTFMove(recordObject));
    } else {
        const TimelineRecordEntry& parent = m_recordStack.last();
        // Nested paint records are an implementation detail and add no information not already contained in the parent.
        if (type == TimelineRecordType::Paint && parent.type == type)
            return;

        parent.children->pushObject(WTFMove(record));
    }
}

void InspectorTimelineAgent::didCompleteRecordEntry(const TimelineRecordEntry& entry)
{
    entry.record->setObject("data"_s, entry.data.copyRef());
    if (entry.children)
        entry.record->setArray("children"_s, *entry.children);
    entry.record->setDouble("endTime"_s, timestamp());
    addRecordToTimeline(entry.record.copyRef(), entry.type);
}

void InspectorTimelineAgent::didCompleteCurrentRecord(TimelineRecordType type)
{
    // An empty stack could merely mean that the timeline agent was turned on in the middle of
    // an event.  Don't treat as an error.
    if (!m_recordStack.isEmpty()) {
        TimelineRecordEntry entry = m_recordStack.last();
        m_recordStack.removeLast();
        ASSERT_UNUSED(type, entry.type == type);

        // Don't send RenderingFrame records that have no children to reduce noise.
        if (entry.type == TimelineRecordType::RenderingFrame && !entry.children->length())
            return;

        didCompleteRecordEntry(entry);
    }
}

InspectorTimelineAgent::TimelineRecordEntry* InspectorTimelineAgent::lastRecordEntry()
{
    if (m_recordStack.isEmpty())
        return nullptr;
    return &m_recordStack.last();
}

void InspectorTimelineAgent::appendRecord(Ref<JSON::Object>&& data, TimelineRecordType type, bool captureCallStack, std::optional<double> startTime)
{
    Ref<JSON::Object> record = TimelineRecordFactory::createGenericRecord(startTime.value_or(timestamp()), captureCallStack ? m_maxCallStackDepth : 0);
    record->setObject("data"_s, WTFMove(data));
    addRecordToTimeline(WTFMove(record), type);
}

void InspectorTimelineAgent::sendEvent(Ref<JSON::Object>&& event)
{
    // FIXME: runtimeCast is a hack. We do it because we can't build TimelineEvent directly now.
    auto recordChecked = Inspector::Protocol::BindingTraits<Inspector::Protocol::Timeline::TimelineEvent>::runtimeCast(WTFMove(event));
    m_frontendDispatcher->eventRecorded(WTFMove(recordChecked));
}

InspectorTimelineAgent::TimelineRecordEntry InspectorTimelineAgent::createRecordEntry(Ref<JSON::Object>&& data, TimelineRecordType type, bool captureCallStack, std::optional<double> startTime)
{
    Ref<JSON::Object> record = TimelineRecordFactory::createGenericRecord(startTime.value_or(timestamp()), captureCallStack ? m_maxCallStackDepth : 0);
    return TimelineRecordEntry(WTFMove(record), WTFMove(data), JSON::Array::create(), type);
}

void InspectorTimelineAgent::pushCurrentRecord(Ref<JSON::Object>&& data, TimelineRecordType type, bool captureCallStack, std::optional<double> startTime)
{
    pushCurrentRecord(createRecordEntry(WTFMove(data), type, captureCallStack, startTime));
}

} // namespace WebCore
