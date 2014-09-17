/*
 * Copyright (C) 2011, Igalia S.L.
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

#include "IntPoint.h"
#include <gtk/gtk.h>

#if PLATFORM(X11)
#include <gdk/gdkx.h>
#endif
#if PLATFORM(WAYLAND) && !defined(GTK_API_VERSION_2)
#include <gdk/gdkwayland.h>
#endif

namespace WebCore {

IntPoint convertWidgetPointToScreenPoint(GtkWidget* widget, const IntPoint& point)
{
    // FIXME: This is actually a very tricky operation and the results of this function should
    // only be thought of as a guess. For instance, sometimes it may not correctly take into
    // account window decorations.

    GtkWidget* toplevelWidget = gtk_widget_get_toplevel(widget);
    if (!toplevelWidget || !gtk_widget_is_toplevel(toplevelWidget) || !GTK_IS_WINDOW(toplevelWidget))
        return point;

    GdkWindow* gdkWindow = gtk_widget_get_window(toplevelWidget);
    if (!gdkWindow)
        return point;

    int xInWindow, yInWindow;
    gtk_widget_translate_coordinates(widget, toplevelWidget, point.x(), point.y(), &xInWindow, &yInWindow);

    int windowOriginX, windowOriginY;
    gdk_window_get_origin(gdkWindow, &windowOriginX, &windowOriginY);

    return IntPoint(windowOriginX + xInWindow, windowOriginY + yInWindow);
}

bool widgetIsOnscreenToplevelWindow(GtkWidget* widget)
{
    return gtk_widget_is_toplevel(widget) && GTK_IS_WINDOW(widget) && !GTK_IS_OFFSCREEN_WINDOW(widget);
}

DisplaySystemType getDisplaySystemType()
{
#if defined(GTK_API_VERSION_2)
    return DisplaySystemType::X11;
#else
    static DisplaySystemType type = [] {
        GdkDisplay* display = gdk_display_manager_get_default_display(gdk_display_manager_get());
#if PLATFORM(X11)
        if (GDK_IS_X11_DISPLAY(display))
            return DisplaySystemType::X11;
#endif
#if PLATFORM(WAYLAND)
        if (GDK_IS_WAYLAND_DISPLAY(display))
            return DisplaySystemType::Wayland;
#endif
        ASSERT_NOT_REACHED();
        return DisplaySystemType::X11;
    }();
    return type;
#endif
}

} // namespace WebCore
