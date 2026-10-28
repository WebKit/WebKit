/*
 * Copyright (C) 2023 Igalia S.L.
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
#include "DisplayVBlankMonitorTimer.h"

#include <WebCore/AnimationFrameRate.h>
#include <chrono>
#include <thread>

#if PLATFORM(GTK)
#include "ScreenManager.h"
#include <gtk/gtk.h>
#endif

namespace WebKit {

std::unique_ptr<DisplayVBlankMonitor> DisplayVBlankMonitorTimer::create(PlatformDisplayID displayID)
{
    unsigned refreshRate = WebCore::FullSpeedFramesPerSecond;
#if PLATFORM(GTK)
    if (auto* monitor = ScreenManager::singleton().screen(displayID)) {
        const int monitorRefreshRate = gdk_monitor_get_refresh_rate(monitor);
        if (monitorRefreshRate >= 1000)
            refreshRate = monitorRefreshRate / 1000;
    }
#else
    UNUSED_PARAM(displayID);
#endif
    return makeUnique<DisplayVBlankMonitorTimer>(refreshRate);
}

DisplayVBlankMonitorTimer::DisplayVBlankMonitorTimer(unsigned refreshRate)
    : DisplayVBlankMonitorThreaded(refreshRate)
{
}

bool DisplayVBlankMonitorTimer::waitForVBlank() const
{
    const auto period = std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(1.0 / m_refreshRate));
    const auto now = std::chrono::steady_clock::now();
    if (!m_nextTick.time_since_epoch().count() || now - m_nextTick >= period)
        m_nextTick = now;
    m_nextTick += period;
    std::this_thread::sleep_until(m_nextTick);
    return true;
}

} // namespace WebKit
