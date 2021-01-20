/*
 * Copyright (C) 2011, 2013 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "GtkUtilities.h"

#include "GtkVersioning.h"
#include "IntPoint.h"
#include <gtk/gtk.h>
#include <wtf/glib/GUniquePtr.h>

#if USE(GTK4) && PLATFORM(X11)
#include <gdk/x11/gdkx.h>
#endif

namespace WebCore {

static IntPoint gtkWindowGetOrigin(GtkWidget* window)
{
    int x = 0, y = 0;

#if USE(GTK4)
    UNUSED_PARAM(window);
#else
    if (auto* gdkWindow = gtk_widget_get_window(window))
        gdk_window_get_origin(gdkWindow, &x, &y);
#endif // !USE(GTK4)

    return IntPoint(x, y);
}

IntPoint convertWidgetPointToScreenPoint(GtkWidget* widget, const IntPoint& point)
{
    // FIXME: This is actually a very tricky operation and the results of this function should
    // only be thought of as a guess. For instance, sometimes it may not correctly take into
    // account window decorations.

    GtkWidget* toplevelWidget = gtk_widget_get_toplevel(widget);
    if (!toplevelWidget || !gtk_widget_is_toplevel(toplevelWidget) || !GTK_IS_WINDOW(toplevelWidget))
        return point;

#if USE(GTK4)
    double xInWindow, yInWindow;
#else
    int xInWindow, yInWindow;
#endif
    gtk_widget_translate_coordinates(widget, toplevelWidget, point.x(), point.y(), &xInWindow, &yInWindow);

    const auto origin = gtkWindowGetOrigin(toplevelWidget);
    return IntPoint(origin.x() + xInWindow, origin.y() + yInWindow);
}

bool widgetIsOnscreenToplevelWindow(GtkWidget* widget)
{
    const bool isToplevelWidget = widget && gtk_widget_is_toplevel(widget);

#if USE(GTK4)
    // A toplevel widget in GTK4 is always a window, there is no need for further checks.
    return isToplevelWidget;
#else
    return isToplevelWidget && GTK_IS_WINDOW(widget) && !GTK_IS_OFFSCREEN_WINDOW(widget);
#endif // USE(GTK4)
}

IntPoint widgetRootCoords(GtkWidget* widget, int x, int y)
{
#if USE(GTK4)
    UNUSED_PARAM(widget);
    return { x, y };
#else
    int xRoot, yRoot;
    gdk_window_get_root_coords(gtk_widget_get_window(widget), x, y, &xRoot, &yRoot);
    return { xRoot, yRoot };
#endif
}

void widgetDevicePosition(GtkWidget* widget, GdkDevice* device, double* x, double* y, GdkModifierType* state)
{
#if USE(GTK4)
    gdk_surface_get_device_position(gtk_native_get_surface(gtk_widget_get_native(widget)), device, x, y, state);
#else
    int xInt, yInt;
    gdk_window_get_device_position(gtk_widget_get_window(widget), device, &xInt, &yInt, state);
    *x = xInt;
    *y = yInt;
#endif
}

unsigned widgetKeyvalToKeycode(GtkWidget* widget, unsigned keyval)
{
    unsigned keycode = 0;
    GUniqueOutPtr<GdkKeymapKey> keys;
    int keysCount;
    auto* display = gtk_widget_get_display(widget);

#if USE(GTK4)
    if (gdk_display_map_keyval(display, keyval, &keys.outPtr(), &keysCount) && keysCount)
        keycode = keys.get()[0].keycode;
#else
    GdkKeymap* keymap = gdk_keymap_get_for_display(display);
    if (gdk_keymap_get_entries_for_keyval(keymap, keyval, &keys.outPtr(), &keysCount) && keysCount)
        keycode = keys.get()[0].keycode;
#endif

    return keycode;
}

template<>
WallTime wallTimeForEvent(const GdkEvent* event)
{
    // This works if and only if the X server or Wayland compositor happens to
    // be using CLOCK_MONOTONIC for its monotonic time, and so long as
    // g_get_monotonic_time() continues to do so as well, and so long as
    // WTF::MonotonicTime continues to use g_get_monotonic_time().
#if USE(GTK4)
    auto time = gdk_event_get_time(const_cast<GdkEvent*>(event));
#else
    auto time = gdk_event_get_time(event);
#endif
    if (time == GDK_CURRENT_TIME)
        return WallTime::now();
    return MonotonicTime::fromRawSeconds(time / 1000.).approximateWallTime();
}

String defaultGtkSystemFont()
{
    GUniqueOutPtr<char> fontString;
    g_object_get(gtk_settings_get_default(), "gtk-font-name", &fontString.outPtr(), nullptr);
    // We need to remove the size from the value of the property,
    // which is separated from the font family using a space.
    if (auto* spaceChar = strrchr(fontString.get(), ' '))
        *spaceChar = '\0';
    return String::fromUTF8(fontString.get());
}

unsigned stateModifierForGdkButton(unsigned button)
{
    return 1 << (8 + button - 1);
}

OptionSet<DragOperation> gdkDragActionToDragOperation(GdkDragAction gdkAction)
{
    OptionSet<DragOperation> action;
    if (gdkAction & GDK_ACTION_COPY)
        action.add(DragOperation::Copy);
    if (gdkAction & GDK_ACTION_MOVE)
        action.add(DragOperation::Move);
    if (gdkAction & GDK_ACTION_LINK)
        action.add(DragOperation::Link);

    return action;
}

GdkDragAction dragOperationToGdkDragActions(OptionSet<DragOperation> coreAction)
{
    unsigned gdkAction = 0;

    if (coreAction.contains(DragOperation::Copy))
        gdkAction |= GDK_ACTION_COPY;
    if (coreAction.contains(DragOperation::Move))
        gdkAction |= GDK_ACTION_MOVE;
    if (coreAction.contains(DragOperation::Link))
        gdkAction |= GDK_ACTION_LINK;

    return static_cast<GdkDragAction>(gdkAction);
}

GdkDragAction dragOperationToSingleGdkDragAction(OptionSet<DragOperation> coreAction)
{
    if (coreAction.contains(DragOperation::Copy))
        return GDK_ACTION_COPY;
    if (coreAction.contains(DragOperation::Move))
        return GDK_ACTION_MOVE;
    if (coreAction.contains(DragOperation::Link))
        return GDK_ACTION_LINK;
    return static_cast<GdkDragAction>(0);
}

void monitorWorkArea(GdkMonitor* monitor, GdkRectangle* area)
{
#if USE(GTK4)
#if PLATFORM(X11)
    if (GDK_IS_X11_MONITOR(monitor)) {
        gdk_x11_monitor_get_workarea(monitor, area);
        return;
    }
#endif

    gdk_monitor_get_geometry(monitor, area);
#else
    gdk_monitor_get_workarea(monitor, area);
#endif
}

} // namespace WebCore
