/*
 * Copyright (C) 2020 Igalia S.L.
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include <gtk/gtk.h>

#if USE(GTK4)

static inline gboolean
gtk_widget_is_toplevel(GtkWidget* widget)
{
    // In theory anything which implements GtkRoot can be a toplevel widget,
    // in practice only the ones which are GtkWindow or derived need to be
    // considered here.
    return GTK_IS_WINDOW(widget);
}

static inline GtkWidget*
gtk_widget_get_toplevel(GtkWidget* widget)
{
    return GTK_WIDGET(gtk_widget_get_root(widget));
}

static inline void
gtk_window_get_position(GtkWindow*, int* x, int* y)
{
    *x = *y = 0;
}

static inline void
gtk_init(int*, char***)
{
    gtk_init();
}

static inline gboolean
gtk_init_check(int*, char***)
{
    return gtk_init_check();
}

static inline GdkKeymap*
gdk_keymap_get_for_display(GdkDisplay *display)
{
    return gdk_display_get_keymap(display);
}

#endif // USE(GTK4)
