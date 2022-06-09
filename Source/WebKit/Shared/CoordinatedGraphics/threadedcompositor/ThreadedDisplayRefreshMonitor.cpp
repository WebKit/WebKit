/*
 * Copyright (C) 2014 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ThreadedDisplayRefreshMonitor.h"

#if USE(COORDINATED_GRAPHICS)

#include "CompositingRunLoop.h"
#include "ThreadedCompositor.h"

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace WebKit {

// FIXME: Use the correct frame rate.
constexpr WebCore::FramesPerSecond DefaultFramesPerSecond = 60;

ThreadedDisplayRefreshMonitor::ThreadedDisplayRefreshMonitor(WebCore::PlatformDisplayID displayID, Client& client)
    : WebCore::DisplayRefreshMonitor(displayID)
    , m_displayRefreshTimer(RunLoop::main(), this, &ThreadedDisplayRefreshMonitor::displayRefreshCallback)
    , m_client(&client)
    , m_currentUpdate({ 0, DefaultFramesPerSecond })
{
#if USE(GLIB_EVENT_LOOP)
    m_displayRefreshTimer.setPriority(RunLoopSourcePriority::DisplayRefreshMonitorTimer);
    m_displayRefreshTimer.setName("[WebKit] ThreadedDisplayRefreshMonitor");
#endif
}

bool ThreadedDisplayRefreshMonitor::requestRefreshCallback()
{
    if (!m_client)
        return false;

    bool previousFrameDone { false };
    {
        Locker locker { lock() };
        setIsScheduled(true);
        previousFrameDone = isPreviousFrameDone();
    }

    // Only request an update in case we're not currently handling the display
    // refresh notifications under ThreadedDisplayRefreshMonitor::displayRefreshCallback().
    // Any such schedule request is handled in that method after the notifications.
    if (previousFrameDone)
        m_client->requestDisplayRefreshMonitorUpdate();

    return true;
}

bool ThreadedDisplayRefreshMonitor::requiresDisplayRefreshCallback()
{
    Locker locker { lock() };
    return isScheduled() && isPreviousFrameDone();
}

void ThreadedDisplayRefreshMonitor::dispatchDisplayRefreshCallback()
{
    if (!m_client)
        return;
    m_displayRefreshTimer.startOneShot(0_s);
}

void ThreadedDisplayRefreshMonitor::invalidate()
{
    m_displayRefreshTimer.stop();
    bool wasScheduled = false;
    {
        Locker locker { lock() };
        wasScheduled = isScheduled();
    }
    if (wasScheduled) {
        displayDidRefresh(m_currentUpdate);
        m_currentUpdate = m_currentUpdate.nextUpdate();
    }
    m_client = nullptr;
}

// FIXME: Refactor to share more code with DisplayRefreshMonitor::displayLinkFired().
void ThreadedDisplayRefreshMonitor::displayRefreshCallback()
{
    bool shouldHandleDisplayRefreshNotification { false };
    {
        Locker locker { lock() };
        shouldHandleDisplayRefreshNotification = isScheduled() && isPreviousFrameDone();
        if (shouldHandleDisplayRefreshNotification) {
            setIsScheduled(false);
            setIsPreviousFrameDone(false);
        }
    }

    if (shouldHandleDisplayRefreshNotification) {
        displayDidRefresh(m_currentUpdate);
        m_currentUpdate = m_currentUpdate.nextUpdate();
    }

    // Retrieve the scheduled status for this DisplayRefreshMonitor.
    bool hasBeenRescheduled { false };
    {
        Locker locker { lock() };
        hasBeenRescheduled = isScheduled();
    }

    // Notify the compositor about the completed DisplayRefreshMonitor update, passing
    // along information about any schedule request that might have occurred during
    // the notification handling.
    if (m_client)
        m_client->handleDisplayRefreshMonitorUpdate(hasBeenRescheduled);
}

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
