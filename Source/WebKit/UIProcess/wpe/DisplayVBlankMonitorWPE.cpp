/*
 * Copyright (C) 2025 Igalia S.L.
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
#include "DisplayVBlankMonitorWPE.h"

#if ENABLE(WPE_PLATFORM)
#include "Logging.h"
#include "ScreenManager.h"
#include <wpe/wpe-platform.h>

namespace WebKit {

std::unique_ptr<DisplayVBlankMonitor> DisplayVBlankMonitorWPE::create(PlatformDisplayID displayID)
{
    auto* screen = ScreenManager::singleton().screen(displayID);
    if (!screen) {
        RELEASE_LOG_FAULT(DisplayLink, "Could not create a vblank monitor for display %u: no screen found", displayID);
        return nullptr;
    }

    GRefPtr<WPEScreenSyncObserver> observer = adoptGRef(wpe_screen_get_sync_observer(screen));
    if (!observer) {
        RELEASE_LOG_FAULT(DisplayLink, "Could not create a vblank monitor for display %u: screen sync not supported by WPE platform", displayID);
        return nullptr;
    }

    return makeUnique<DisplayVBlankMonitorWPE>(wpe_screen_get_refresh_rate(screen) / 1000, WTFMove(observer));
}

DisplayVBlankMonitorWPE::DisplayVBlankMonitorWPE(unsigned refreshRate, GRefPtr<WPEScreenSyncObserver>&& observer)
    : DisplayVBlankMonitor(refreshRate)
    , m_observer(WTFMove(observer))
{
    wpe_screen_sync_observer_set_callback(m_observer.get(), +[](WPEScreenSyncObserver* observer, gpointer userData) {
        auto* monitor = static_cast<DisplayVBlankMonitorWPE*>(userData);
        monitor->m_handler();
    }, this, nullptr);
}

DisplayVBlankMonitorWPE::~DisplayVBlankMonitorWPE()
{
    ASSERT(!m_observer);
}

void DisplayVBlankMonitorWPE::start()
{
    if (!m_observer)
        return;

    wpe_screen_sync_observer_start(m_observer.get());
}

void DisplayVBlankMonitorWPE::stop()
{
    if (!m_observer)
        return;

    wpe_screen_sync_observer_stop(m_observer.get());
}

void DisplayVBlankMonitorWPE::invalidate()
{
    if (!m_observer)
        return;

    if (isActive())
        stop();

    m_observer = nullptr;
}

bool DisplayVBlankMonitorWPE::isActive()
{
    return m_observer && wpe_screen_sync_observer_is_active(m_observer.get());
}

} // namespace WebKit

#endif // ENABLE(WPE_PLATFORM)
