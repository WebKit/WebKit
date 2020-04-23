/*
 * Copyright (C) 2018 Igalia S.L.
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
#include "DisplayRefreshMonitorGtk.h"

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)

#include <gtk/gtk.h>
#include <wtf/RunLoop.h>

namespace WebCore {

DisplayRefreshMonitorGtk::DisplayRefreshMonitorGtk(PlatformDisplayID displayID)
    : DisplayRefreshMonitor(displayID)
{
}

DisplayRefreshMonitorGtk::~DisplayRefreshMonitorGtk()
{
    if (!m_window)
        return;

    auto* frameClock = gtk_widget_get_frame_clock(m_window);
    ASSERT(frameClock);
    g_signal_handlers_disconnect_matched(frameClock, G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
    gdk_frame_clock_end_updating(frameClock);
    gtk_widget_destroy(m_window);
}

static void onFrameClockUpdate(GdkFrameClock*, DisplayRefreshMonitorGtk* monitor)
{
    monitor->displayLinkFired();
}

bool DisplayRefreshMonitorGtk::requestRefreshCallback()
{
    if (!isActive())
        return false;

    if (!m_window) {
        // GdkFrameClockIdle is private in GDK, so we need to create a toplevel to get its frame clock.
        m_window = gtk_offscreen_window_new();
        gtk_widget_realize(m_window);

        auto* frameClock = gtk_widget_get_frame_clock(m_window);
        ASSERT(frameClock);

        g_signal_connect(frameClock, "update", G_CALLBACK(onFrameClockUpdate), this);
        gdk_frame_clock_begin_updating(frameClock);

        setIsActive(true);
    }

    LockHolder lock(mutex());
    setIsScheduled(true);
    return true;
}

void DisplayRefreshMonitorGtk::displayLinkFired()
{
    {
        LockHolder lock(mutex());
        if (!isPreviousFrameDone())
            return;

        setIsPreviousFrameDone(false);
    }
    ASSERT(isMainThread());
    handleDisplayRefreshedNotificationOnMainThread(this);
}

} // namespace WebCore

#endif // USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
