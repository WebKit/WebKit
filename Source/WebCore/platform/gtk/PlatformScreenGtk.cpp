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
#include "GtkVersioning.h"
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

static GtkWidget* getToplevel(GtkWidget* widget)
{
    GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
    return gtk_widget_is_toplevel(toplevel) ? toplevel : 0;
}

static GdkVisual* getVisual(Widget* widget)
{
    GtkWidget* container = widget ? GTK_WIDGET(widget->root()->hostWindow()->platformPageClient()) : 0;
    if (!container) {
        GdkScreen* screen = gdk_screen_get_default();
        return screen ? gdk_screen_get_system_visual(screen) : 0;
    }

    if (!gtk_widget_get_realized(container))
        container = getToplevel(container);
    return container ? gdk_window_get_visual(gtk_widget_get_window(container)) : 0;
}

int screenDepth(Widget* widget)
{
    GdkVisual* visual = getVisual(widget);
    if (!visual)
        return 24;
    return gdk_visual_get_depth(visual);
}

int screenDepthPerComponent(Widget* widget)
{
    GdkVisual* visual = getVisual(widget);
    if (!visual)
        return 8;

    return gdk_visual_get_bits_per_rgb(visual);
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
    GdkScreen* screen = gdk_screen_get_default();
    if (!screen)
        return defaultDpi;

    double dpi = gdk_screen_get_resolution(screen);
    if (dpi != -1)
        return dpi;

    static double cachedDpi = 0;
    if (cachedDpi)
        return cachedDpi;

    static const double millimetresPerInch = 25.4;
    double diagonalInPixels = std::hypot(gdk_screen_get_width(screen), gdk_screen_get_height(screen));
    double diagonalInInches = std::hypot(gdk_screen_get_width_mm(screen), gdk_screen_get_height_mm(screen)) / millimetresPerInch;
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

static GdkScreen* getScreen(GtkWidget* widget)
{
    return gtk_widget_has_screen(widget) ? gtk_widget_get_screen(widget) : gdk_screen_get_default();
}

FloatRect screenRect(Widget* widget)
{
    GtkWidget* container = widget ? GTK_WIDGET(widget->root()->hostWindow()->platformPageClient()) : 0;
    if (container)
        container = getToplevel(container);

    GdkScreen* screen = container ? getScreen(container) : gdk_screen_get_default();
    if (!screen)
        return FloatRect();

    gint monitor = container ? gdk_screen_get_monitor_at_window(screen, gtk_widget_get_window(container)) : 0;

    GdkRectangle geometry;
    gdk_screen_get_monitor_geometry(screen, monitor, &geometry);

    return FloatRect(geometry.x, geometry.y, geometry.width, geometry.height);
}

FloatRect screenAvailableRect(Widget* widget)
{
    GtkWidget* container = widget ? GTK_WIDGET(widget->root()->hostWindow()->platformPageClient()) : 0;
    if (container && !gtk_widget_get_realized(container))
        return screenRect(widget);

    GdkScreen* screen = container ? getScreen(container) : gdk_screen_get_default();
    if (!screen)
        return FloatRect();

    gint monitor = container ? gdk_screen_get_monitor_at_window(screen, gtk_widget_get_window(container)) : 0;

    GdkRectangle workArea;
    gdk_screen_get_monitor_workarea(screen, monitor, &workArea);

    return FloatRect(workArea.x, workArea.y, workArea.width, workArea.height);

}

bool screenSupportsExtendedColor(Widget*)
{
    return false;
}

#if ENABLE(TOUCH_EVENTS)
#ifdef GTK_API_VERSION_2
bool screenHasTouchDevice()
{
    return false;
}

bool screenIsTouchPrimaryInputDevice()
{
    return false;
}
#else // GTK_API_VERSION_2
#if !GTK_CHECK_VERSION(3, 20, 0)
static void deviceAddedCallback(GdkDeviceManager*, GdkDevice*);
static void deviceRemovedCallback(GdkDeviceManager*, GdkDevice*);

static HashSet<GdkDevice*>& touchDevices()
{
    static LazyNeverDestroyed<HashSet<GdkDevice*>> touchDeviceSet;
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        touchDeviceSet.construct();

        auto* display = gdk_display_get_default();
        if (!display)
            return;

        auto* deviceManager = gdk_display_get_device_manager(display);
        if (!deviceManager)
            return;

        GUniquePtr<GList> devices(gdk_device_manager_list_devices(deviceManager, GDK_DEVICE_TYPE_SLAVE));
        for (GList* it = devices.get(); it; it = g_list_next(it)) {
            auto* device = GDK_DEVICE(it->data);
            if (gdk_device_get_source(device) == GDK_SOURCE_TOUCHSCREEN)
                touchDeviceSet->add(device);
        }

        g_signal_connect(deviceManager, "device-added", G_CALLBACK(deviceAddedCallback), nullptr);
        g_signal_connect(deviceManager, "device-removed", G_CALLBACK(deviceRemovedCallback), nullptr);
    });

    return touchDeviceSet;
}

static inline bool isTouchDevice(GdkDevice* device)
{
    return gdk_device_get_device_type(device) == GDK_DEVICE_TYPE_SLAVE && gdk_device_get_source(device) == GDK_SOURCE_TOUCHSCREEN;
}

static void deviceAddedCallback(GdkDeviceManager*, GdkDevice* device)
{
    if (!isTouchDevice(device))
        return;

    ASSERT(!touchDevices().contains(device));
    touchDevices().add(device);
}

static void deviceRemovedCallback(GdkDeviceManager*, GdkDevice* device)
{
    if (!isTouchDevice(device))
        return;

    ASSERT(touchDevices().contains(device));
    touchDevices().remove(device);
}
#endif // !GTK_CHECK_VERSION(3, 20, 0)

bool screenHasTouchDevice()
{
#if GTK_CHECK_VERSION(3, 20, 0)
    auto* display = gdk_display_get_default();
    if (!display)
        return true;

    auto* seat = gdk_display_get_default_seat(display);
    return seat ? gdk_seat_get_capabilities(seat) & GDK_SEAT_CAPABILITY_TOUCH : true;
#else
    return !touchDevices().isEmpty();
#endif
}

bool screenIsTouchPrimaryInputDevice()
{
    auto* display = gdk_display_get_default();
    if (!display)
        return true;

#if GTK_CHECK_VERSION(3, 20, 0)
    auto* seat = gdk_display_get_default_seat(display);
    if (!seat)
        return true;

    auto* device = gdk_seat_get_pointer(seat);
#else
    auto* deviceManager = gdk_display_get_device_manager(display);
    if (!deviceManager)
        return true;

    auto* device = gdk_device_manager_get_client_pointer(deviceManager);
#endif

    return device ? gdk_device_get_source(device) == GDK_SOURCE_TOUCHSCREEN : true;
}
#endif // !GTK_API_VERSION_2
#endif // ENABLE(TOUCH_EVENTS)

} // namespace WebCore
