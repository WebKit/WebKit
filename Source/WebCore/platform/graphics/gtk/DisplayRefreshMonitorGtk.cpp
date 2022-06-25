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

#if !USE(GTK4)

#include <gtk/gtk.h>
#include <wtf/RunLoop.h>

namespace WebCore {

constexpr FramesPerSecond DefaultFramesPerSecond = 60;

DisplayRefreshMonitorGtk::DisplayRefreshMonitorGtk(PlatformDisplayID displayID)
    : DisplayRefreshMonitor(displayID)
{
}

DisplayRefreshMonitorGtk::~DisplayRefreshMonitorGtk()
{
    ASSERT(!m_window);
}

static void onFrameClockUpdate(GdkFrameClock*, DisplayRefreshMonitorGtk* monitor)
{
    monitor->displayLinkCallbackFired();
}

void DisplayRefreshMonitorGtk::displayLinkCallbackFired()
{
    displayLinkFired(m_currentUpdate);
    m_currentUpdate = m_currentUpdate.nextUpdate();
}

void DisplayRefreshMonitorGtk::stop()
{
    if (!m_window)
        return;

    auto* frameClock = gtk_widget_get_frame_clock(m_window);
    ASSERT(frameClock);
    g_signal_handlers_disconnect_matched(frameClock, G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);

    gtk_widget_destroy(m_window);
    m_window = nullptr;
}

bool DisplayRefreshMonitorGtk::startNotificationMechanism()
{
    if (m_clockIsActive)
        return true;

    GdkFrameClock* frameClock;
    if (!m_window) {
        // GdkFrameClockIdle is private in GDK, so we need to create a toplevel to get its frame clock.
        m_window = gtk_offscreen_window_new();
        gtk_widget_realize(m_window);

        frameClock = gtk_widget_get_frame_clock(m_window);
        ASSERT(frameClock);
        g_signal_connect(frameClock, "update", G_CALLBACK(onFrameClockUpdate), this);
    } else
        frameClock = gtk_widget_get_frame_clock(m_window);

    ASSERT(frameClock);
    gdk_frame_clock_begin_updating(frameClock);

    // FIXME: Use gdk_frame_clock_get_refresh_info to get the correct frame rate.
    m_currentUpdate = { 0, DefaultFramesPerSecond };

    m_clockIsActive = true;
    return true;
}

void DisplayRefreshMonitorGtk::stopNotificationMechanism()
{
    if (!m_clockIsActive)
        return;

    auto* frameClock = gtk_widget_get_frame_clock(m_window);
    ASSERT(frameClock);
    gdk_frame_clock_end_updating(frameClock);

    m_clockIsActive = false;    
}

} // namespace WebCore

#endif // !USE(GTK4)
