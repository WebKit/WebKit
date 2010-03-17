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

InspectorTimelineAgent::InspectorTimelineAgent(InspectorFrontend* frontend)
    : m_frontend(frontend)
{
    ++s_instanceCount;
    ASSERT(m_frontend);
}

InspectorTimelineAgent::~InspectorTimelineAgent()
{
    ASSERT(s_instanceCount);
    --s_instanceCount;
}

void InspectorTimelineAgent::willCallFunction(const String& scriptName, int scriptLine)
{
    pushCurrentRecord(TimelineRecordFactory::createFunctionCallData(m_frontend, scriptName, scriptLine), FunctionCallTimelineRecordType);
}

void InspectorTimelineAgent::didCallFunction()
{
    didCompleteCurrentRecord(FunctionCallTimelineRecordType);
}

void InspectorTimelineAgent::willDispatchEvent(const Event& event)
{
    pushCurrentRecord(TimelineRecordFactory::createEventDispatchData(m_frontend, event),
        EventDispatchTimelineRecordType);
}

void InspectorTimelineAgent::didDispatchEvent()
{
    didCompleteCurrentRecord(EventDispatchTimelineRecordType);
}

void InspectorTimelineAgent::willLayout()
{
    pushCurrentRecord(m_frontend->newScriptObject(), LayoutTimelineRecordType);
}

void InspectorTimelineAgent::didLayout()
{
    didCompleteCurrentRecord(LayoutTimelineRecordType);
}

void InspectorTimelineAgent::willRecalculateStyle()
{
    pushCurrentRecord(m_frontend->newScriptObject(), RecalculateStylesTimelineRecordType);
}

void InspectorTimelineAgent::didRecalculateStyle()
{
    didCompleteCurrentRecord(RecalculateStylesTimelineRecordType);
}

void InspectorTimelineAgent::willPaint(const IntRect& rect)
{
    pushCurrentRecord(TimelineRecordFactory::createPaintData(m_frontend, rect), PaintTimelineRecordType);
}

void InspectorTimelineAgent::didPaint()
{
    didCompleteCurrentRecord(PaintTimelineRecordType);
}

void InspectorTimelineAgent::willWriteHTML(unsigned int length, unsigned int startLine)
{
    pushCurrentRecord(TimelineRecordFactory::createParseHTMLData(m_frontend, length, startLine), ParseHTMLTimelineRecordType);
}

void InspectorTimelineAgent::didWriteHTML(unsigned int endLine)
{
    if (!m_recordStack.isEmpty()) {
        TimelineRecordEntry entry = m_recordStack.last();
        entry.data.set("endLine", endLine);
        didCompleteCurrentRecord(ParseHTMLTimelineRecordType);
    }
}

void InspectorTimelineAgent::didInstallTimer(int timerId, int timeout, bool singleShot)
{
    ScriptObject record = TimelineRecordFactory::createGenericRecord(m_frontend, currentTimeInMilliseconds());
    record.set("data", TimelineRecordFactory::createTimerInstallData(m_frontend, timerId, timeout, singleShot));
    addRecordToTimeline(record, TimerInstallTimelineRecordType);
}

void InspectorTimelineAgent::didRemoveTimer(int timerId)
{
    ScriptObject record = TimelineRecordFactory::createGenericRecord(m_frontend, currentTimeInMilliseconds());
    record.set("data", TimelineRecordFactory::createGenericTimerData(m_frontend, timerId));
    addRecordToTimeline(record, TimerRemoveTimelineRecordType);
}

void InspectorTimelineAgent::willFireTimer(int timerId)
{
    pushCurrentRecord(TimelineRecordFactory::createGenericTimerData(m_frontend, timerId), TimerFireTimelineRecordType); 
}

void InspectorTimelineAgent::didFireTimer()
{
    didCompleteCurrentRecord(TimerFireTimelineRecordType);
}

void InspectorTimelineAgent::willChangeXHRReadyState(const String& url, int readyState)
{
    pushCurrentRecord(TimelineRecordFactory::createXHRReadyStateChangeData(m_frontend, url, readyState), XHRReadyStateChangeRecordType);
}

void InspectorTimelineAgent::didChangeXHRReadyState()
{
    didCompleteCurrentRecord(XHRReadyStateChangeRecordType);
}

void InspectorTimelineAgent::willLoadXHR(const String& url) 
{
    pushCurrentRecord(TimelineRecordFactory::createXHRLoadData(m_frontend, url), XHRLoadRecordType);
}

void InspectorTimelineAgent::didLoadXHR()
{
    didCompleteCurrentRecord(XHRLoadRecordType);
}

void InspectorTimelineAgent::willEvaluateScript(const String& url, int lineNumber)
{
    pushCurrentRecord(TimelineRecordFactory::createEvaluateScriptData(m_frontend, url, lineNumber), EvaluateScriptTimelineRecordType);
}
    
void InspectorTimelineAgent::didEvaluateScript()
{
    didCompleteCurrentRecord(EvaluateScriptTimelineRecordType);
}

void InspectorTimelineAgent::willSendResourceRequest(unsigned long identifier, bool isMainResource,
    const ResourceRequest& request)
{
    ScriptObject record = TimelineRecordFactory::createGenericRecord(m_frontend, currentTimeInMilliseconds());
    record.set("data", TimelineRecordFactory::createResourceSendRequestData(m_frontend, identifier, isMainResource, request));
    record.set("type", ResourceSendRequestTimelineRecordType);
    m_frontend->addRecordToTimeline(record);
}

void InspectorTimelineAgent::willReceiveResourceData(unsigned long identifier)
{
    pushCurrentRecord(TimelineRecordFactory::createReceiveResourceData(m_frontend, identifier), ReceiveResourceDataTimelineRecordType);
}

void InspectorTimelineAgent::didReceiveResourceData()
{
    didCompleteCurrentRecord(ReceiveResourceDataTimelineRecordType);
}
    
void InspectorTimelineAgent::willReceiveResourceResponse(unsigned long identifier, const ResourceResponse& response)
{
    pushCurrentRecord(TimelineRecordFactory::createResourceReceiveResponseData(m_frontend, identifier, response), ResourceReceiveResponseTimelineRecordType);
}

void InspectorTimelineAgent::didReceiveResourceResponse()
{
    didCompleteCurrentRecord(ResourceReceiveResponseTimelineRecordType);
}

void InspectorTimelineAgent::didFinishLoadingResource(unsigned long identifier, bool didFail)
{
    ScriptObject record = TimelineRecordFactory::createGenericRecord(m_frontend, currentTimeInMilliseconds());
    record.set("data", TimelineRecordFactory::createResourceFinishData(m_frontend, identifier, didFail));
    record.set("type", ResourceFinishTimelineRecordType);
    m_frontend->addRecordToTimeline(record);
}

void InspectorTimelineAgent::didMarkTimeline(const String& message)
{
    ScriptObject record = TimelineRecordFactory::createGenericRecord(m_frontend, currentTimeInMilliseconds());
    record.set("data", TimelineRecordFactory::createMarkTimelineData(m_frontend, message));
    addRecordToTimeline(record, MarkTimelineRecordType);
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

void InspectorTimelineAgent::addRecordToTimeline(ScriptObject record, TimelineRecordType type)
{
    record.set("type", type);
    if (m_recordStack.isEmpty())
        m_frontend->addRecordToTimeline(record);
    else {
        TimelineRecordEntry parent = m_recordStack.last();
        parent.children.set(parent.children.length(), record);
    }
}

void InspectorTimelineAgent::didCompleteCurrentRecord(TimelineRecordType type)
{
    // An empty stack could merely mean that the timeline agent was turned on in the middle of
    // an event.  Don't treat as an error.
    if (!m_recordStack.isEmpty()) {
        TimelineRecordEntry entry = m_recordStack.last();
        m_recordStack.removeLast();
        ASSERT(entry.type == type);
        entry.record.set("data", entry.data);
        entry.record.set("children", entry.children);
        entry.record.set("endTime", currentTimeInMilliseconds());
        addRecordToTimeline(entry.record, type);
    }
}

double InspectorTimelineAgent::currentTimeInMilliseconds()
{
    return currentTime() * 1000.0;
}

void InspectorTimelineAgent::pushCurrentRecord(ScriptObject data, TimelineRecordType type)
{
    ScriptObject record = TimelineRecordFactory::createGenericRecord(m_frontend, currentTimeInMilliseconds());
    m_recordStack.append(TimelineRecordEntry(record, data, m_frontend->newScriptArray(), type));
}
} // namespace WebCore

#endif // ENABLE(INSPECTOR)
