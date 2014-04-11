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

#if ENABLE(INSPECTOR)

#include "InspectorWebAgentBase.h"
#include "InspectorWebBackendDispatchers.h"
#include "InspectorWebFrontendDispatchers.h"
#include "LayoutRect.h"
#include <inspector/InspectorValues.h>
#include <inspector/ScriptDebugListener.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

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
class ResourceRequest;
class ResourceResponse;

typedef String ErrorString;

enum class TimelineRecordType {
    EventDispatch,
    ScheduleStyleRecalculation,
    RecalculateStyles,
    InvalidateLayout,
    Layout,
    Paint,
    ScrollLayer,
    ResizeImage,

    ParseHTML,

    TimerInstall,
    TimerRemove,
    TimerFire,

    EvaluateScript,

    MarkLoad,
    MarkDOMContent,

    TimeStamp,
    Time,
    TimeEnd,

    ScheduleResourceRequest,
    ResourceSendRequest,
    ResourceReceiveResponse,
    ResourceReceivedData,
    ResourceFinish,

    XHRReadyStateChange,
    XHRLoad,

    FunctionCall,
    ProbeSample,

    RequestAnimationFrame,
    CancelAnimationFrame,
    FireAnimationFrame,

    WebSocketCreate,
    WebSocketSendHandshakeRequest,
    WebSocketReceiveHandshakeResponse,
    WebSocketDestroy
};

class TimelineTimeConverter {
public:
    TimelineTimeConverter()
        : m_startOffset(0)
    {
    }
    double fromMonotonicallyIncreasingTime(double time) const  { return (time - m_startOffset) * 1000.0; }
    void reset();

private:
    double m_startOffset;
};

class InspectorTimelineAgent
    : public InspectorAgentBase
    , public Inspector::InspectorTimelineBackendDispatcherHandler
    , public Inspector::ScriptDebugListener {
    WTF_MAKE_NONCOPYABLE(InspectorTimelineAgent);
public:
    enum InspectorType { PageInspector, WorkerInspector };

    InspectorTimelineAgent(InstrumentingAgents*, InspectorPageAgent*, InspectorType, InspectorClient*);
    ~InspectorTimelineAgent();

    virtual void didCreateFrontendAndBackend(Inspector::InspectorFrontendChannel*, Inspector::InspectorBackendDispatcher*) override;
    virtual void willDestroyFrontendAndBackend(Inspector::InspectorDisconnectReason) override;

    virtual void start(ErrorString*, const int* maxCallStackDepth) override;
    virtual void stop(ErrorString*) override;

    int id() const { return m_id; }

    void setPageScriptDebugServer(PageScriptDebugServer*);

    void didCommitLoad();

    // Methods called from WebCore.
    void willCallFunction(const String& scriptName, int scriptLine, Frame*);
    void didCallFunction(Frame*);

    void willDispatchEvent(const Event&, Frame*);
    void didDispatchEvent();

    void didInvalidateLayout(Frame*);
    void willLayout(Frame*);
    void didLayout(RenderObject*);

    void didScheduleStyleRecalculation(Frame*);
    void willRecalculateStyle(Frame*);
    void didRecalculateStyle();

    void willPaint(Frame*);
    void didPaint(RenderObject*, const LayoutRect&);

    void willScroll(Frame*);
    void didScroll();

    void willWriteHTML(unsigned startLine, Frame*);
    void didWriteHTML(unsigned endLine);

    void didInstallTimer(int timerId, int timeout, bool singleShot, Frame*);
    void didRemoveTimer(int timerId, Frame*);
    void willFireTimer(int timerId, Frame*);
    void didFireTimer();

    void willDispatchXHRReadyStateChangeEvent(const String&, int, Frame*);
    void didDispatchXHRReadyStateChangeEvent();
    void willDispatchXHRLoadEvent(const String&, Frame*);
    void didDispatchXHRLoadEvent();

    void willEvaluateScript(const String&, int, Frame*);
    void didEvaluateScript(Frame*);

    void didTimeStamp(Frame*, const String&);
    void didMarkDOMContentEvent(Frame*);
    void didMarkLoadEvent(Frame*);

    void time(Frame*, const String&);
    void timeEnd(Frame*, const String&);

    void didScheduleResourceRequest(const String& url, Frame*);
    void willSendResourceRequest(unsigned long, const ResourceRequest&, Frame*);
    void willReceiveResourceResponse(unsigned long, const ResourceResponse&, Frame*);
    void didReceiveResourceResponse();
    void didFinishLoadingResource(unsigned long, bool didFail, double finishTime, Frame*);
    void willReceiveResourceData(unsigned long identifier, Frame*, int length);
    void didReceiveResourceData();

    void didRequestAnimationFrame(int callbackId, Frame*);
    void didCancelAnimationFrame(int callbackId, Frame*);
    void willFireAnimationFrame(int callbackId, Frame*);
    void didFireAnimationFrame();

#if ENABLE(WEB_SOCKETS)
    void didCreateWebSocket(unsigned long identifier, const URL&, const String& protocol, Frame*);
    void willSendWebSocketHandshakeRequest(unsigned long identifier, Frame*);
    void didReceiveWebSocketHandshakeResponse(unsigned long identifier, Frame*);
    void didDestroyWebSocket(unsigned long identifier, Frame*);
#endif

protected:
    // ScriptDebugListener. This is only used to create records for probe samples.
    virtual void didParseSource(JSC::SourceID, const Script&) override { }
    virtual void failedToParseSource(const String&, const String&, int, int, const String&) override { }
    virtual void didPause(JSC::ExecState*, const Deprecated::ScriptValue&, const Deprecated::ScriptValue&) override { }
    virtual void didContinue() override { }

    virtual void breakpointActionLog(JSC::ExecState*, const String&) override { }
    virtual void breakpointActionSound(int) override { }
    virtual void breakpointActionProbe(JSC::ExecState*, const Inspector::ScriptBreakpointAction&, int hitCount, const Deprecated::ScriptValue& result) override;

private:
    friend class TimelineRecordStack;

    struct TimelineRecordEntry {
        TimelineRecordEntry(PassRefPtr<Inspector::InspectorObject> record, PassRefPtr<Inspector::InspectorObject> data, PassRefPtr<Inspector::InspectorArray> children, TimelineRecordType type)
            : record(record), data(data), children(children), type(type)
        {
        }
        RefPtr<Inspector::InspectorObject> record;
        RefPtr<Inspector::InspectorObject> data;
        RefPtr<Inspector::InspectorArray> children;
        TimelineRecordType type;
    };

    void sendEvent(PassRefPtr<Inspector::InspectorObject>);
    void appendRecord(PassRefPtr<Inspector::InspectorObject> data, TimelineRecordType, bool captureCallStack, Frame*);
    void pushCurrentRecord(PassRefPtr<Inspector::InspectorObject>, TimelineRecordType, bool captureCallStack, Frame*);

    void setFrameIdentifier(Inspector::InspectorObject* record, Frame*);

    void didCompleteCurrentRecord(TimelineRecordType);

    void addRecordToTimeline(PassRefPtr<Inspector::InspectorObject>, TimelineRecordType);
    void clearRecordStack();

    void localToPageQuad(const RenderObject&, const LayoutRect&, FloatQuad*);
    const TimelineTimeConverter& timeConverter() const { return m_timeConverter; }
    double timestamp();
    Page* page();

    InspectorPageAgent* m_pageAgent;
    PageScriptDebugServer* m_scriptDebugServer;
    TimelineTimeConverter m_timeConverter;

    std::unique_ptr<Inspector::InspectorTimelineFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::InspectorTimelineBackendDispatcher> m_backendDispatcher;
    double m_timestampOffset;

    Vector<TimelineRecordEntry> m_recordStack;

    int m_id;
    int m_maxCallStackDepth;
    InspectorType m_inspectorType;
    InspectorClient* m_client;
    WeakPtrFactory<InspectorTimelineAgent> m_weakFactory;

    bool m_enabled;
    bool m_recordingProfile;
};

} // namespace WebCore

#endif // !ENABLE(INSPECTOR)
#endif // !defined(InspectorTimelineAgent_h)
