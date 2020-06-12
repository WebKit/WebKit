/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2008 Christian Dywan <christian@imendio.com>
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2009 Holger Hans Peter Freyther
 * All rights reserved.
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
#include "PlatformScreen.h"

#include "FloatRect.h"
#include "FrameView.h"
#include "HostWindow.h"
#include "NotImplemented.h"
#include "Widget.h"

#include <cmath>
#include <gtk/gtk.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

#if !USE(GTK4)
static GdkVisual* systemVisual()
{
    if (auto* screen = gdk_screen_get_default())
        return gdk_screen_get_system_visual(screen);

    return nullptr;
}
#endif

int screenDepth(Widget*)
{
#if !USE(GTK4)
    if (auto* visual = systemVisual())
        return gdk_visual_get_depth(visual);
#endif

    return 24;
}

int screenDepthPerComponent(Widget*)
{
#if !USE(GTK4)
    if (auto* visual = systemVisual()) {
        int redDepth;
        gdk_visual_get_red_pixel_details(visual, nullptr, nullptr, &redDepth);
        return redDepth;
    }
#endif

    return 8;
}

bool screenIsMonochrome(Widget* widget)
{
    return screenDepth(widget) < 2;
}

bool screenHasInvertedColors()
{
    return false;
}

double screenDPI()
{
    static const double defaultDpi = 96;
#if !USE(GTK4)
    GdkScreen* screen = gdk_screen_get_default();
    if (screen) {
        double dpi = gdk_screen_get_resolution(screen);
        if (dpi != -1)
            return dpi;
    }
#endif

    static GtkSettings* gtkSettings = gtk_settings_get_default();
    if (gtkSettings) {
        int gtkXftDpi;
        g_object_get(gtkSettings, "gtk-xft-dpi", &gtkXftDpi, nullptr);
        return gtkXftDpi / 1024.0;
    }

    static double cachedDpi = 0;
    if (cachedDpi)
        return cachedDpi;

    static const double millimetresPerInch = 25.4;

    GdkDisplay* display = gdk_display_get_default();
    if (!display)
        return defaultDpi;
#if USE(GTK4)
    GdkMonitor* monitor = GDK_MONITOR(g_list_model_get_item(gdk_display_get_monitors(display), 0));
#else
    GdkMonitor* monitor = gdk_display_get_monitor(display, 0);
#endif
    if (!monitor)
        return defaultDpi;

    GdkRectangle geometry;
    gdk_monitor_get_geometry(monitor, &geometry);
    double diagonalInPixels = std::hypot(geometry.width, geometry.height);
    double diagonalInInches = std::hypot(gdk_monitor_get_width_mm(monitor), gdk_monitor_get_height_mm(monitor)) / millimetresPerInch;
    cachedDpi = diagonalInPixels / diagonalInInches;

    return cachedDpi;
}

static WTF::HashMap<void*, Function<void()>>& screenDPIObserverHandlersMap()
{
    static WTF::NeverDestroyed<WTF::HashMap<void*, Function<void()>>> handlersMap;
    return handlersMap;
}

static void gtkXftDPIChangedCallback()
{
    for (const auto& keyValuePair : screenDPIObserverHandlersMap())
        keyValuePair.value();
}

void setScreenDPIObserverHandler(Function<void()>&& handler, void* context)
{
    static GtkSettings* gtkSettings = gtk_settings_get_default();
    static unsigned long gtkXftDpiChangedHandlerID = 0;

    if (!gtkSettings)
        return;

    if (handler)
        screenDPIObserverHandlersMap().set(context, WTFMove(handler));
    else
        screenDPIObserverHandlersMap().remove(context);

    if (!screenDPIObserverHandlersMap().isEmpty()) {
        if (!gtkXftDpiChangedHandlerID)
            gtkXftDpiChangedHandlerID = g_signal_connect(gtkSettings, "notify::gtk-xft-dpi", G_CALLBACK(gtkXftDPIChangedCallback), nullptr);
    } else if (gtkXftDpiChangedHandlerID) {
        g_signal_handler_disconnect(gtkSettings, gtkXftDpiChangedHandlerID);
        gtkXftDpiChangedHandlerID = 0;
    }
}

static GRefPtr<GdkMonitor> currentScreenMonitor()
{
    GdkDisplay* display = gdk_display_get_default();
    if (!display)
        return nullptr;

#if USE(GTK4)
    return adoptGRef(static_cast<GdkMonitor*>(g_list_model_get_item(gdk_display_get_monitors(display), 0)));
#else
    auto* rootWindow = gdk_get_default_root_window();
    if (!rootWindow)
        return nullptr;

    return gdk_display_get_monitor_at_window(display, rootWindow);
#endif
}


FloatRect screenRect(Widget*)
{
    GdkRectangle geometry;

    auto monitor = currentScreenMonitor();
    if (!monitor)
        return { };

    gdk_monitor_get_geometry(monitor.get(), &geometry);

    return FloatRect(geometry.x, geometry.y, geometry.width, geometry.height);
}

FloatRect screenAvailableRect(Widget*)
{
    auto monitor = currentScreenMonitor();
    if (!monitor)
        return { };

    GdkRectangle workArea;
    gdk_monitor_get_workarea(monitor.get(), &workArea);

    return FloatRect(workArea.x, workArea.y, workArea.width, workArea.height);
}

bool screenSupportsExtendedColor(Widget*)
{
    return false;
}

#if ENABLE(TOUCH_EVENTS)
bool screenHasTouchDevice()
{
    auto* display = gdk_display_get_default();
    if (!display)
        return true;

    auto* seat = gdk_display_get_default_seat(display);
    return seat ? gdk_seat_get_capabilities(seat) & GDK_SEAT_CAPABILITY_TOUCH : true;
}

bool screenIsTouchPrimaryInputDevice()
{
    auto* display = gdk_display_get_default();
    if (!display)
        return true;

    auto* seat = gdk_display_get_default_seat(display);
    if (!seat)
        return true;

    auto* device = gdk_seat_get_pointer(seat);
    return device ? gdk_device_get_source(device) == GDK_SOURCE_TOUCHSCREEN : true;
}
#endif // ENABLE(TOUCH_EVENTS)

} // namespace WebCore
