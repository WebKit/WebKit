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
#include "DisplayVBlankMonitor.h"

#include "DisplayVBlankMonitorTimer.h"
#include "Logging.h"
#include <WebCore/AnimationFrameRate.h>
#include <algorithm>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>

#if PLATFORM(GTK)
#include "ScreenManager.h"
#include <gtk/gtk.h>
#endif

#if USE(LIBDRM)
#include "DisplayVBlankMonitorDRM.h"
#endif

#if ENABLE(WPE_PLATFORM)
#include "DisplayVBlankMonitorWPE.h"
#include "WPEUtilities.h"
#endif

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DisplayVBlankMonitor);

#if PLATFORM(GTK)
static unsigned refreshRateForDisplay(PlatformDisplayID displayID)
{
    auto* monitor = ScreenManager::singleton().screen(displayID);
    if (!monitor)
        return WebCore::FullSpeedFramesPerSecond;

    int refreshRate = gdk_monitor_get_refresh_rate(monitor);
    if (refreshRate <= 0)
        return WebCore::FullSpeedFramesPerSecond;

    return std::max((refreshRate + 500) / 1000, 1);
}
#endif

static std::unique_ptr<DisplayVBlankMonitor> createTimerMonitor(PlatformDisplayID displayID)
{
#if PLATFORM(GTK)
    return DisplayVBlankMonitorTimer::create(refreshRateForDisplay(displayID));
#else
    UNUSED_PARAM(displayID);
    return DisplayVBlankMonitorTimer::create(WebCore::FullSpeedFramesPerSecond);
#endif
}

IGNORE_CLANG_WARNINGS_BEGIN("unsafe-buffer-usage-in-libc-call")
std::unique_ptr<DisplayVBlankMonitor> DisplayVBlankMonitor::create(PlatformDisplayID displayID)
{
    static const char* forceTimer = getenv("WEBKIT_FORCE_VBLANK_TIMER");
    if (!displayID || (forceTimer && strcmp(forceTimer, "0"))) // NOLINT
        return createTimerMonitor(displayID);

#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI()) {
        if (auto monitor = DisplayVBlankMonitorWPE::create(displayID))
            return monitor;

        RELEASE_LOG_FAULT(DisplayLink, "Failed to create WPE vblank monitor, falling back to timer");
        return createTimerMonitor(displayID);
    }
#endif

#if USE(LIBDRM)
    if (auto monitor = DisplayVBlankMonitorDRM::create(displayID))
        return monitor;
    RELEASE_LOG_FAULT(DisplayLink, "Failed to create DRM vblank monitor, falling back to timer");
#endif

    return createTimerMonitor(displayID);
}
IGNORE_CLANG_WARNINGS_END

DisplayVBlankMonitor::DisplayVBlankMonitor(unsigned refreshRate)
    : m_refreshRate(refreshRate)
{
}

DisplayVBlankMonitor::~DisplayVBlankMonitor() = default;

void DisplayVBlankMonitor::setHandler(Function<void()>&& handler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_handler);
    m_handler = WTF::move(handler);
}

} // namespace WebKit
