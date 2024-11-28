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

PlatformDisplayID ScreenManager::generatePlatformDisplayID(WPEScreen* screen)
{
    return wpe_screen_get_id(screen);
}

ScreenManager::ScreenManager()
{
    auto* display = wpe_display_get_primary();
    auto screensCount = wpe_display_get_n_screens(display);
    for (unsigned i = 0; i < screensCount; ++i) {
        if (auto* screen = wpe_display_get_screen(display, i))
            addScreen(screen);
    }

    g_signal_connect(display, "screen-added", G_CALLBACK(+[](WPEDisplay*, WPEScreen* screen, ScreenManager* manager) {
        manager->addScreen(screen);
        manager->updatePrimaryDisplayID();
        manager->propertiesDidChange();
    }), this);
    g_signal_connect(display, "screen-removed", G_CALLBACK(+[](WPEDisplay*, WPEScreen* screen, ScreenManager* manager) {
        manager->removeScreen(screen);
        manager->updatePrimaryDisplayID();
        manager->propertiesDidChange();
    }), this);
}

void ScreenManager::updatePrimaryDisplayID()
{
    // Assume the first screen is the primary one.
    auto* display = wpe_display_get_primary();
    auto screensCount = wpe_display_get_n_screens(display);
    auto* screen = screensCount ? wpe_display_get_screen(display, 0) : nullptr;
    m_primaryDisplayID = screen ? displayID(screen) : 0;
}

ScreenProperties ScreenManager::collectScreenProperties() const
{
    ScreenProperties properties;
    properties.primaryDisplayID = m_primaryDisplayID;

    for (const auto& iter : m_screenToDisplayIDMap) {
        WPEScreen* screen = iter.key;
        auto width = wpe_screen_get_width(screen);
        auto height = wpe_screen_get_height(screen);

        ScreenData data;
        data.screenRect = FloatRect(wpe_screen_get_x(screen), wpe_screen_get_y(screen), width, height);
        data.screenAvailableRect = data.screenRect;
        data.screenDepth = 24;
        data.screenDepthPerComponent = 8;
        data.screenSize = { wpe_screen_get_physical_width(screen), wpe_screen_get_physical_height(screen) };
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
