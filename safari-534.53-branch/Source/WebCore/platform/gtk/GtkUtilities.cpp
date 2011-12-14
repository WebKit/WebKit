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

#include "IntRect.h"
#include <gtk/gtk.h>

namespace WebCore {

IntRect convertWidgetRectToScreenRect(GtkWidget* widget, const IntRect& rect)
{
    // FIXME: This is actually a very tricky operation and the results of this function should
    // only be thought of as a guess. For instance, sometimes it may not correctly take into
    // account window decorations.

    GtkWidget* toplevelWidget = gtk_widget_get_toplevel(widget);
    if (!toplevelWidget || !gtk_widget_is_toplevel(toplevelWidget) || !GTK_IS_WINDOW(toplevelWidget))
        return rect;

    int xInWindow, yInWindow;
    gtk_widget_translate_coordinates(widget, toplevelWidget, rect.x(), rect.y(), &xInWindow, &yInWindow);
    int windowOriginX, windowOriginY;
    gdk_window_get_origin(gtk_widget_get_window(toplevelWidget), &windowOriginX, &windowOriginY);

    IntRect rectInScreenCoordinates(rect);
    rectInScreenCoordinates.move(windowOriginX + xInWindow, windowOriginY + yInWindow);
    return rectInScreenCoordinates;
}

} // namespace WebCore
