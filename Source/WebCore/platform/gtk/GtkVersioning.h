/*
 * Copyright (C) 2010 Collabora Ltd.
 * Copyright (C) 2010 Igalia, S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef GtkVersioning_h
#define GtkVersioning_h

#include <gtk/gtk.h>

#ifndef GTK_API_VERSION_2
#include <gdk/gdkkeysyms-compat.h>
#endif

G_BEGIN_DECLS

// Macros to avoid deprecation checking churn
#ifndef GTK_API_VERSION_2
#define GDK_WINDOW_XWINDOW(window) (gdk_x11_window_get_xid(window))
#else
GdkPixbuf* gdk_pixbuf_get_from_surface(cairo_surface_t* surface, int srcX, int srcY,
                                       int width, int height);
void gdk_screen_get_monitor_workarea(GdkScreen *, int monitor, GdkRectangle *area);
#endif

GdkDevice* getDefaultGDKPointerDevice(GdkWindow* window);

G_END_DECLS

#endif // GtkVersioning_h
