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
int InspectorTimelineAgent::s_id = 0;

InspectorTimelineAgent::InspectorTimelineAgent(InspectorFrontend* frontend)
    : m_frontend(frontend)
    , m_id(++s_id)
{
    ++s_instanceCount;
    ScriptGCEvent::addEventListener(this);
    ASSERT(m_frontend);
}

void InspectorTimelineAgent::pushGCEventRecords()
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

void InspectorTimelineAgent::didGC(double startTime, double endTime, size_t collectedBytesCount)
{
    m_gcEvents.append(GCEvent(startTime, endTime, collectedBytesCount));
}

InspectorTimelineAgent::~InspectorTimelineAgent()
{
    ASSERT(s_instanceCount);
    --s_instanceCount;
    ScriptGCEvent::removeEventListener(this);
}

void InspectorTimelineAgent::willCallFunction(const String& scriptName, int scriptLine)
{
    pushCurrentRecord(TimelineRecordFactory::createFunctionCallData(scriptName, scriptLine), FunctionCallTimelineRecordType);
}

void InspectorTimelineAgent::didCallFunction()
{
    didCompleteCurrentRecord(FunctionCallTimelineRecordType);
}

void InspectorTimelineAgent::willDispatchEvent(const Event& event)
{
    pushCurrentRecord(TimelineRecordFactory::createEventDispatchData(event),
        EventDispatchTimelineRecordType);
}

void InspectorTimelineAgent::didDispatchEvent()
{
    didCompleteCurrentRecord(EventDispatchTimelineRecordType);
}

void InspectorTimelineAgent::willLayout()
{
    pushCurrentRecord(InspectorObject::create(), LayoutTimelineRecordType);
}

void InspectorTimelineAgent::didLayout()
{
    didCompleteCurrentRecord(LayoutTimelineRecordType);
}

void InspectorTimelineAgent::willRecalculateStyle()
{
    pushCurrentRecord(InspectorObject::create(), RecalculateStylesTimelineRecordType);
}

void InspectorTimelineAgent::didRecalculateStyle()
{
    didCompleteCurrentRecord(RecalculateStylesTimelineRecordType);
}

void InspectorTimelineAgent::willPaint(const IntRect& rect)
{
    pushCurrentRecord(TimelineRecordFactory::createPaintData(rect), PaintTimelineRecordType);
}

void InspectorTimelineAgent::didPaint()
{
    didCompleteCurrentRecord(PaintTimelineRecordType);
}

void InspectorTimelineAgent::willWriteHTML(unsigned int length, unsigned int startLine)
{
    pushCurrentRecord(TimelineRecordFactory::createParseHTMLData(length, startLine), ParseHTMLTimelineRecordType);
}

void InspectorTimelineAgent::didWriteHTML(unsigned int endLine)
{
    if (!m_recordStack.isEmpty()) {
        TimelineRecordEntry entry = m_recordStack.last();
        entry.data->setNumber("endLine", endLine);
        didCompleteCurrentRecord(ParseHTMLTimelineRecordType);
    }
}

void InspectorTimelineAgent::didInstallTimer(int timerId, int timeout, bool singleShot)
{
    pushGCEventRecords();
    RefPtr<InspectorObject> record = TimelineRecordFactory::createGenericRecord(WTF::currentTimeMS());
    record->setObject("data", TimelineRecordFactory::createTimerInstallData(timerId, timeout, singleShot));
    addRecordToTimeline(record.release(), TimerInstallTimelineRecordType);
}

void InspectorTimelineAgent::didRemoveTimer(int timerId)
{
    pushGCEventRecords();
    RefPtr<InspectorObject> record = TimelineRecordFactory::createGenericRecord(WTF::currentTimeMS());
    record->setObject("data", TimelineRecordFactory::createGenericTimerData(timerId));
    addRecordToTimeline(record.release(), TimerRemoveTimelineRecordType);
}

void InspectorTimelineAgent::willFireTimer(int timerId)
{
    pushCurrentRecord(TimelineRecordFactory::createGenericTimerData(timerId), TimerFireTimelineRecordType);
}

void InspectorTimelineAgent::didFireTimer()
{
    didCompleteCurrentRecord(TimerFireTimelineRecordType);
}

void InspectorTimelineAgent::willChangeXHRReadyState(const String& url, int readyState)
{
    pushCurrentRecord(TimelineRecordFactory::createXHRReadyStateChangeData(url, readyState), XHRReadyStateChangeRecordType);
}

void InspectorTimelineAgent::didChangeXHRReadyState()
{
    didCompleteCurrentRecord(XHRReadyStateChangeRecordType);
}

void InspectorTimelineAgent::willLoadXHR(const String& url) 
{
    pushCurrentRecord(TimelineRecordFactory::createXHRLoadData(url), XHRLoadRecordType);
}

void InspectorTimelineAgent::didLoadXHR()
{
    didCompleteCurrentRecord(XHRLoadRecordType);
}

void InspectorTimelineAgent::willEvaluateScript(const String& url, int lineNumber)
{
    pushCurrentRecord(TimelineRecordFactory::createEvaluateScriptData(url, lineNumber), EvaluateScriptTimelineRecordType);
}
    
void InspectorTimelineAgent::didEvaluateScript()
{
    didCompleteCurrentRecord(EvaluateScriptTimelineRecordType);
}

void InspectorTimelineAgent::didScheduleResourceRequest(const String& url)
{
    pushGCEventRecords();
    RefPtr<InspectorObject> record = TimelineRecordFactory::createGenericRecord(WTF::currentTimeMS());
    record->setObject("data", TimelineRecordFactory::createScheduleResourceRequestData(url));
    record->setNumber("type", ScheduleResourceRequestTimelineRecordType);
    addRecordToTimeline(record.release(), ScheduleResourceRequestTimelineRecordType);
}

void InspectorTimelineAgent::willSendResourceRequest(unsigned long identifier, bool isMainResource,
    const ResourceRequest& request)
{
    pushGCEventRecords();
    RefPtr<InspectorObject> record = TimelineRecordFactory::createGenericRecord(WTF::currentTimeMS());
    record->setObject("data", TimelineRecordFactory::createResourceSendRequestData(identifier, isMainResource, request));
    record->setNumber("type", ResourceSendRequestTimelineRecordType);
    setHeapSizeStatistic(record.get());
    m_frontend->addRecordToTimeline(record.release());
}

void InspectorTimelineAgent::willReceiveResourceData(unsigned long identifier)
{
    pushCurrentRecord(TimelineRecordFactory::createReceiveResourceData(identifier), ReceiveResourceDataTimelineRecordType);
}

void InspectorTimelineAgent::didReceiveResourceData()
{
    didCompleteCurrentRecord(ReceiveResourceDataTimelineRecordType);
}
    
void InspectorTimelineAgent::willReceiveResourceResponse(unsigned long identifier, const ResourceResponse& response)
{
    pushCurrentRecord(TimelineRecordFactory::createResourceReceiveResponseData(identifier, response), ResourceReceiveResponseTimelineRecordType);
}

void InspectorTimelineAgent::didReceiveResourceResponse()
{
    didCompleteCurrentRecord(ResourceReceiveResponseTimelineRecordType);
}

void InspectorTimelineAgent::didFinishLoadingResource(unsigned long identifier, bool didFail, double finishTime)
{
    pushGCEventRecords();
    // Sometimes network stack can provide for us exact finish loading time. In the other case we will use currentTime.
    RefPtr<InspectorObject> record = TimelineRecordFactory::createGenericRecord(finishTime ? finishTime * 1000 : WTF::currentTimeMS());
    record->setObject("data", TimelineRecordFactory::createResourceFinishData(identifier, didFail));
    record->setNumber("type", ResourceFinishTimelineRecordType);
    setHeapSizeStatistic(record.get());
    m_frontend->addRecordToTimeline(record.release());
}

void InspectorTimelineAgent::didMarkTimeline(const String& message)
{
    pushGCEventRecords();
    RefPtr<InspectorObject> record = TimelineRecordFactory::createGenericRecord(WTF::currentTimeMS());
    record->setObject("data", TimelineRecordFactory::createMarkTimelineData(message));
    addRecordToTimeline(record.release(), MarkTimelineRecordType);
}

void InspectorTimelineAgent::didMarkDOMContentEvent()
{
    pushGCEventRecords();
    RefPtr<InspectorObject> record = TimelineRecordFactory::createGenericRecord(WTF::currentTimeMS());
    addRecordToTimeline(record.release(), MarkDOMContentEventType);
}

void InspectorTimelineAgent::didMarkLoadEvent()
{
    pushGCEventRecords();
    RefPtr<InspectorObject> record = TimelineRecordFactory::createGenericRecord(WTF::currentTimeMS());
    addRecordToTimeline(record.release(), MarkLoadEventType);
}

void InspectorTimelineAgent::reset()
{
    m_recordStack.clear();
}

void InspectorTimelineAgent::resetFrontendProxyObject(InspectorFrontend* frontend)
{
    ASSERT(frontend);
    reset();
    m_frontend = frontend;
}

void InspectorTimelineAgent::addRecordToTimeline(PassRefPtr<InspectorObject> prpRecord, TimelineRecordType type)
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

void InspectorTimelineAgent::setHeapSizeStatistic(InspectorObject* record)
{
    size_t usedHeapSize = 0;
    size_t totalHeapSize = 0;
    ScriptGCEvent::getHeapSize(usedHeapSize, totalHeapSize);
    record->setNumber("usedHeapSize", usedHeapSize);
    record->setNumber("totalHeapSize", totalHeapSize);
}

void InspectorTimelineAgent::didCompleteCurrentRecord(TimelineRecordType type)
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

void InspectorTimelineAgent::pushCurrentRecord(PassRefPtr<InspectorObject> data, TimelineRecordType type)
{
    pushGCEventRecords();
    RefPtr<InspectorObject> record = TimelineRecordFactory::createGenericRecord(WTF::currentTimeMS());
    m_recordStack.append(TimelineRecordEntry(record.release(), data, InspectorArray::create(), type));
}
} // namespace WebCore

#endif // ENABLE(INSPECTOR)
