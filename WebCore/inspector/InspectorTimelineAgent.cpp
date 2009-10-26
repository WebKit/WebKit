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
#include "TimelineItemFactory.h"

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
    pushCurrentTimelineItem(TimelineItemFactory::createDOMDispatchTimelineItem(m_frontend, currentTimeInMilliseconds(), event), DOMDispatchTimelineItemType);
}

void InspectorTimelineAgent::didDispatchDOMEvent()
{
    didCompleteCurrentTimelineItem(DOMDispatchTimelineItemType);
}

void InspectorTimelineAgent::willLayout()
{
    pushCurrentTimelineItem(TimelineItemFactory::createGenericTimelineItem(m_frontend, currentTimeInMilliseconds()), LayoutTimelineItemType);
}

void InspectorTimelineAgent::didLayout()
{
    didCompleteCurrentTimelineItem(LayoutTimelineItemType);
}

void InspectorTimelineAgent::willRecalculateStyle()
{
    pushCurrentTimelineItem(TimelineItemFactory::createGenericTimelineItem(m_frontend, currentTimeInMilliseconds()), RecalculateStylesTimelineItemType);
}

void InspectorTimelineAgent::didRecalculateStyle()
{
    didCompleteCurrentTimelineItem(RecalculateStylesTimelineItemType);
}

void InspectorTimelineAgent::willPaint()
{
    pushCurrentTimelineItem(TimelineItemFactory::createGenericTimelineItem(m_frontend, currentTimeInMilliseconds()), PaintTimelineItemType);
}

void InspectorTimelineAgent::didPaint()
{
    didCompleteCurrentTimelineItem(PaintTimelineItemType);
}

void InspectorTimelineAgent::willWriteHTML()
{
    pushCurrentTimelineItem(TimelineItemFactory::createGenericTimelineItem(m_frontend, currentTimeInMilliseconds()), ParseHTMLTimelineItemType);
}

void InspectorTimelineAgent::didWriteHTML()
{
    didCompleteCurrentTimelineItem(ParseHTMLTimelineItemType);
}

void InspectorTimelineAgent::didInstallTimer(int timerId, int timeout, bool singleShot)
{
    addItemToTimeline(TimelineItemFactory::createTimerInstallTimelineItem(m_frontend, currentTimeInMilliseconds(), timerId,
        timeout, singleShot), TimerInstallTimelineItemType);
}

void InspectorTimelineAgent::didRemoveTimer(int timerId)
{
    addItemToTimeline(TimelineItemFactory::createGenericTimerTimelineItem(m_frontend, currentTimeInMilliseconds(), timerId),
        TimerRemoveTimelineItemType);
}

void InspectorTimelineAgent::willFireTimer(int timerId)
{
    pushCurrentTimelineItem(TimelineItemFactory::createGenericTimerTimelineItem(m_frontend, currentTimeInMilliseconds(), timerId),
        TimerFireTimelineItemType); 
}

void InspectorTimelineAgent::didFireTimer()
{
    didCompleteCurrentTimelineItem(TimerFireTimelineItemType);
}

void InspectorTimelineAgent::reset()
{
    m_itemStack.clear();
}

void InspectorTimelineAgent::addItemToTimeline(ScriptObject item, TimelineItemType type)
{
    item.set("type", type);
    if (m_itemStack.isEmpty())
        m_frontend->addItemToTimeline(item);
    else {
        TimelineItemEntry parent = m_itemStack.last();
        parent.children.set(parent.children.length(), item);
    }
}

void InspectorTimelineAgent::didCompleteCurrentTimelineItem(TimelineItemType type)
{
    ASSERT(!m_itemStack.isEmpty());
    TimelineItemEntry entry = m_itemStack.last();
    m_itemStack.removeLast();
    ASSERT(entry.type == type);
    entry.item.set("children", entry.children);
    entry.item.set("endTime", currentTimeInMilliseconds());
    addItemToTimeline(entry.item, type);
}

double InspectorTimelineAgent::currentTimeInMilliseconds()
{
    return currentTime() * 1000.0;
}

void InspectorTimelineAgent::pushCurrentTimelineItem(ScriptObject item, TimelineItemType type)
{
    m_itemStack.append(TimelineItemEntry(item, m_frontend->newScriptArray(), type));
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
