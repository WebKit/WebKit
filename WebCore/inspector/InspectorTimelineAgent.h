/*
* Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "InspectorValues.h"
#include "ScriptGCEvent.h"
#include "ScriptGCEventListener.h"
#include <wtf/Vector.h>

namespace WebCore {
class Event;
class InspectorFrontend;
class IntRect;
class ResourceRequest;
class ResourceResponse;

// Must be kept in sync with TimelineAgent.js
enum TimelineRecordType {
    EventDispatchTimelineRecordType = 0,
    LayoutTimelineRecordType = 1,
    RecalculateStylesTimelineRecordType = 2,
    PaintTimelineRecordType = 3,
    ParseHTMLTimelineRecordType = 4,
    TimerInstallTimelineRecordType = 5,
    TimerRemoveTimelineRecordType = 6,
    TimerFireTimelineRecordType = 7,
    XHRReadyStateChangeRecordType = 8,
    XHRLoadRecordType = 9,
    EvaluateScriptTimelineRecordType = 10,
    MarkTimelineRecordType = 11,
    ResourceSendRequestTimelineRecordType = 12,
    ResourceReceiveResponseTimelineRecordType = 13,
    ResourceFinishTimelineRecordType = 14,
    FunctionCallTimelineRecordType = 15,
    ReceiveResourceDataTimelineRecordType = 16,
    GCEventTimelineRecordType = 17,
    MarkDOMContentEventType = 18,
    MarkLoadEventType = 19,
    ScheduleResourceRequestTimelineRecordType = 20
};

class InspectorTimelineAgent : ScriptGCEventListener, public Noncopyable {
public:
    InspectorTimelineAgent(InspectorFrontend* frontend);
    ~InspectorTimelineAgent();

    int id() const { return m_id; }

    void reset();
    void resetFrontendProxyObject(InspectorFrontend*);

    // Methods called from WebCore.
    void willCallFunction(const String& scriptName, int scriptLine);
    void didCallFunction();

    void willDispatchEvent(const Event&);
    void didDispatchEvent();

    void willLayout();
    void didLayout();

    void willRecalculateStyle();
    void didRecalculateStyle();

    void willPaint(const IntRect&);
    void didPaint();

    // FIXME: |length| should be passed in didWrite instead willWrite
    // as the parser can not know how much it will process until it tries.
    void willWriteHTML(unsigned int length, unsigned int startLine);
    void didWriteHTML(unsigned int endLine);

    void didInstallTimer(int timerId, int timeout, bool singleShot);
    void didRemoveTimer(int timerId);
    void willFireTimer(int timerId);
    void didFireTimer();

    void willChangeXHRReadyState(const String&, int);
    void didChangeXHRReadyState();
    void willLoadXHR(const String&);
    void didLoadXHR();

    void willEvaluateScript(const String&, int);
    void didEvaluateScript();

    void didMarkTimeline(const String&);
    void didMarkDOMContentEvent();
    void didMarkLoadEvent();

    void didScheduleResourceRequest(const String& url);
    void willSendResourceRequest(unsigned long, bool isMainResource, const ResourceRequest&);
    void willReceiveResourceResponse(unsigned long, const ResourceResponse&);
    void didReceiveResourceResponse();
    void didFinishLoadingResource(unsigned long, bool didFail, double finishTime);
    void willReceiveResourceData(unsigned long identifier);
    void didReceiveResourceData();
        
    virtual void didGC(double, double, size_t);

private:
    struct TimelineRecordEntry {
        TimelineRecordEntry(PassRefPtr<InspectorObject> record, PassRefPtr<InspectorObject> data, PassRefPtr<InspectorArray> children, TimelineRecordType type)
            : record(record), data(data), children(children), type(type)
        {
        }
        RefPtr<InspectorObject> record;
        RefPtr<InspectorObject> data;
        RefPtr<InspectorArray> children;
        TimelineRecordType type;
    };
        
    void pushCurrentRecord(PassRefPtr<InspectorObject>, TimelineRecordType);
    void setHeapSizeStatistic(InspectorObject* record);
        
    void didCompleteCurrentRecord(TimelineRecordType);

    void addRecordToTimeline(PassRefPtr<InspectorObject>, TimelineRecordType);

    void pushGCEventRecords();

    InspectorFrontend* m_frontend;

    Vector<TimelineRecordEntry> m_recordStack;

    static int s_id;
    const int m_id;
    struct GCEvent {
        GCEvent(double startTime, double endTime, size_t collectedBytes)
            : startTime(startTime), endTime(endTime), collectedBytes(collectedBytes)
        {
        }
        double startTime;
        double endTime;
        size_t collectedBytes;
    };
    typedef Vector<GCEvent> GCEvents;
    GCEvents m_gcEvents;
};

} // namespace WebCore

#endif // !ENABLE(INSPECTOR)
#endif // !defined(InspectorTimelineAgent_h)
