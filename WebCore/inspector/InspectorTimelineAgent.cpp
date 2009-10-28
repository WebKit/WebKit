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
#include "TimelineRecordFactory.h"

#include <wtf/CurrentTime.h>

namespace WebCore {

InspectorTimelineAgent::InspectorTimelineAgent(InspectorFrontend* frontend)
    : m_frontend(frontend)
{
    ASSERT(m_frontend);
}

InspectorTimelineAgent::~InspectorTimelineAgent()
{
}

void InspectorTimelineAgent::willDispatchDOMEvent(const Event& event)
{
    pushCurrentRecord(TimelineRecordFactory::createDOMDispatchRecord(m_frontend, currentTimeInMilliseconds(), event), DOMDispatchTimelineRecordType);
}

void InspectorTimelineAgent::didDispatchDOMEvent()
{
    didCompleteCurrentRecord(DOMDispatchTimelineRecordType);
}

void InspectorTimelineAgent::willLayout()
{
    pushCurrentRecord(TimelineRecordFactory::createGenericRecord(m_frontend, currentTimeInMilliseconds()), LayoutTimelineRecordType);
}

void InspectorTimelineAgent::didLayout()
{
    didCompleteCurrentRecord(LayoutTimelineRecordType);
}

void InspectorTimelineAgent::willRecalculateStyle()
{
    pushCurrentRecord(TimelineRecordFactory::createGenericRecord(m_frontend, currentTimeInMilliseconds()), RecalculateStylesTimelineRecordType);
}

void InspectorTimelineAgent::didRecalculateStyle()
{
    didCompleteCurrentRecord(RecalculateStylesTimelineRecordType);
}

void InspectorTimelineAgent::willPaint()
{
    pushCurrentRecord(TimelineRecordFactory::createGenericRecord(m_frontend, currentTimeInMilliseconds()), PaintTimelineRecordType);
}

void InspectorTimelineAgent::didPaint()
{
    didCompleteCurrentRecord(PaintTimelineRecordType);
}

void InspectorTimelineAgent::willWriteHTML()
{
    pushCurrentRecord(TimelineRecordFactory::createGenericRecord(m_frontend, currentTimeInMilliseconds()), ParseHTMLTimelineRecordType);
}

void InspectorTimelineAgent::didWriteHTML()
{
    didCompleteCurrentRecord(ParseHTMLTimelineRecordType);
}
   
void InspectorTimelineAgent::didInstallTimer(int timerId, int timeout, bool singleShot)
{
    addRecordToTimeline(TimelineRecordFactory::createTimerInstallRecord(m_frontend, currentTimeInMilliseconds(), timerId,
        timeout, singleShot), TimerInstallTimelineRecordType);
}

void InspectorTimelineAgent::didRemoveTimer(int timerId)
{
    addRecordToTimeline(TimelineRecordFactory::createGenericTimerRecord(m_frontend, currentTimeInMilliseconds(), timerId),
        TimerRemoveTimelineRecordType);
}

void InspectorTimelineAgent::willFireTimer(int timerId)
{
    pushCurrentRecord(TimelineRecordFactory::createGenericTimerRecord(m_frontend, currentTimeInMilliseconds(), timerId),
        TimerFireTimelineRecordType); 
}

void InspectorTimelineAgent::didFireTimer()
{
    didCompleteCurrentRecord(TimerFireTimelineRecordType);
}

void InspectorTimelineAgent::willChangeXHRReadyState(const String& url, int readyState)
{
    pushCurrentRecord(TimelineRecordFactory::createXHRReadyStateChangeTimelineRecord(m_frontend, currentTimeInMilliseconds(), url, readyState),
        XHRReadyStateChangeRecordType);
}

void InspectorTimelineAgent::didChangeXHRReadyState()
{
    didCompleteCurrentRecord(XHRReadyStateChangeRecordType);
}

void InspectorTimelineAgent::willLoadXHR(const String& url) 
{
    pushCurrentRecord(TimelineRecordFactory::createXHRLoadTimelineRecord(m_frontend, currentTimeInMilliseconds(), url), XHRLoadRecordType);
}

void InspectorTimelineAgent::didLoadXHR()
{
    didCompleteCurrentRecord(XHRLoadRecordType);
}

void InspectorTimelineAgent::willEvaluateScriptTag(const String& url, int lineNumber)
{
    pushCurrentRecord(TimelineRecordFactory::createEvaluateScriptTagTimelineRecord(m_frontend, currentTimeInMilliseconds(), url, lineNumber), EvaluateScriptTagTimelineRecordType);
}
    
void InspectorTimelineAgent::didEvaluateScriptTag()
{
    didCompleteCurrentRecord(EvaluateScriptTagTimelineRecordType);
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
    ASSERT(!m_recordStack.isEmpty());
    TimelineRecordEntry entry = m_recordStack.last();
    m_recordStack.removeLast();
    ASSERT(entry.type == type);
    entry.record.set("children", entry.children);
    entry.record.set("endTime", currentTimeInMilliseconds());
    addRecordToTimeline(entry.record, type);
}

double InspectorTimelineAgent::currentTimeInMilliseconds()
{
    return currentTime() * 1000.0;
}

void InspectorTimelineAgent::pushCurrentRecord(ScriptObject record, TimelineRecordType type)
{
    m_recordStack.append(TimelineRecordEntry(record, m_frontend->newScriptArray(), type));
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
