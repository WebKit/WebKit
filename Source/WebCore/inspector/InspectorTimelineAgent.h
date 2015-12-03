/*
* Copyright (C) 2012 Google Inc. All rights reserved.
* Copyright (C) 2014 University of Washington.
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

#ifndef InspectorTimelineAgent_h
#define InspectorTimelineAgent_h

#include "InspectorWebAgentBase.h"
#include "LayoutRect.h"
#include <inspector/InspectorBackendDispatchers.h>
#include <inspector/InspectorFrontendDispatchers.h>
#include <inspector/InspectorValues.h>
#include <inspector/ScriptDebugListener.h>
#include <wtf/Vector.h>

namespace JSC {
class Profile;
}

namespace WebCore {

class Event;
class FloatQuad;
class Frame;
class InspectorClient;
class InspectorPageAgent;
class InstrumentingAgents;
class IntRect;
class URL;
class Page;
class PageScriptDebugServer;
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
    ScrollLayer,

    TimerInstall,
    TimerRemove,
    TimerFire,

    EvaluateScript,

    TimeStamp,
    Time,
    TimeEnd,

    XHRReadyStateChange,
    XHRLoad,

    FunctionCall,
    ProbeSample,
    ConsoleProfile,

    RequestAnimationFrame,
    CancelAnimationFrame,
    FireAnimationFrame,

    WebSocketCreate,
    WebSocketSendHandshakeRequest,
    WebSocketReceiveHandshakeResponse,
    WebSocketDestroy
};

class InspectorTimelineAgent final
    : public InspectorAgentBase
    , public Inspector::TimelineBackendDispatcherHandler
    , public Inspector::ScriptDebugListener {
    WTF_MAKE_NONCOPYABLE(InspectorTimelineAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum InspectorType { PageInspector, WorkerInspector };

    InspectorTimelineAgent(InstrumentingAgents*, InspectorPageAgent*, InspectorType, InspectorClient*);
    virtual ~InspectorTimelineAgent();

    virtual void didCreateFrontendAndBackend(Inspector::FrontendChannel*, Inspector::BackendDispatcher*) override;
    virtual void willDestroyFrontendAndBackend(Inspector::DisconnectReason) override;

    virtual void start(ErrorString&, const int* maxCallStackDepth = nullptr) override;
    virtual void stop(ErrorString&) override;

    int id() const { return m_id; }

    void setPageScriptDebugServer(PageScriptDebugServer*);

    void didCommitLoad();

    // Methods called from WebCore.
    void startFromConsole(JSC::ExecState*, const String &title);
    PassRefPtr<JSC::Profile> stopFromConsole(JSC::ExecState*, const String& title);

    // InspectorInstrumentation callbacks.
    void didInstallTimer(int timerId, int timeout, bool singleShot, Frame*);
    void didRemoveTimer(int timerId, Frame*);
    void willFireTimer(int timerId, Frame*);
    void didFireTimer();
    void willCallFunction(const String& scriptName, int scriptLine, Frame*);
    void didCallFunction(Frame*);
    void willDispatchXHRReadyStateChangeEvent(const String&, int, Frame*);
    void didDispatchXHRReadyStateChangeEvent();
    void willDispatchEvent(const Event&, Frame*);
    void didDispatchEvent();
    void willEvaluateScript(const String&, int, Frame&);
    void didEvaluateScript(Frame&);
    void didInvalidateLayout(Frame&);
    void willLayout(Frame&);
    void didLayout(RenderObject*);
    void willScroll(Frame&);
    void didScroll();
    void willDispatchXHRLoadEvent(const String&, Frame*);
    void didDispatchXHRLoadEvent();
    void willComposite(Frame&);
    void didComposite();
    void willPaint(Frame&);
    void didPaint(RenderObject*, const LayoutRect&);
    void willRecalculateStyle(Frame*);
    void didRecalculateStyle();
    void didScheduleStyleRecalculation(Frame*);
    void didTimeStamp(Frame&, const String&);
    void didRequestAnimationFrame(int callbackId, Frame*);
    void didCancelAnimationFrame(int callbackId, Frame*);
    void willFireAnimationFrame(int callbackId, Frame*);
    void didFireAnimationFrame();
    void time(Frame&, const String&);
    void timeEnd(Frame&, const String&);
#if ENABLE(WEB_SOCKETS)
    void didCreateWebSocket(unsigned long identifier, const URL&, const String& protocol, Frame*);
    void willSendWebSocketHandshakeRequest(unsigned long identifier, Frame*);
    void didReceiveWebSocketHandshakeResponse(unsigned long identifier, Frame*);
    void didDestroyWebSocket(unsigned long identifier, Frame*);
#endif

protected:
    // ScriptDebugListener
    virtual void didParseSource(JSC::SourceID, const Script&) override { }
    virtual void failedToParseSource(const String&, const String&, int, int, const String&) override { }
    virtual void didPause(JSC::ExecState*, const Deprecated::ScriptValue&, const Deprecated::ScriptValue&) override { }
    virtual void didContinue() override { }

    virtual void breakpointActionLog(JSC::ExecState*, const String&) override { }
    virtual void breakpointActionSound(int) override { }
    virtual void breakpointActionProbe(JSC::ExecState*, const Inspector::ScriptBreakpointAction&, unsigned batchId, unsigned sampleId, const Deprecated::ScriptValue& result) override;

private:
    friend class TimelineRecordStack;

    struct TimelineRecordEntry {
        TimelineRecordEntry()
            : type(TimelineRecordType::EventDispatch) { }
        TimelineRecordEntry(PassRefPtr<Inspector::InspectorObject> record, PassRefPtr<Inspector::InspectorObject> data, PassRefPtr<Inspector::InspectorArray> children, TimelineRecordType type)
            : record(record), data(data), children(children), type(type)
        {
        }

        RefPtr<Inspector::InspectorObject> record;
        RefPtr<Inspector::InspectorObject> data;
        RefPtr<Inspector::InspectorArray> children;
        TimelineRecordType type;
    };

    void internalStart(const int* maxCallStackDepth = nullptr);
    void internalStop();
    double timestamp();

    void sendEvent(RefPtr<Inspector::InspectorObject>&&);
    void appendRecord(RefPtr<Inspector::InspectorObject>&& data, TimelineRecordType, bool captureCallStack, Frame*);
    void pushCurrentRecord(RefPtr<Inspector::InspectorObject>&&, TimelineRecordType, bool captureCallStack, Frame*);
    void pushCurrentRecord(const TimelineRecordEntry& record) { m_recordStack.append(record); }

    TimelineRecordEntry createRecordEntry(RefPtr<Inspector::InspectorObject>&& data, TimelineRecordType, bool captureCallStack, Frame*);

    void setFrameIdentifier(Inspector::InspectorObject* record, Frame*);

    void didCompleteRecordEntry(const TimelineRecordEntry&);
    void didCompleteCurrentRecord(TimelineRecordType);

    void addRecordToTimeline(RefPtr<Inspector::InspectorObject>&&, TimelineRecordType);
    void clearRecordStack();

    void localToPageQuad(const RenderObject&, const LayoutRect&, FloatQuad*);
    Page* page();

    InspectorPageAgent* m_pageAgent;
    PageScriptDebugServer* m_scriptDebugServer { nullptr };

    std::unique_ptr<Inspector::TimelineFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::TimelineBackendDispatcher> m_backendDispatcher;

    Vector<TimelineRecordEntry> m_recordStack;

    int m_id { 1 };
    int m_callStackDepth { 0 };
    int m_maxCallStackDepth { 5 };
    InspectorType m_inspectorType;
    InspectorClient* m_client;

    Vector<TimelineRecordEntry> m_pendingConsoleProfileRecords;

    bool m_enabled { false };
    bool m_enabledFromFrontend { false };

#if PLATFORM(COCOA)
    std::unique_ptr<WebCore::RunLoopObserver> m_frameStartObserver;
    std::unique_ptr<WebCore::RunLoopObserver> m_frameStopObserver;
#endif
    int m_runLoopNestingLevel { 0 };
    bool m_startedComposite { false };
};

} // namespace WebCore

#endif // !defined(InspectorTimelineAgent_h)
