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
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <JavaScriptCore/ScriptDebugListener.h>
#include <wtf/JSONValues.h>
#include <wtf/Vector.h>

namespace Inspector {
class InspectorHeapAgent;
class InspectorScriptProfilerAgent;
}

namespace WebCore {

class Event;
class FloatQuad;
class Frame;
class InspectorPageAgent;
class RenderObject;
class RunLoopObserver;

typedef String ErrorString;

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

class InspectorTimelineAgent final
    : public InspectorAgentBase
    , public Inspector::TimelineBackendDispatcherHandler
    , public Inspector::ScriptDebugListener {
    WTF_MAKE_NONCOPYABLE(InspectorTimelineAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorTimelineAgent(WebAgentContext&, Inspector::InspectorScriptProfilerAgent*, Inspector::InspectorHeapAgent*, InspectorPageAgent*);
    virtual ~InspectorTimelineAgent();

    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*) final;
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason) final;

    void start(ErrorString&, const int* maxCallStackDepth = nullptr) final;
    void stop(ErrorString&) final;
    void setAutoCaptureEnabled(ErrorString&, bool) final;
    void setInstruments(ErrorString&, const JSON::Array&) final;

    int id() const { return m_id; }

    void didCommitLoad();

    // Methods called from WebCore.
    void startFromConsole(JSC::ExecState*, const String& title);
    void stopFromConsole(JSC::ExecState*, const String& title);

    // InspectorInstrumentation
    void didInstallTimer(int timerId, Seconds timeout, bool singleShot, Frame*);
    void didRemoveTimer(int timerId, Frame*);
    void willFireTimer(int timerId, Frame*);
    void didFireTimer();
    void willCallFunction(const String& scriptName, int scriptLine, Frame*);
    void didCallFunction(Frame*);
    void willDispatchEvent(const Event&, Frame*);
    void didDispatchEvent();
    void willEvaluateScript(const String&, int, Frame&);
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

private:
    // ScriptDebugListener
    void didParseSource(JSC::SourceID, const Script&) final { }
    void failedToParseSource(const String&, const String&, int, int, const String&) final { }
    void didPause(JSC::ExecState&, JSC::JSValue, JSC::JSValue) final { }
    void didContinue() final { }

    void breakpointActionLog(JSC::ExecState&, const String&) final { }
    void breakpointActionSound(int) final { }
    void breakpointActionProbe(JSC::ExecState&, const Inspector::ScriptBreakpointAction&, unsigned batchId, unsigned sampleId, JSC::JSValue result) final;

    void startProgrammaticCapture();
    void stopProgrammaticCapture();

    enum class InstrumentState { Start, Stop };
    void toggleInstruments(InstrumentState);
    void toggleScriptProfilerInstrument(InstrumentState);
    void toggleHeapInstrument(InstrumentState);
    void toggleMemoryInstrument(InstrumentState);
    void toggleTimelineInstrument(InstrumentState);
    void disableBreakpoints();
    void enableBreakpoints();

    friend class TimelineRecordStack;

    struct TimelineRecordEntry {
        TimelineRecordEntry()
            : type(TimelineRecordType::EventDispatch) { }
        TimelineRecordEntry(RefPtr<JSON::Object>&& record, RefPtr<JSON::Object>&& data, RefPtr<JSON::Array>&& children, TimelineRecordType type)
            : record(WTFMove(record))
            , data(WTFMove(data))
            , children(WTFMove(children))
            , type(type)
        {
        }

        RefPtr<JSON::Object> record;
        RefPtr<JSON::Object> data;
        RefPtr<JSON::Array> children;
        TimelineRecordType type;
    };

    void internalStart(const int* maxCallStackDepth = nullptr);
    void internalStop();
    double timestamp();

    void sendEvent(RefPtr<JSON::Object>&&);
    void appendRecord(RefPtr<JSON::Object>&& data, TimelineRecordType, bool captureCallStack, Frame*);
    void pushCurrentRecord(RefPtr<JSON::Object>&&, TimelineRecordType, bool captureCallStack, Frame*);
    void pushCurrentRecord(const TimelineRecordEntry& record) { m_recordStack.append(record); }

    TimelineRecordEntry createRecordEntry(RefPtr<JSON::Object>&& data, TimelineRecordType, bool captureCallStack, Frame*);

    void setFrameIdentifier(JSON::Object* record, Frame*);

    void didCompleteRecordEntry(const TimelineRecordEntry&);
    void didCompleteCurrentRecord(TimelineRecordType);

    void addRecordToTimeline(RefPtr<JSON::Object>&&, TimelineRecordType);
    void clearRecordStack();

    void localToPageQuad(const RenderObject&, const LayoutRect&, FloatQuad*);

    std::unique_ptr<Inspector::TimelineFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::TimelineBackendDispatcher> m_backendDispatcher;
    Inspector::InspectorScriptProfilerAgent* m_scriptProfilerAgent;
    Inspector::InspectorHeapAgent* m_heapAgent;
    InspectorPageAgent* m_pageAgent;

    Vector<TimelineRecordEntry> m_recordStack;
    Vector<TimelineRecordEntry> m_pendingConsoleProfileRecords;

    int m_id { 1 };
    int m_maxCallStackDepth { 5 };

    bool m_enabled { false };
    bool m_enabledFromFrontend { false };
    bool m_programmaticCaptureRestoreBreakpointActiveValue { false };

    bool m_autoCaptureEnabled { false };
    enum class AutoCapturePhase { None, BeforeLoad, FirstNavigation, AfterFirstNavigation };
    AutoCapturePhase m_autoCapturePhase { AutoCapturePhase::None };
    Vector<Inspector::Protocol::Timeline::Instrument> m_instruments;

#if PLATFORM(COCOA)
    std::unique_ptr<WebCore::RunLoopObserver> m_frameStartObserver;
    std::unique_ptr<WebCore::RunLoopObserver> m_frameStopObserver;
#endif
    int m_runLoopNestingLevel { 0 };
    bool m_startedComposite { false };
};

} // namespace WebCore
