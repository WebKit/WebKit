/*
* Copyright (C) 2012 Google Inc. All rights reserved.
* Copyright (C) 2014 University of Washington.
* Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#include "InspectorWebAgentBase.h"
#include "LayoutRect.h"
#include <JavaScriptCore/Debugger.h>
#include <JavaScriptCore/DebuggerPrimitives.h>
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <wtf/JSONValues.h>
#include <wtf/RunLoop.h>
#include <wtf/Vector.h>

namespace WebCore {

class Event;
class FloatQuad;
class Frame;
class RenderObject;
class RunLoopObserver;

enum class TimelineRecordType {
    EventDispatch,
    ScheduleStyleRecalculation,
    RecalculateStyles,
    InvalidateLayout,
    Layout,
    Paint,
    Composite,
    RenderingFrame,

    TimerInstall,
    TimerRemove,
    TimerFire,

    EvaluateScript,

    TimeStamp,
    Time,
    TimeEnd,

    FunctionCall,
    ProbeSample,
    ConsoleProfile,

    RequestAnimationFrame,
    CancelAnimationFrame,
    FireAnimationFrame,
    
    ObserverCallback,
};

class InspectorTimelineAgent final : public InspectorAgentBase , public Inspector::TimelineBackendDispatcherHandler , public JSC::Debugger::Observer {
    WTF_MAKE_NONCOPYABLE(InspectorTimelineAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorTimelineAgent(PageAgentContext&);
    ~InspectorTimelineAgent();

    // InspectorAgentBase
    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*);
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason);

    // TimelineBackendDispatcherHandler
    Inspector::Protocol::ErrorStringOr<void> enable();
    Inspector::Protocol::ErrorStringOr<void> disable();
    Inspector::Protocol::ErrorStringOr<void> start(std::optional<int>&& maxCallStackDepth);
    Inspector::Protocol::ErrorStringOr<void> stop();
    Inspector::Protocol::ErrorStringOr<void> setAutoCaptureEnabled(bool);
    Inspector::Protocol::ErrorStringOr<void> setInstruments(Ref<JSON::Array>&&);

    // JSC::Debugger::Observer
    void breakpointActionProbe(JSC::JSGlobalObject*, JSC::BreakpointActionID, unsigned batchId, unsigned sampleId, JSC::JSValue result);

    // InspectorInstrumentation
    void didInstallTimer(int timerId, Seconds timeout, bool singleShot, Frame*);
    void didRemoveTimer(int timerId, Frame*);
    void willFireTimer(int timerId, Frame*);
    void didFireTimer();
    void willCallFunction(const String& scriptName, int scriptLine, int scriptColumn, Frame*);
    void didCallFunction(Frame*);
    void willDispatchEvent(const Event&, Frame*);
    void didDispatchEvent(bool defaultPrevented);
    void willEvaluateScript(const String&, int lineNumber, int columnNumber, Frame&);
    void didEvaluateScript(Frame&);
    void didInvalidateLayout(Frame&);
    void willLayout(Frame&);
    void didLayout(RenderObject&);
    void willComposite(Frame&);
    void didComposite();
    void willPaint(Frame&);
    void didPaint(RenderObject&, const LayoutRect&);
    void willRecalculateStyle(Frame*);
    void didRecalculateStyle();
    void didScheduleStyleRecalculation(Frame*);
    void didTimeStamp(Frame&, const String&);
    void didRequestAnimationFrame(int callbackId, Frame*);
    void didCancelAnimationFrame(int callbackId, Frame*);
    void willFireAnimationFrame(int callbackId, Frame*);
    void didFireAnimationFrame();
    void willFireObserverCallback(const String& callbackType, Frame*);
    void didFireObserverCallback();
    void time(Frame&, const String&);
    void timeEnd(Frame&, const String&);
    void mainFrameStartedLoading();
    void mainFrameNavigated();

    // Console
    void startFromConsole(JSC::JSGlobalObject*, const String& title);
    void stopFromConsole(JSC::JSGlobalObject*, const String& title);

private:
    void startProgrammaticCapture();
    void stopProgrammaticCapture();

    enum class InstrumentState { Start, Stop };
    void toggleInstruments(InstrumentState);
    void toggleScriptProfilerInstrument(InstrumentState);
    void toggleHeapInstrument(InstrumentState);
    void toggleCPUInstrument(InstrumentState);
    void toggleMemoryInstrument(InstrumentState);
    void toggleTimelineInstrument(InstrumentState);
    void toggleAnimationInstrument(InstrumentState);
    void disableBreakpoints();
    void enableBreakpoints();

    friend class TimelineRecordStack;

    struct TimelineRecordEntry {
        TimelineRecordEntry(Ref<JSON::Object>&& record, Ref<JSON::Object>&& data, RefPtr<JSON::Array>&& children, TimelineRecordType type)
            : record(WTFMove(record))
            , data(WTFMove(data))
            , children(WTFMove(children))
            , type(type)
        {
        }

        Ref<JSON::Object> record;
        Ref<JSON::Object> data;
        RefPtr<JSON::Array> children;
        TimelineRecordType type;
    };

    void internalStart(std::optional<int>&& maxCallStackDepth);
    void internalStop();
    double timestamp();

    void sendEvent(Ref<JSON::Object>&&);
    void appendRecord(Ref<JSON::Object>&& data, TimelineRecordType, bool captureCallStack, Frame*);
    void pushCurrentRecord(Ref<JSON::Object>&&, TimelineRecordType, bool captureCallStack, Frame*);
    void pushCurrentRecord(const TimelineRecordEntry& record) { m_recordStack.append(record); }

    TimelineRecordEntry createRecordEntry(Ref<JSON::Object>&& data, TimelineRecordType, bool captureCallStack, Frame*);

    void setFrameIdentifier(JSON::Object* record, Frame*);

    void didCompleteRecordEntry(const TimelineRecordEntry&);
    void didCompleteCurrentRecord(TimelineRecordType);

    void addRecordToTimeline(Ref<JSON::Object>&&, TimelineRecordType);

    std::unique_ptr<Inspector::TimelineFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::TimelineBackendDispatcher> m_backendDispatcher;
    Page& m_inspectedPage;

    Vector<TimelineRecordEntry> m_recordStack;
    Vector<TimelineRecordEntry> m_pendingConsoleProfileRecords;

    int m_maxCallStackDepth { 5 };

    bool m_tracking { false };
    bool m_trackingFromFrontend { false };
    bool m_programmaticCaptureRestoreBreakpointActiveValue { false };

    bool m_autoCaptureEnabled { false };
    enum class AutoCapturePhase { None, BeforeLoad, FirstNavigation, AfterFirstNavigation };
    AutoCapturePhase m_autoCapturePhase { AutoCapturePhase::None };
    Vector<Inspector::Protocol::Timeline::Instrument> m_instruments;

#if PLATFORM(COCOA)
    std::unique_ptr<WebCore::RunLoopObserver> m_frameStartObserver;
    std::unique_ptr<WebCore::RunLoopObserver> m_frameStopObserver;
    int m_runLoopNestingLevel { 0 };
#elif USE(GLIB_EVENT_LOOP)
    std::unique_ptr<RunLoop::Observer> m_runLoopObserver;
#endif
    bool m_startedComposite { false };
};

} // namespace WebCore
