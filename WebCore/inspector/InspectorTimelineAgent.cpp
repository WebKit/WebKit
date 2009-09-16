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

#include "DOMDispatchTimelineItem.h"
#include "Event.h"
#include "InspectorFrontend.h"
#include "TimelineItem.h"

#include <wtf/CurrentTime.h>

namespace WebCore {

InspectorTimelineAgent::InspectorTimelineAgent(InspectorFrontend* frontend)
    : m_sessionStartTime(currentTimeInMilliseconds())
    , m_frontend(frontend)
    , m_currentTimelineItem(0)
{
    ASSERT(m_frontend);
}

InspectorTimelineAgent::~InspectorTimelineAgent()
{
}

void InspectorTimelineAgent::willDispatchDOMEvent(const Event& event)
{
    m_currentTimelineItem = new DOMDispatchTimelineItem(m_currentTimelineItem.release(), sessionTimeInMilliseconds(), event);
}

void InspectorTimelineAgent::didDispatchDOMEvent()
{
    ASSERT(m_currentTimelineItem->type() == DOMDispatchTimelineItemType);
    didCompleteCurrentRecord();
}

void InspectorTimelineAgent::willLayout()
{
    m_currentTimelineItem = new TimelineItem(m_currentTimelineItem.release(), sessionTimeInMilliseconds(), LayoutTimelineItemType);
}

void InspectorTimelineAgent::didLayout()
{
    ASSERT(m_currentTimelineItem->type() == LayoutTimelineItemType);
    didCompleteCurrentRecord();
}

void InspectorTimelineAgent::willRecalculateStyle()
{
    m_currentTimelineItem = new TimelineItem(m_currentTimelineItem.release(), sessionTimeInMilliseconds(), RecalculateStylesTimelineItemType);
}

void InspectorTimelineAgent::didRecalculateStyle()
{
    ASSERT(m_currentTimelineItem->type() == RecalculateStylesTimelineItemType);
    didCompleteCurrentRecord();
}

void InspectorTimelineAgent::willPaint()
{
    m_currentTimelineItem = new TimelineItem(m_currentTimelineItem.release(), sessionTimeInMilliseconds(), PaintTimelineItemType);
}

void InspectorTimelineAgent::didPaint()
{
    ASSERT(m_currentTimelineItem->type() == PaintTimelineItemType);
    didCompleteCurrentRecord();
}

void InspectorTimelineAgent::willWriteHTML()
{
    m_currentTimelineItem = new TimelineItem(m_currentTimelineItem.release(), sessionTimeInMilliseconds(), ParseHTMLTimelineItemType);
}

void InspectorTimelineAgent::didWriteHTML()
{
    ASSERT(m_currentTimelineItem->type() == ParseHTMLTimelineItemType);
    didCompleteCurrentRecord();
}

void InspectorTimelineAgent::reset()
{
    m_sessionStartTime = currentTimeInMilliseconds();
    m_currentTimelineItem.set(0);
}

void InspectorTimelineAgent::didCompleteCurrentRecord()
{
    OwnPtr<TimelineItem> item(m_currentTimelineItem.release());
    m_currentTimelineItem = item->releasePrevious();

    item->setEndTime(sessionTimeInMilliseconds());
    if (m_currentTimelineItem.get())
        m_currentTimelineItem->addChildItem(item.release());
    else
        item->addToTimeline(m_frontend);
}

double InspectorTimelineAgent::currentTimeInMilliseconds()
{
    return currentTime() * 1000.0;
}

double InspectorTimelineAgent::sessionTimeInMilliseconds()
{
    return currentTimeInMilliseconds() - m_sessionStartTime;
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
