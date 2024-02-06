/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "ScreenManager.h"

#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
#include <WebCore/PlatformScreen.h>
#include <cmath>
#include <wpe/wpe-platform.h>

namespace WebKit {
using namespace WebCore;

PlatformDisplayID ScreenManager::generatePlatformDisplayID(WPEMonitor* monitor)
{
    return wpe_monitor_get_id(monitor);
}

ScreenManager::ScreenManager()
{
    // FIXME: default might not be the right display.
    auto* display = wpe_display_get_default();
    auto monitorsCount = wpe_display_get_n_monitors(display);
    for (unsigned i = 0; i < monitorsCount; ++i) {
        if (auto* monitor = wpe_display_get_monitor(display, i))
            addMonitor(monitor);
    }

    g_signal_connect(display, "monitor-added", G_CALLBACK(+[](WPEDisplay*, WPEMonitor* monitor, ScreenManager* manager) {
        manager->addMonitor(monitor);
        manager->updatePrimaryDisplayID();
        manager->propertiesDidChange();
    }), this);
    g_signal_connect(display, "monitor-removed", G_CALLBACK(+[](WPEDisplay*, WPEMonitor* monitor, ScreenManager* manager) {
        manager->removeMonitor(monitor);
        manager->updatePrimaryDisplayID();
        manager->propertiesDidChange();
    }), this);
}

void ScreenManager::updatePrimaryDisplayID()
{
    // Assume the first monitor is the primary one.
    // FIXME: default might not be the right display.
    auto* display = wpe_display_get_default();
    auto monitorsCount = wpe_display_get_n_monitors(display);
    auto* monitor = monitorsCount ? wpe_display_get_monitor(display, 0) : nullptr;
    m_primaryDisplayID = monitor ? displayID(monitor) : 0;
}

ScreenProperties ScreenManager::collectScreenProperties() const
{
    ScreenProperties properties;
    properties.primaryDisplayID = m_primaryDisplayID;

    for (const auto& iter : m_monitorToDisplayIDMap) {
        WPEMonitor* monitor = iter.key;
        auto width = wpe_monitor_get_width(monitor);
        auto height = wpe_monitor_get_height(monitor);

        ScreenData data;
        data.screenRect = FloatRect(wpe_monitor_get_x(monitor), wpe_monitor_get_y(monitor), width, height);
        data.screenAvailableRect = data.screenRect;
        data.screenDepth = 24;
        data.screenDepthPerComponent = 8;
        data.screenSize = { wpe_monitor_get_physical_width(monitor), wpe_monitor_get_physical_height(monitor) };
        static constexpr double millimetresPerInch = 25.4;
        double diagonalInPixels = std::hypot(width, height);
        double diagonalInInches = std::hypot(data.screenSize.width(), data.screenSize.height()) / millimetresPerInch;
        data.dpi = diagonalInPixels / diagonalInInches;
        properties.screenDataMap.add(iter.value, WTFMove(data));
    }

    // FIXME: don't use PlatformScreen from the UI process, better use ScreenManager directly.
    WebCore::setScreenProperties(properties);

    return properties;
}

} // namespace WebKit

#endif // PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
