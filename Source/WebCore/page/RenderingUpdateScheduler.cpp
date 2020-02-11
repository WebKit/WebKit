/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "Page.h"
#include <wtf/SystemTracing.h>

namespace WebCore {

RenderingUpdateScheduler::RenderingUpdateScheduler(Page& page)
    : m_page(page)
{
#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    windowScreenDidChange(page.chrome().displayID());
#endif
}

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR) && PLATFORM(IOS_FAMILY)
void RenderingUpdateScheduler::adjustFramesPerSecond()
{
    Seconds interval = m_page.preferredRenderingUpdateInterval();
    // CADisplayLink.preferredFramesPerSecond is an integer. So a fraction PreferredFramesPerSecond can't be set.
    if (interval < 1_s)
        DisplayRefreshMonitorManager::sharedManager().setPreferredFramesPerSecond(*this, preferredFramesPerSecond(interval));
}
#endif

void RenderingUpdateScheduler::adjustRenderingUpdateFrequency()
{
#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR) && PLATFORM(IOS_FAMILY)
    adjustFramesPerSecond();
#endif
    if (isScheduled()) {
        clearScheduled();
        scheduleTimedRenderingUpdate();
    }
}

void RenderingUpdateScheduler::scheduleTimedRenderingUpdate()
{
    if (isScheduled())
        return;

    // Optimize the case when an invisible page wants just to schedule layer flush.
    if (!m_page.isVisible()) {
        scheduleImmediateRenderingUpdate();
        return;
    }

    tracePoint(ScheduleRenderingUpdate);

    Seconds interval = m_page.preferredRenderingUpdateInterval();

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    // CADisplayLink.preferredFramesPerSecond is an integer. Fall back to timer if the PreferredFramesPerSecond is a fraction.
    if (interval < 1_s) {
#if PLATFORM(IOS_FAMILY)
        if (!m_isMonitorCreated) {
            adjustFramesPerSecond();
            m_isMonitorCreated = true;
        }
#else
        if (interval == FullSpeedAnimationInterval)
#endif
            m_scheduled = DisplayRefreshMonitorManager::sharedManager().scheduleAnimation(*this);
    }
#endif

    if (!isScheduled())
        startTimer(interval);
}

bool RenderingUpdateScheduler::isScheduled() const
{
    ASSERT_IMPLIES(m_refreshTimer.get(), m_scheduled);
    return m_scheduled;
}
    
void RenderingUpdateScheduler::startTimer(Seconds delay)
{
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

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
RefPtr<DisplayRefreshMonitor> RenderingUpdateScheduler::createDisplayRefreshMonitor(PlatformDisplayID displayID) const
{
    if (auto monitor = m_page.chrome().client().createDisplayRefreshMonitor(displayID))
        return monitor;

    return DisplayRefreshMonitor::createDefaultDisplayRefreshMonitor(displayID);
}

void RenderingUpdateScheduler::windowScreenDidChange(PlatformDisplayID displayID)
{
    DisplayRefreshMonitorManager::sharedManager().windowScreenDidChange(displayID, *this);
}
#endif

void RenderingUpdateScheduler::displayRefreshFired()
{
    tracePoint(TriggerRenderingUpdate);

    clearScheduled();
    scheduleImmediateRenderingUpdate();
}

void RenderingUpdateScheduler::scheduleImmediateRenderingUpdate()
{
    m_page.chrome().client().scheduleCompositingLayerFlush();
}

void RenderingUpdateScheduler::scheduleRenderingUpdate()
{
    if (m_page.chrome().client().needsImmediateRenderingUpdate())
        scheduleImmediateRenderingUpdate();
    else
        scheduleTimedRenderingUpdate();
}

}
