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
#include "ScreenManager.h"

#include "GtkSettingsManager.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include <WebCore/GtkUtilities.h>
#include <WebCore/PlatformScreen.h>
#include <cmath>

namespace WebKit {
using namespace WebCore;

ScreenManager& ScreenManager::singleton()
{
    static NeverDestroyed<ScreenManager> manager;
    return manager;
}

static PlatformDisplayID generatePlatformDisplayID()
{
    static PlatformDisplayID id;
    return ++id;
}

ScreenManager::ScreenManager()
{
    auto* display = gdk_display_get_default();
#if USE(GTK4)
    auto* monitors = gdk_display_get_monitors(display);
    auto monitorsCount = g_list_model_get_n_items(monitors);
    for (unsigned i = 0; i < monitorsCount; ++i) {
        auto monitor = adoptGRef(GDK_MONITOR(g_list_model_get_item(monitors, i)));
        addMonitor(monitor.get());
    }
    g_signal_connect(monitors, "items-changed", G_CALLBACK(+[](GListModel* monitors, guint index, guint removedCount, guint addedCount, ScreenManager* manager) {
        for (unsigned i = 0; i < removedCount; ++i)
            manager->removeMonitor(manager->m_monitors[index].get());
        for (unsigned i = 0; i < addedCount; ++i) {
            auto monitor = adoptGRef(GDK_MONITOR(g_list_model_get_item(monitors, index + i)));
            manager->addMonitor(monitor.get());
        }
        manager->propertiesDidChange();
    }), this);
#else
    auto monitorsCount = gdk_display_get_n_monitors(display);
    for (int i = 0; i < monitorsCount; ++i) {
        if (auto* monitor = gdk_display_get_monitor(display, i))
            addMonitor(monitor);
    }
    g_signal_connect(display, "monitor-added", G_CALLBACK(+[](GdkDisplay*, GdkMonitor* monitor, ScreenManager* manager) {
        manager->addMonitor(monitor);
        manager->propertiesDidChange();
    }), this);
    g_signal_connect(display, "monitor-removed", G_CALLBACK(+[](GdkDisplay*, GdkMonitor* monitor, ScreenManager* manager) {
        manager->removeMonitor(monitor);
        manager->propertiesDidChange();
    }), this);
#endif
}

PlatformDisplayID ScreenManager::displayID(GdkMonitor* monitor) const
{
    return m_monitorToDisplayIDMap.get(monitor);
}

GdkMonitor* ScreenManager::monitor(PlatformDisplayID displayID) const
{
    for (const auto& iter : m_monitorToDisplayIDMap) {
        if (iter.value == displayID)
            return iter.key;
    }
    return nullptr;
}

void ScreenManager::addMonitor(GdkMonitor* monitor)
{
    m_monitors.append(monitor);
    m_monitorToDisplayIDMap.add(monitor, generatePlatformDisplayID());
}

void ScreenManager::removeMonitor(GdkMonitor* monitor)
{
    m_monitorToDisplayIDMap.remove(monitor);
    m_monitors.removeFirstMatching([monitor](const auto& item) {
        return item.get() == monitor;
    });
}

void ScreenManager::propertiesDidChange() const
{
    auto properties = collectScreenProperties();
    for (auto& pool : WebProcessPool::allProcessPools())
        pool->sendToAllProcesses(Messages::WebProcess::SetScreenProperties(properties));
}

ScreenProperties ScreenManager::collectScreenProperties() const
{
#if USE(GTK4)
    GdkMonitor* primaryMonitor = nullptr;
#else
    auto systemVisual = [](GdkDisplay* display) -> GdkVisual* {
        if (auto* screen = gdk_display_get_default_screen(display))
            return gdk_screen_get_system_visual(screen);

        return nullptr;
    };

    auto* display = gdk_display_get_default();
    auto* primaryMonitor = gdk_display_get_primary_monitor(display);
#endif

    ScreenProperties properties;
    for (const auto& iter : m_monitorToDisplayIDMap) {
        GdkMonitor* monitor = iter.key;
        if (!properties.primaryDisplayID && (!primaryMonitor || primaryMonitor == monitor))
            properties.primaryDisplayID = iter.value;

        ScreenData data;
        GdkRectangle workArea;
        monitorWorkArea(monitor, &workArea);
        data.screenAvailableRect = FloatRect(workArea.x, workArea.y, workArea.width, workArea.height);
        GdkRectangle geometry;
        gdk_monitor_get_geometry(monitor, &geometry);
        data.screenRect = FloatRect(geometry.x, geometry.y, geometry.width, geometry.height);
#if USE(GTK4)
        data.screenDepth = 24;
        data.screenDepthPerComponent = 8;
#else
        auto* visual = systemVisual(display);
        data.screenDepth = visual ? gdk_visual_get_depth(visual) : 24;
        if (visual) {
            int redDepth;
            gdk_visual_get_red_pixel_details(visual, nullptr, nullptr, &redDepth);
            data.screenDepthPerComponent = redDepth;
        } else
            data.screenDepthPerComponent = 8;
#endif
        data.screenSize = { gdk_monitor_get_width_mm(monitor), gdk_monitor_get_height_mm(monitor) };
        static const double millimetresPerInch = 25.4;
        double diagonalInPixels = std::hypot(geometry.width, geometry.height);
        double diagonalInInches = std::hypot(data.screenSize.width(), data.screenSize.height()) / millimetresPerInch;
        data.dpi = diagonalInPixels / diagonalInInches;
        properties.screenDataMap.add(iter.value, WTFMove(data));
    }

    // FIXME: don't use PlatformScreen from the UI process, better use ScreenManager directly.
    WebCore::setScreenProperties(properties);

    return properties;
}

} // namespace WebKit
