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

#include "config.h"
#include "InspectorTimelineAgent.h"

#if ENABLE(INSPECTOR)

#include "Event.h"
#include "InspectorFrontend.h"
#include "IntRect.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "TimelineRecordFactory.h"

#include <wtf/CurrentTime.h>

namespace WebCore {

int InspectorTimelineAgent::s_instanceCount = 0;

class InspectorTimelineAgent2 : ScriptGCEventListener, public Noncopyable {
public:
    InspectorTimelineAgent2(InspectorFrontend* frontend);
    virtual ~InspectorTimelineAgent2();

    void startNewRecord(PassRefPtr<InspectorObject>, TimelineRecordType);
    void completeCurrentRecord(TimelineRecordType);
    void addAtomicRecord(PassRefPtr<InspectorObject>, TimelineRecordType);
    void pushOutOfOrderRecord(PassRefPtr<InspectorObject>, TimelineRecordType);
    InspectorObject* getTopRecordData();
    virtual void didGC(double, double, size_t);

    void resetFrontendProxyObject(InspectorFrontend* frontend);
    void reset();

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
    Vector<TimelineRecordEntry> m_recordStack;

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
    InspectorFrontend* m_frontend;

    void pushGCEventRecords();
    void setHeapSizeStatistic(InspectorObject* record);
    void addRecordToTimeline(PassRefPtr<InspectorObject> prpRecord, TimelineRecordType type);
};

InspectorTimelineAgent2::InspectorTimelineAgent2(InspectorFrontend* frontend)
{
    m_frontend = frontend;
    ScriptGCEvent::addEventListener(this);
}

InspectorTimelineAgent2::~InspectorTimelineAgent2()
{
    ScriptGCEvent::removeEventListener(this);
}

void InspectorTimelineAgent2::didGC(double startTime, double endTime, size_t collectedBytesCount)
{
    m_gcEvents.append(GCEvent(startTime, endTime, collectedBytesCount));
}

void InspectorTimelineAgent2::addRecordToTimeline(PassRefPtr<InspectorObject> prpRecord, TimelineRecordType type)
{
    RefPtr<InspectorObject> record(prpRecord);
    record->setNumber("type", type);
    setHeapSizeStatistic(record.get());
    if (m_recordStack.isEmpty())
        m_frontend->addRecordToTimeline(record.release());
    else {
        TimelineRecordEntry parent = m_recordStack.last();
        parent.children->pushObject(record.release());
    }
}

void InspectorTimelineAgent2::setHeapSizeStatistic(InspectorObject* record)
{
    size_t usedHeapSize = 0;
    size_t totalHeapSize = 0;
    ScriptGCEvent::getHeapSize(usedHeapSize, totalHeapSize);
    record->setNumber("usedHeapSize", usedHeapSize);
    record->setNumber("totalHeapSize", totalHeapSize);
}

void InspectorTimelineAgent2::startNewRecord(PassRefPtr<InspectorObject> data, TimelineRecordType type)
{
    pushGCEventRecords();
    RefPtr<InspectorObject> record = TimelineRecordFactory::createGenericRecord(WTF::currentTimeMS());
    m_recordStack.append(TimelineRecordEntry(record.release(), data, InspectorArray::create(), type));
}

void InspectorTimelineAgent2::completeCurrentRecord(TimelineRecordType type)
{
    // An empty stack could merely mean that the timeline agent was turned on in the middle of
    // an event.  Don't treat as an error.
    if (!m_recordStack.isEmpty()) {
        pushGCEventRecords();
        TimelineRecordEntry entry = m_recordStack.last();
        m_recordStack.removeLast();
        ASSERT(entry.type == type);
        entry.record->setObject("data", entry.data);
        entry.record->setArray("children", entry.children);
        entry.record->setNumber("endTime", WTF::currentTimeMS());
        addRecordToTimeline(entry.record, type);
    }
}

void InspectorTimelineAgent2::pushOutOfOrderRecord(PassRefPtr<InspectorObject> data, TimelineRecordType recordType)
{
    pushGCEventRecords();
    RefPtr<InspectorObject> record = TimelineRecordFactory::createGenericRecord(WTF::currentTimeMS());
    record->setObject("data", data);
    record->setNumber("type", recordType);
    setHeapSizeStatistic(record.get());
    m_frontend->addRecordToTimeline(record.release());
}

void InspectorTimelineAgent2::addAtomicRecord(PassRefPtr<InspectorObject> data, TimelineRecordType recordType)
{
    pushGCEventRecords();
    RefPtr<InspectorObject> record = TimelineRecordFactory::createGenericRecord(WTF::currentTimeMS());
    record->setObject("data", data);
    addRecordToTimeline(record.release(), recordType);
}

InspectorObject* InspectorTimelineAgent2::getTopRecordData()
{
    if (m_recordStack.isEmpty())
        return 0;
    return m_recordStack.last().data.get();
}

void InspectorTimelineAgent2::reset()
{
    m_recordStack.clear();
}

void InspectorTimelineAgent2::resetFrontendProxyObject(InspectorFrontend* frontend)
{
    ASSERT(frontend);
    reset();
    m_frontend = frontend;
}

void InspectorTimelineAgent2::pushGCEventRecords()
{
    if (!m_gcEvents.size())
        return;

    GCEvents events = m_gcEvents;
    m_gcEvents.clear();
    for (GCEvents::iterator i = events.begin(); i != events.end(); ++i) {
        RefPtr<InspectorObject> record = TimelineRecordFactory::createGenericRecord(i->startTime);
        record->setObject("data", TimelineRecordFactory::createGCEventData(i->collectedBytes));
        record->setNumber("endTime", i->endTime);
        addRecordToTimeline(record.release(), GCEventTimelineRecordType);
    }
}

InspectorTimelineAgent::InspectorTimelineAgent(InspectorFrontend* frontend)
{
    ++s_instanceCount;
    m_timelineAgent = new InspectorTimelineAgent2(frontend);
}

InspectorTimelineAgent::~InspectorTimelineAgent()
{
    ASSERT(s_instanceCount);
    --s_instanceCount;
}

void InspectorTimelineAgent::reset()
{
    if (m_timelineAgent)
        m_timelineAgent->reset();
}

void InspectorTimelineAgent::willCallFunction(const String& scriptName, int scriptLine)
{
    if (m_timelineAgent)
        m_timelineAgent->startNewRecord(TimelineRecordFactory::createFunctionCallData(scriptName, scriptLine), FunctionCallTimelineRecordType);
}

void InspectorTimelineAgent::didCallFunction()
{
    if (m_timelineAgent)
        m_timelineAgent->completeCurrentRecord(FunctionCallTimelineRecordType);
}

void InspectorTimelineAgent::willDispatchEvent(const Event& event)
{
    if (m_timelineAgent)
        m_timelineAgent->startNewRecord(TimelineRecordFactory::createEventDispatchData(event), EventDispatchTimelineRecordType);
}

void InspectorTimelineAgent::didDispatchEvent()
{
    if (m_timelineAgent)
        m_timelineAgent->completeCurrentRecord(EventDispatchTimelineRecordType);
}

void InspectorTimelineAgent::willLayout()
{
    if (m_timelineAgent)
        m_timelineAgent->startNewRecord(InspectorObject::create(), LayoutTimelineRecordType);
}

void InspectorTimelineAgent::didLayout()
{
    if (m_timelineAgent)
        m_timelineAgent->completeCurrentRecord(LayoutTimelineRecordType);
}

void InspectorTimelineAgent::willRecalculateStyle()
{
    if (m_timelineAgent)
        m_timelineAgent->startNewRecord(InspectorObject::create(), RecalculateStylesTimelineRecordType);
}

void InspectorTimelineAgent::didRecalculateStyle()
{
    if (m_timelineAgent)
        m_timelineAgent->completeCurrentRecord(RecalculateStylesTimelineRecordType);
}

void InspectorTimelineAgent::willPaint(const IntRect& rect)
{
    if (m_timelineAgent)
        m_timelineAgent->startNewRecord(TimelineRecordFactory::createPaintData(rect), PaintTimelineRecordType);
}

void InspectorTimelineAgent::didPaint()
{
    if (m_timelineAgent)
        m_timelineAgent->completeCurrentRecord(PaintTimelineRecordType);
}

void InspectorTimelineAgent::willWriteHTML(unsigned int length, unsigned int startLine)
{
    if (m_timelineAgent)
        m_timelineAgent->startNewRecord(TimelineRecordFactory::createParseHTMLData(length, startLine), ParseHTMLTimelineRecordType);
}

void InspectorTimelineAgent::didWriteHTML(unsigned int endLine)
{
    if (m_timelineAgent) {
        InspectorObject* data = m_timelineAgent->getTopRecordData();
        if (data) {
            data->setNumber("endLine", endLine);
            m_timelineAgent->completeCurrentRecord(ParseHTMLTimelineRecordType);
        }
    }
}

void InspectorTimelineAgent::didInstallTimer(int timerId, int timeout, bool singleShot)
{
    if (m_timelineAgent)
        m_timelineAgent->addAtomicRecord(TimelineRecordFactory::createTimerInstallData(timerId, timeout, singleShot), TimerInstallTimelineRecordType);
}

void InspectorTimelineAgent::didRemoveTimer(int timerId)
{
    if (m_timelineAgent)
        m_timelineAgent->addAtomicRecord(TimelineRecordFactory::createGenericTimerData(timerId), TimerRemoveTimelineRecordType);
}

void InspectorTimelineAgent::willFireTimer(int timerId)
{
    if (m_timelineAgent)
        m_timelineAgent->startNewRecord(TimelineRecordFactory::createGenericTimerData(timerId), TimerFireTimelineRecordType);
}

void InspectorTimelineAgent::didFireTimer()
{
    if (m_timelineAgent)
        m_timelineAgent->completeCurrentRecord(TimerFireTimelineRecordType);
}

void InspectorTimelineAgent::willChangeXHRReadyState(const String& url, int readyState)
{
    if (m_timelineAgent)
        m_timelineAgent->startNewRecord(TimelineRecordFactory::createXHRReadyStateChangeData(url, readyState), XHRReadyStateChangeRecordType);
}

void InspectorTimelineAgent::didChangeXHRReadyState()
{
    if (m_timelineAgent)
        m_timelineAgent->completeCurrentRecord(XHRReadyStateChangeRecordType);
}

void InspectorTimelineAgent::willLoadXHR(const String& url) 
{
    if (m_timelineAgent)
        m_timelineAgent->startNewRecord(TimelineRecordFactory::createXHRLoadData(url), XHRLoadRecordType);
}

void InspectorTimelineAgent::didLoadXHR()
{
    if (m_timelineAgent)
        m_timelineAgent->completeCurrentRecord(XHRLoadRecordType);
}

void InspectorTimelineAgent::willEvaluateScript(const String& url, int lineNumber)
{
    if (m_timelineAgent)
        m_timelineAgent->startNewRecord(TimelineRecordFactory::createEvaluateScriptData(url, lineNumber), EvaluateScriptTimelineRecordType);
}
    
void InspectorTimelineAgent::didEvaluateScript()
{
    if (m_timelineAgent)
        m_timelineAgent->completeCurrentRecord(EvaluateScriptTimelineRecordType);
}

void InspectorTimelineAgent::didScheduleResourceRequest(const String& url)
{
    if (m_timelineAgent)
        m_timelineAgent->addAtomicRecord(TimelineRecordFactory::createScheduleResourceRequestData(url), ScheduleResourceRequestTimelineRecordType);
}

void InspectorTimelineAgent::willSendResourceRequest(unsigned long identifier, bool isMainResource, const ResourceRequest& request)
{
    if (m_timelineAgent)
        m_timelineAgent->pushOutOfOrderRecord(TimelineRecordFactory::createResourceSendRequestData(identifier, isMainResource, request), ResourceSendRequestTimelineRecordType);
}

void InspectorTimelineAgent::willReceiveResourceData(unsigned long identifier)
{
    if (m_timelineAgent)
        m_timelineAgent->startNewRecord(TimelineRecordFactory::createReceiveResourceData(identifier), ReceiveResourceDataTimelineRecordType);
}

void InspectorTimelineAgent::didReceiveResourceData()
{
    if (m_timelineAgent)
        m_timelineAgent->completeCurrentRecord(ReceiveResourceDataTimelineRecordType);
}
    
void InspectorTimelineAgent::willReceiveResourceResponse(unsigned long identifier, const ResourceResponse& response)
{
    if (m_timelineAgent)
        m_timelineAgent->startNewRecord(TimelineRecordFactory::createResourceReceiveResponseData(identifier, response), ResourceReceiveResponseTimelineRecordType);
}

void InspectorTimelineAgent::didReceiveResourceResponse()
{
    if (m_timelineAgent)
        m_timelineAgent->completeCurrentRecord(ResourceReceiveResponseTimelineRecordType);
}

void InspectorTimelineAgent::didFinishLoadingResource(unsigned long identifier, bool didFail)
{
    if (m_timelineAgent)
        m_timelineAgent->pushOutOfOrderRecord(TimelineRecordFactory::createResourceFinishData(identifier, didFail), ResourceFinishTimelineRecordType);
}

void InspectorTimelineAgent::didMarkTimeline(const String& message)
{
    if (m_timelineAgent)
        m_timelineAgent->addAtomicRecord(TimelineRecordFactory::createMarkTimelineData(message), MarkTimelineRecordType);
}

void InspectorTimelineAgent::didMarkDOMContentEvent()
{
    if (m_timelineAgent)
        m_timelineAgent->addAtomicRecord(InspectorObject::create(), MarkDOMContentEventType);
}

void InspectorTimelineAgent::didMarkLoadEvent()
{
    if (m_timelineAgent)
        m_timelineAgent->addAtomicRecord(InspectorObject::create(), MarkLoadEventType);
}

void InspectorTimelineAgent::resetFrontendProxyObject(InspectorFrontend* frontend)
{
    if (m_timelineAgent)
        m_timelineAgent->resetFrontendProxyObject(frontend);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
