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

#include "IntPoint.h"
#include <gtk/gtk.h>
#include <wtf/glib/GUniquePtr.h>

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
    return widget && gtk_widget_is_toplevel(widget) && GTK_IS_WINDOW(widget) && !GTK_IS_OFFSCREEN_WINDOW(widget);
}

template<>
WallTime wallTimeForEvent(const GdkEvent* event)
{
    // This works if and only if the X server or Wayland compositor happens to
    // be using CLOCK_MONOTONIC for its monotonic time, and so long as
    // g_get_monotonic_time() continues to do so as well, and so long as
    // WTF::MonotonicTime continues to use g_get_monotonic_time().
    auto time = gdk_event_get_time(event);
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

} // namespace WebCore
