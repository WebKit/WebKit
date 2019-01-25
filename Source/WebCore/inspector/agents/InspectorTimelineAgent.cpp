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

#include "DOMWindow.h"
#include "Event.h"
#include "Frame.h"
#include "InspectorCPUProfilerAgent.h"
#include "InspectorMemoryAgent.h"
#include "InspectorPageAgent.h"
#include "InstrumentingAgents.h"
#include "JSDOMWindow.h"
#include "PageScriptDebugServer.h"
#include "RenderView.h"
#include "ScriptState.h"
#include "TimelineRecordFactory.h"
#include "WebConsoleAgent.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <JavaScriptCore/InspectorDebuggerAgent.h>
#include <JavaScriptCore/InspectorHeapAgent.h>
#include <JavaScriptCore/InspectorScriptProfilerAgent.h>
#include <JavaScriptCore/ScriptBreakpoint.h>
#include <wtf/Stopwatch.h>

#if PLATFORM(IOS_FAMILY)
#include "RuntimeApplicationChecks.h"
#include "WebCoreThreadInternal.h"
#endif

#if PLATFORM(COCOA)
#include "RunLoopObserver.h"
#endif


namespace WebCore {

using namespace Inspector;

#if PLATFORM(COCOA)
static CFRunLoopRef currentRunLoop()
{
#if PLATFORM(IOS_FAMILY)
    // A race condition during WebView deallocation can lead to a crash if the layer sync run loop
    // observer is added to the main run loop <rdar://problem/9798550>. However, for responsiveness,
    // we still allow this, see <rdar://problem/7403328>. Since the race condition and subsequent
    // crash are especially troublesome for iBooks, we never allow the observer to be added to the
    // main run loop in iBooks.
    if (IOSApplication::isIBooks())
        return WebThreadRunLoop();
#endif
    return CFRunLoopGetCurrent();
}
#endif

InspectorTimelineAgent::InspectorTimelineAgent(WebAgentContext& context, InspectorScriptProfilerAgent* scriptProfileAgent, InspectorHeapAgent* heapAgent, InspectorPageAgent* pageAgent)
    : InspectorAgentBase("Timeline"_s, context)
    , m_frontendDispatcher(std::make_unique<Inspector::TimelineFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::TimelineBackendDispatcher::create(context.backendDispatcher, this))
    , m_scriptProfilerAgent(scriptProfileAgent)
    , m_heapAgent(heapAgent)
    , m_pageAgent(pageAgent)
{
}

InspectorTimelineAgent::~InspectorTimelineAgent() = default;

void InspectorTimelineAgent::didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*)
{
    m_instrumentingAgents.setPersistentInspectorTimelineAgent(this);
}

void InspectorTimelineAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    m_instrumentingAgents.setPersistentInspectorTimelineAgent(nullptr);

    ErrorString unused;
    stop(unused);

    m_autoCaptureEnabled = false;
    m_instruments.clear();
}

void InspectorTimelineAgent::start(ErrorString&, const int* maxCallStackDepth)
{
    m_enabledFromFrontend = true;

    internalStart(maxCallStackDepth);
}

void InspectorTimelineAgent::stop(ErrorString&)
{
    internalStop();

    m_enabledFromFrontend = false;
}

void InspectorTimelineAgent::setAutoCaptureEnabled(ErrorString&, bool enabled)
{
    m_autoCaptureEnabled = enabled;
}

void InspectorTimelineAgent::setInstruments(ErrorString& errorString, const JSON::Array& instruments)
{
    Vector<Protocol::Timeline::Instrument> newInstruments;
    newInstruments.reserveCapacity(instruments.length());

    for (const auto& instrumentValue : instruments) {
        String enumValueString;
        if (!instrumentValue->asString(enumValueString)) {
            errorString = "Unexpected type in instruments list, should be string"_s;
            return;
        }

        Optional<Protocol::Timeline::Instrument> instrumentType = Protocol::InspectorHelpers::parseEnumValueFromString<Protocol::Timeline::Instrument>(enumValueString);
        if (!instrumentType) {
            errorString = makeString("Unexpected enum value: ", enumValueString);
            return;
        }

        newInstruments.uncheckedAppend(*instrumentType);
    }

    m_instruments.swap(newInstruments);
}

void InspectorTimelineAgent::internalStart(const int* maxCallStackDepth)
{
    if (m_enabled)
        return;

    if (maxCallStackDepth && *maxCallStackDepth > 0)
        m_maxCallStackDepth = *maxCallStackDepth;
    else
        m_maxCallStackDepth = 5;

    m_instrumentingAgents.setInspectorTimelineAgent(this);

    m_environment.scriptDebugServer().addListener(this);

    m_enabled = true;

    // FIXME: Abstract away platform-specific code once https://bugs.webkit.org/show_bug.cgi?id=142748 is fixed.

#if PLATFORM(COCOA)
    m_frameStartObserver = std::make_unique<RunLoopObserver>(static_cast<CFIndex>(RunLoopObserver::WellKnownRunLoopOrders::InspectorFrameBegin), [this]() {
        if (!m_enabled || m_environment.scriptDebugServer().isPaused())
            return;

        if (!m_runLoopNestingLevel)
            pushCurrentRecord(JSON::Object::create(), TimelineRecordType::RenderingFrame, false, nullptr);
        m_runLoopNestingLevel++;
    });

    m_frameStopObserver = std::make_unique<RunLoopObserver>(static_cast<CFIndex>(RunLoopObserver::WellKnownRunLoopOrders::InspectorFrameEnd), [this]() {
        if (!m_enabled || m_environment.scriptDebugServer().isPaused())
            return;

        ASSERT(m_runLoopNestingLevel > 0);
        m_runLoopNestingLevel--;
        if (m_runLoopNestingLevel)
            return;

        if (m_startedComposite)
            didComposite();

        didCompleteCurrentRecord(TimelineRecordType::RenderingFrame);
    });

    m_frameStartObserver->schedule(currentRunLoop(), kCFRunLoopEntry | kCFRunLoopAfterWaiting);
    m_frameStopObserver->schedule(currentRunLoop(), kCFRunLoopExit | kCFRunLoopBeforeWaiting);

    // Create a runloop record and increment the runloop nesting level, to capture the current turn of the main runloop
    // (which is the outer runloop if recording started while paused in the debugger).
    pushCurrentRecord(JSON::Object::create(), TimelineRecordType::RenderingFrame, false, nullptr);

    m_runLoopNestingLevel = 1;
#endif

    m_frontendDispatcher->recordingStarted(timestamp());
}

void InspectorTimelineAgent::internalStop()
{
    if (!m_enabled)
        return;

    m_instrumentingAgents.setInspectorTimelineAgent(nullptr);

    m_environment.scriptDebugServer().removeListener(this, true);

#if PLATFORM(COCOA)
    m_frameStartObserver = nullptr;
    m_frameStopObserver = nullptr;
    m_runLoopNestingLevel = 0;

    // Complete all pending records to prevent discarding events that are currently in progress.
    while (!m_recordStack.isEmpty())
        didCompleteCurrentRecord(m_recordStack.last().type);
#endif

    clearRecordStack();

    m_enabled = false;
    m_startedComposite = false;
    m_autoCapturePhase = AutoCapturePhase::None;

    m_frontendDispatcher->recordingStopped(timestamp());
}

double InspectorTimelineAgent::timestamp()
{
    return m_environment.executionStopwatch()->elapsedTime().seconds();
}

void InspectorTimelineAgent::startFromConsole(JSC::ExecState* exec, const String& title)
{
    // Allow duplicate unnamed profiles. Disallow duplicate named profiles.
    if (!title.isEmpty()) {
        for (const TimelineRecordEntry& record : m_pendingConsoleProfileRecords) {
            String recordTitle;
            record.data->getString("title"_s, recordTitle);
            if (recordTitle == title) {
                if (WebConsoleAgent* consoleAgent = m_instrumentingAgents.webConsoleAgent()) {
                    // FIXME: Send an enum to the frontend for localization?
                    String warning = title.isEmpty() ? "Unnamed Profile already exists"_s : makeString("Profile \"", title, "\" already exists");
                    consoleAgent->addMessageToConsole(std::make_unique<ConsoleMessage>(MessageSource::ConsoleAPI, MessageType::Profile, MessageLevel::Warning, warning));
                }
                return;
            }
        }
    }

    if (!m_enabled && m_pendingConsoleProfileRecords.isEmpty())
        startProgrammaticCapture();

    m_pendingConsoleProfileRecords.append(createRecordEntry(TimelineRecordFactory::createConsoleProfileData(title), TimelineRecordType::ConsoleProfile, true, frameFromExecState(exec)));
}

void InspectorTimelineAgent::stopFromConsole(JSC::ExecState*, const String& title)
{
    // Stop profiles in reverse order. If the title is empty, then stop the last profile.
    // Otherwise, match the title of the profile to stop.
    for (int i = m_pendingConsoleProfileRecords.size() - 1; i >= 0; --i) {
        const TimelineRecordEntry& record = m_pendingConsoleProfileRecords[i];

        String recordTitle;
        record.data->getString("title"_s, recordTitle);
        if (title.isEmpty() || recordTitle == title) {
            didCompleteRecordEntry(record);
            m_pendingConsoleProfileRecords.remove(i);

            if (!m_enabledFromFrontend && m_pendingConsoleProfileRecords.isEmpty())
                stopProgrammaticCapture();

            return;
        }
    }

    if (WebConsoleAgent* consoleAgent = m_instrumentingAgents.webConsoleAgent()) {
        // FIXME: Send an enum to the frontend for localization?
        String warning = title.isEmpty() ? "No profiles exist"_s : makeString("Profile \"", title, "\" does not exist");
        consoleAgent->addMessageToConsole(std::make_unique<ConsoleMessage>(MessageSource::ConsoleAPI, MessageType::ProfileEnd, MessageLevel::Warning, warning));
    }
}

void InspectorTimelineAgent::willCallFunction(const String& scriptName, int scriptLine, int scriptColumn, Frame* frame)
{
    pushCurrentRecord(TimelineRecordFactory::createFunctionCallData(scriptName, scriptLine, scriptColumn), TimelineRecordType::FunctionCall, true, frame);
}

void InspectorTimelineAgent::didCallFunction(Frame*)
{
    didCompleteCurrentRecord(TimelineRecordType::FunctionCall);
}

void InspectorTimelineAgent::willDispatchEvent(const Event& event, Frame* frame)
{
    pushCurrentRecord(TimelineRecordFactory::createEventDispatchData(event), TimelineRecordType::EventDispatch, false, frame);
}

void InspectorTimelineAgent::didDispatchEvent()
{
    didCompleteCurrentRecord(TimelineRecordType::EventDispatch);
}

void InspectorTimelineAgent::didInvalidateLayout(Frame& frame)
{
    appendRecord(JSON::Object::create(), TimelineRecordType::InvalidateLayout, true, &frame);
}

void InspectorTimelineAgent::willLayout(Frame& frame)
{
    pushCurrentRecord(JSON::Object::create(), TimelineRecordType::Layout, true, &frame);
}

void InspectorTimelineAgent::didLayout(RenderObject& root)
{
    if (m_recordStack.isEmpty())
        return;
    TimelineRecordEntry& entry = m_recordStack.last();
    ASSERT(entry.type == TimelineRecordType::Layout);
    Vector<FloatQuad> quads;
    root.absoluteQuads(quads);
    if (quads.size() >= 1)
        TimelineRecordFactory::appendLayoutRoot(entry.data.get(), quads[0]);
    else
        ASSERT_NOT_REACHED();
    didCompleteCurrentRecord(TimelineRecordType::Layout);
}

void InspectorTimelineAgent::didScheduleStyleRecalculation(Frame* frame)
{
    appendRecord(JSON::Object::create(), TimelineRecordType::ScheduleStyleRecalculation, true, frame);
}

void InspectorTimelineAgent::willRecalculateStyle(Frame* frame)
{
    pushCurrentRecord(JSON::Object::create(), TimelineRecordType::RecalculateStyles, true, frame);
}

void InspectorTimelineAgent::didRecalculateStyle()
{
    didCompleteCurrentRecord(TimelineRecordType::RecalculateStyles);
}

void InspectorTimelineAgent::willComposite(Frame& frame)
{
    ASSERT(!m_startedComposite);
    pushCurrentRecord(JSON::Object::create(), TimelineRecordType::Composite, true, &frame);
    m_startedComposite = true;
}

void InspectorTimelineAgent::didComposite()
{
    ASSERT(m_startedComposite);
    didCompleteCurrentRecord(TimelineRecordType::Composite);
    m_startedComposite = false;
}

void InspectorTimelineAgent::willPaint(Frame& frame)
{
    pushCurrentRecord(JSON::Object::create(), TimelineRecordType::Paint, true, &frame);
}

void InspectorTimelineAgent::didPaint(RenderObject& renderer, const LayoutRect& clipRect)
{
    TimelineRecordEntry& entry = m_recordStack.last();
    ASSERT(entry.type == TimelineRecordType::Paint);
    FloatQuad quad;
    localToPageQuad(renderer, clipRect, &quad);
    entry.data = TimelineRecordFactory::createPaintData(quad);
    didCompleteCurrentRecord(TimelineRecordType::Paint);
}

void InspectorTimelineAgent::didInstallTimer(int timerId, Seconds timeout, bool singleShot, Frame* frame)
{
    appendRecord(TimelineRecordFactory::createTimerInstallData(timerId, timeout, singleShot), TimelineRecordType::TimerInstall, true, frame);
}

void InspectorTimelineAgent::didRemoveTimer(int timerId, Frame* frame)
{
    appendRecord(TimelineRecordFactory::createGenericTimerData(timerId), TimelineRecordType::TimerRemove, true, frame);
}

void InspectorTimelineAgent::willFireTimer(int timerId, Frame* frame)
{
    pushCurrentRecord(TimelineRecordFactory::createGenericTimerData(timerId), TimelineRecordType::TimerFire, false, frame);
}

void InspectorTimelineAgent::didFireTimer()
{
    didCompleteCurrentRecord(TimelineRecordType::TimerFire);
}

void InspectorTimelineAgent::willEvaluateScript(const String& url, int lineNumber, int columnNumber, Frame& frame)
{
    pushCurrentRecord(TimelineRecordFactory::createEvaluateScriptData(url, lineNumber, columnNumber), TimelineRecordType::EvaluateScript, true, &frame);
}

void InspectorTimelineAgent::didEvaluateScript(Frame&)
{
    didCompleteCurrentRecord(TimelineRecordType::EvaluateScript);
}

void InspectorTimelineAgent::didTimeStamp(Frame& frame, const String& message)
{
    appendRecord(TimelineRecordFactory::createTimeStampData(message), TimelineRecordType::TimeStamp, true, &frame);
}

void InspectorTimelineAgent::time(Frame& frame, const String& message)
{
    appendRecord(TimelineRecordFactory::createTimeStampData(message), TimelineRecordType::Time, true, &frame);
}

void InspectorTimelineAgent::timeEnd(Frame& frame, const String& message)
{
    appendRecord(TimelineRecordFactory::createTimeStampData(message), TimelineRecordType::TimeEnd, true, &frame);
}

void InspectorTimelineAgent::mainFrameStartedLoading()
{
    if (m_enabled)
        return;

    if (!m_autoCaptureEnabled)
        return;

    if (m_instruments.isEmpty())
        return;

    m_autoCapturePhase = AutoCapturePhase::BeforeLoad;

    // Pre-emptively disable breakpoints. The frontend must re-enable them.
    if (InspectorDebuggerAgent* debuggerAgent = m_instrumentingAgents.inspectorDebuggerAgent()) {
        ErrorString unused;
        debuggerAgent->setBreakpointsActive(unused, false);
    }

    // Inform the frontend we started an auto capture. The frontend must stop capture.
    m_frontendDispatcher->autoCaptureStarted();

    toggleInstruments(InstrumentState::Start);
}

void InspectorTimelineAgent::mainFrameNavigated()
{
    if (m_autoCapturePhase == AutoCapturePhase::BeforeLoad) {
        m_autoCapturePhase = AutoCapturePhase::FirstNavigation;
        toggleInstruments(InstrumentState::Start);
        m_autoCapturePhase = AutoCapturePhase::AfterFirstNavigation;
    }
}

void InspectorTimelineAgent::startProgrammaticCapture()
{
    ASSERT(!m_enabled);

    // Disable breakpoints during programmatic capture.
    if (InspectorDebuggerAgent* debuggerAgent = m_instrumentingAgents.inspectorDebuggerAgent()) {
        m_programmaticCaptureRestoreBreakpointActiveValue = debuggerAgent->breakpointsActive();
        if (m_programmaticCaptureRestoreBreakpointActiveValue) {
            ErrorString unused;
            debuggerAgent->setBreakpointsActive(unused, false);
        }
    } else
        m_programmaticCaptureRestoreBreakpointActiveValue = false;

    m_frontendDispatcher->programmaticCaptureStarted();

    toggleScriptProfilerInstrument(InstrumentState::Start); // Ensure JavaScript samping data.
    toggleTimelineInstrument(InstrumentState::Start); // Ensure Console Profile event records.
    toggleInstruments(InstrumentState::Start); // Any other instruments the frontend wants us to record.
}

void InspectorTimelineAgent::stopProgrammaticCapture()
{
    ASSERT(m_enabled);
    ASSERT(!m_enabledFromFrontend);

    toggleInstruments(InstrumentState::Stop);
    toggleTimelineInstrument(InstrumentState::Stop);
    toggleScriptProfilerInstrument(InstrumentState::Stop);

    // Re-enable breakpoints if they were enabled.
    if (m_programmaticCaptureRestoreBreakpointActiveValue) {
        if (InspectorDebuggerAgent* debuggerAgent = m_instrumentingAgents.inspectorDebuggerAgent()) {
            ErrorString unused;
            debuggerAgent->setBreakpointsActive(unused, true);
        }
    }

    m_frontendDispatcher->programmaticCaptureStopped();
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
        }
    }
}

void InspectorTimelineAgent::toggleScriptProfilerInstrument(InstrumentState state)
{
    if (m_scriptProfilerAgent) {
        ErrorString unused;
        if (state == InstrumentState::Start) {
            const bool includeSamples = true;
            m_scriptProfilerAgent->startTracking(unused, &includeSamples);
        } else
            m_scriptProfilerAgent->stopTracking(unused);
    }
}

void InspectorTimelineAgent::toggleHeapInstrument(InstrumentState state)
{
    if (m_heapAgent) {
        ErrorString unused;
        if (state == InstrumentState::Start) {
            if (m_autoCapturePhase == AutoCapturePhase::None || m_autoCapturePhase == AutoCapturePhase::FirstNavigation)
                m_heapAgent->startTracking(unused);
        } else
            m_heapAgent->stopTracking(unused);
    }
}

void InspectorTimelineAgent::toggleCPUInstrument(InstrumentState state)
{
#if ENABLE(RESOURCE_USAGE)
    if (InspectorCPUProfilerAgent* cpuProfilerAgent = m_instrumentingAgents.inspectorCPUProfilerAgent()) {
        ErrorString unused;
        if (state == InstrumentState::Start)
            cpuProfilerAgent->startTracking(unused);
        else
            cpuProfilerAgent->stopTracking(unused);
    }
#else
    UNUSED_PARAM(state);
#endif
}

void InspectorTimelineAgent::toggleMemoryInstrument(InstrumentState state)
{
#if ENABLE(RESOURCE_USAGE)
    if (InspectorMemoryAgent* memoryAgent = m_instrumentingAgents.inspectorMemoryAgent()) {
        ErrorString unused;
        if (state == InstrumentState::Start)
            memoryAgent->startTracking(unused);
        else
            memoryAgent->stopTracking(unused);
    }
#else
    UNUSED_PARAM(state);
#endif
}

void InspectorTimelineAgent::toggleTimelineInstrument(InstrumentState state)
{
    if (state == InstrumentState::Start)
        internalStart();
    else
        internalStop();
}

void InspectorTimelineAgent::didCommitLoad()
{
    clearRecordStack();
}

void InspectorTimelineAgent::didRequestAnimationFrame(int callbackId, Frame* frame)
{
    appendRecord(TimelineRecordFactory::createAnimationFrameData(callbackId), TimelineRecordType::RequestAnimationFrame, true, frame);
}

void InspectorTimelineAgent::didCancelAnimationFrame(int callbackId, Frame* frame)
{
    appendRecord(TimelineRecordFactory::createAnimationFrameData(callbackId), TimelineRecordType::CancelAnimationFrame, true, frame);
}

void InspectorTimelineAgent::willFireAnimationFrame(int callbackId, Frame* frame)
{
    pushCurrentRecord(TimelineRecordFactory::createAnimationFrameData(callbackId), TimelineRecordType::FireAnimationFrame, false, frame);
}

void InspectorTimelineAgent::didFireAnimationFrame()
{
    didCompleteCurrentRecord(TimelineRecordType::FireAnimationFrame);
}

void InspectorTimelineAgent::willFireObserverCallback(const String& callbackType, Frame* frame)
{
    pushCurrentRecord(TimelineRecordFactory::createObserverCallbackData(callbackType), TimelineRecordType::ObserverCallback, false, frame);
}

void InspectorTimelineAgent::didFireObserverCallback()
{
    didCompleteCurrentRecord(TimelineRecordType::ObserverCallback);
}

// ScriptDebugListener

void InspectorTimelineAgent::breakpointActionProbe(JSC::ExecState& state, const Inspector::ScriptBreakpointAction& action, unsigned /*batchId*/, unsigned sampleId, JSC::JSValue)
{
    appendRecord(TimelineRecordFactory::createProbeSampleData(action, sampleId), TimelineRecordType::ProbeSample, false, frameFromExecState(&state));
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
    }

    return Inspector::Protocol::Timeline::EventType::TimeStamp;
}

void InspectorTimelineAgent::addRecordToTimeline(RefPtr<JSON::Object>&& record, TimelineRecordType type)
{
    ASSERT_ARG(record, record);
    record->setString("type", Inspector::Protocol::InspectorHelpers::getEnumConstantValue(toProtocol(type)));

    if (m_recordStack.isEmpty()) {
        auto recordObject = BindingTraits<Inspector::Protocol::Timeline::TimelineEvent>::runtimeCast(WTFMove(record));
        sendEvent(WTFMove(recordObject));
    } else {
        const TimelineRecordEntry& parent = m_recordStack.last();
        // Nested paint records are an implementation detail and add no information not already contained in the parent.
        if (type == TimelineRecordType::Paint && parent.type == type)
            return;

        parent.children->pushObject(WTFMove(record));
    }
}

void InspectorTimelineAgent::setFrameIdentifier(JSON::Object* record, Frame* frame)
{
    if (!frame || !m_pageAgent)
        return;
    String frameId;
    if (frame && m_pageAgent)
        frameId = m_pageAgent->frameId(frame);
    record->setString("frameId", frameId);
}

void InspectorTimelineAgent::didCompleteRecordEntry(const TimelineRecordEntry& entry)
{
    entry.record->setObject("data"_s, entry.data);
    entry.record->setArray("children"_s, entry.children);
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

void InspectorTimelineAgent::appendRecord(RefPtr<JSON::Object>&& data, TimelineRecordType type, bool captureCallStack, Frame* frame)
{
    Ref<JSON::Object> record = TimelineRecordFactory::createGenericRecord(timestamp(), captureCallStack ? m_maxCallStackDepth : 0);
    record->setObject("data", WTFMove(data));
    setFrameIdentifier(&record.get(), frame);
    addRecordToTimeline(WTFMove(record), type);
}

void InspectorTimelineAgent::sendEvent(RefPtr<JSON::Object>&& event)
{
    // FIXME: runtimeCast is a hack. We do it because we can't build TimelineEvent directly now.
    auto recordChecked = BindingTraits<Inspector::Protocol::Timeline::TimelineEvent>::runtimeCast(WTFMove(event));
    m_frontendDispatcher->eventRecorded(WTFMove(recordChecked));
}

InspectorTimelineAgent::TimelineRecordEntry InspectorTimelineAgent::createRecordEntry(RefPtr<JSON::Object>&& data, TimelineRecordType type, bool captureCallStack, Frame* frame)
{
    Ref<JSON::Object> record = TimelineRecordFactory::createGenericRecord(timestamp(), captureCallStack ? m_maxCallStackDepth : 0);
    setFrameIdentifier(&record.get(), frame);
    return TimelineRecordEntry(WTFMove(record), WTFMove(data), JSON::Array::create(), type);
}

void InspectorTimelineAgent::pushCurrentRecord(RefPtr<JSON::Object>&& data, TimelineRecordType type, bool captureCallStack, Frame* frame)
{
    pushCurrentRecord(createRecordEntry(WTFMove(data), type, captureCallStack, frame));
}

void InspectorTimelineAgent::clearRecordStack()
{
    m_recordStack.clear();
    m_id++;
}

void InspectorTimelineAgent::localToPageQuad(const RenderObject& renderer, const LayoutRect& rect, FloatQuad* quad)
{
    const FrameView& frameView = renderer.view().frameView();
    FloatQuad absolute = renderer.localToAbsoluteQuad(FloatQuad(rect));
    quad->setP1(frameView.contentsToRootView(roundedIntPoint(absolute.p1())));
    quad->setP2(frameView.contentsToRootView(roundedIntPoint(absolute.p2())));
    quad->setP3(frameView.contentsToRootView(roundedIntPoint(absolute.p3())));
    quad->setP4(frameView.contentsToRootView(roundedIntPoint(absolute.p4())));
}

} // namespace WebCore
