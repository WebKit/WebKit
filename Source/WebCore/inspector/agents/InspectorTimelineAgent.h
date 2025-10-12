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
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebCore {

class Event;
class FloatQuad;
class RenderObject;

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

    Screenshot,
};

class InspectorTimelineAgent : public InspectorAgentBase, public Inspector::TimelineBackendDispatcherHandler, public JSC::Debugger::Observer {
    WTF_MAKE_NONCOPYABLE(InspectorTimelineAgent);
    WTF_MAKE_TZONE_ALLOCATED(InspectorTimelineAgent);
public:
    InspectorTimelineAgent(WebAgentContext&);
    ~InspectorTimelineAgent();

    // InspectorAgentBase
    void didCreateFrontendAndBackend();
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason);

    // TimelineBackendDispatcherHandler
    Inspector::Protocol::ErrorStringOr<void> enable();
    Inspector::Protocol::ErrorStringOr<void> disable();
    Inspector::Protocol::ErrorStringOr<void> start(std::optional<int>&& maxCallStackDepth);
    Inspector::Protocol::ErrorStringOr<void> stop();
    Inspector::Protocol::ErrorStringOr<void> setInstruments(Ref<JSON::Array>&&);

    // JSC::Debugger::Observer
    void breakpointActionProbe(JSC::JSGlobalObject*, JSC::BreakpointActionID, unsigned batchId, unsigned sampleId, JSC::JSValue result);

    // InspectorInstrumentation
    void didInstallTimer(int timerId, Seconds timeout, bool singleShot);
    void didRemoveTimer(int timerId);
    void willFireTimer(int timerId);
    void didFireTimer();
    void willCallFunction(const String& scriptName, int scriptLine, int scriptColumn);
    void didCallFunction();
    void willDispatchEvent(const Event&);
    void didDispatchEvent(bool defaultPrevented);
    void willEvaluateScript(const String& url, int lineNumber, int columnNumber);
    void didEvaluateScript();
    void didTimeStamp(const String& message);
    void didPerformanceMark(const String& label, std::optional<MonotonicTime>);
    void didRequestAnimationFrame(int callbackId);
    void didCancelAnimationFrame(int callbackId);
    void willFireAnimationFrame(int callbackId);
    void didFireAnimationFrame();
    void willFireObserverCallback(const String& callbackType);
    void didFireObserverCallback();
    void time(const String& label);
    void timeEnd(const String& label);

    // Console
    void startFromConsole(const String& title);
    void stopFromConsole(const String& title);

protected:
    virtual bool enabled() const;
    virtual void internalEnable();
    virtual void internalDisable();

    virtual bool tracking() const;
    virtual void internalStart(std::optional<int>&& maxCallStackDepth);
    virtual void internalStop();

    virtual bool shouldStartHeapInstrument() const { return true; }
    void autoCaptureStarted() const;

    const Vector<Inspector::Protocol::Timeline::Instrument>& instruments() const { return m_instruments; }

    enum class InstrumentState { Start, Stop };
    void toggleInstruments(InstrumentState);

    double timestamp();

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
    TimelineRecordEntry* lastRecordEntry();

    void appendRecord(Ref<JSON::Object>&& data, TimelineRecordType, bool captureCallStack, std::optional<double> startTime = std::nullopt);
    void pushCurrentRecord(Ref<JSON::Object>&&, TimelineRecordType, bool captureCallStack, std::optional<double> startTime = std::nullopt);
    void didCompleteCurrentRecord(TimelineRecordType);

private:
    void startProgrammaticCapture();
    void stopProgrammaticCapture();

    void toggleScriptProfilerInstrument(InstrumentState);
    void toggleHeapInstrument(InstrumentState);
    void toggleCPUInstrument(InstrumentState);
    void toggleMemoryInstrument(InstrumentState);
    void toggleTimelineInstrument(InstrumentState);
    void toggleAnimationInstrument(InstrumentState);
    void disableBreakpoints();
    void enableBreakpoints();

    std::optional<double> timestampFromMonotonicTime(MonotonicTime);

    void sendEvent(Ref<JSON::Object>&&);
    void pushCurrentRecord(const TimelineRecordEntry& record) { m_recordStack.append(record); }

    TimelineRecordEntry createRecordEntry(Ref<JSON::Object>&& data, TimelineRecordType, bool captureCallStack, std::optional<double> startTime = std::nullopt);

    void didCompleteRecordEntry(const TimelineRecordEntry&);

    void addRecordToTimeline(Ref<JSON::Object>&&, TimelineRecordType);

    const UniqueRef<Inspector::TimelineFrontendDispatcher> m_frontendDispatcher;
    const Ref<Inspector::TimelineBackendDispatcher> m_backendDispatcher;

    Vector<TimelineRecordEntry> m_recordStack;
    Vector<TimelineRecordEntry> m_pendingConsoleProfileRecords;

    int m_maxCallStackDepth { 5 };

    bool m_trackingFromFrontend { false };
    bool m_programmaticCaptureRestoreBreakpointActiveValue { false };

    Vector<Inspector::Protocol::Timeline::Instrument> m_instruments;
};

} // namespace WebCore
