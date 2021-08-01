/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderingUpdateScheduler.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "DisplayRefreshMonitorManager.h"
#include "Logging.h"
#include "Page.h"
#include <wtf/SystemTracing.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

RenderingUpdateScheduler::RenderingUpdateScheduler(Page& page)
    : m_page(page)
{
    windowScreenDidChange(page.chrome().displayID());
}

bool RenderingUpdateScheduler::scheduleAnimation()
{
    if (m_useTimer)
        return false;

    return DisplayRefreshMonitorManager::sharedManager().scheduleAnimation(*this);
}

void RenderingUpdateScheduler::adjustRenderingUpdateFrequency()
{
    auto renderingUpdateFramesPerSecond = m_page.preferredRenderingUpdateFramesPerSecond();
    if (renderingUpdateFramesPerSecond) {
        setPreferredFramesPerSecond(renderingUpdateFramesPerSecond.value());
        m_useTimer = false;
    } else
        m_useTimer = true;

    if (isScheduled()) {
        clearScheduled();
        scheduleRenderingUpdate();
    }
}

void RenderingUpdateScheduler::scheduleRenderingUpdate()
{
    LOG_WITH_STREAM(EventLoop, stream << "RenderingUpdateScheduler for page " << &m_page << " scheduleTimedRenderingUpdate() - already scheduled " << isScheduled() << " page visible " << m_page.isVisible());

    if (isScheduled())
        return;

    // Optimize the case when an invisible page wants just to schedule layer flush.
    if (!m_page.isVisible()) {
        triggerRenderingUpdate();
        return;
    }

    tracePoint(ScheduleRenderingUpdate);

    if (!scheduleAnimation()) {
        LOG_WITH_STREAM(DisplayLink, stream << "RenderingUpdateScheduler::scheduleRenderingUpdate for interval " << m_page.preferredRenderingUpdateInterval() << " falling back to timer");
        startTimer(m_page.preferredRenderingUpdateInterval());
    }
}

bool RenderingUpdateScheduler::isScheduled() const
{
    ASSERT_IMPLIES(m_refreshTimer.get(), m_scheduled);
    return m_scheduled;
}
    
void RenderingUpdateScheduler::startTimer(Seconds delay)
{
    LOG_WITH_STREAM(EventLoop, stream << "RenderingUpdateScheduler for page " << &m_page << " startTimer(" << delay << ")");

    ASSERT(!isScheduled());
    m_refreshTimer = makeUnique<Timer>(*this, &RenderingUpdateScheduler::displayRefreshFired);
    m_refreshTimer->startOneShot(delay);
    m_scheduled = true;
}

void RenderingUpdateScheduler::clearScheduled()
{
    m_scheduled = false;
    m_refreshTimer = nullptr;
}

DisplayRefreshMonitorFactory* RenderingUpdateScheduler::displayRefreshMonitorFactory() const
{
    return m_page.chrome().client().displayRefreshMonitorFactory();
}

void RenderingUpdateScheduler::windowScreenDidChange(PlatformDisplayID displayID)
{
    adjustRenderingUpdateFrequency();
    DisplayRefreshMonitorManager::sharedManager().windowScreenDidChange(displayID, *this);
}

void RenderingUpdateScheduler::displayRefreshFired()
{
    LOG_WITH_STREAM(EventLoop, stream << "RenderingUpdateScheduler for page " << &m_page << " displayRefreshFired()");

    tracePoint(TriggerRenderingUpdate);

    clearScheduled();
    
    if (m_page.chrome().client().shouldTriggerRenderingUpdate(m_rescheduledRenderingUpdateCount)) {
        triggerRenderingUpdate();
        m_rescheduledRenderingUpdateCount = 0;
    } else {
        scheduleRenderingUpdate();
        ++m_rescheduledRenderingUpdateCount;
    }
}

void RenderingUpdateScheduler::triggerRenderingUpdateForTesting()
{
    triggerRenderingUpdate();
}

void RenderingUpdateScheduler::triggerRenderingUpdate()
{
    m_page.chrome().client().triggerRenderingUpdate();
}

}
