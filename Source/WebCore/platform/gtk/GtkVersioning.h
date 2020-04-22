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

#define GDK_MOD1_MASK GDK_ALT_MASK

static inline gboolean
gdk_event_get_state(GdkEvent *event, GdkModifierType *state)
{
    *state = gdk_event_get_modifier_state(event);
    // The GTK3 method returns TRUE if there is a state, otherwise
    // FALSE.
    return !!*state;
}

static inline gboolean
gdk_event_get_coords(GdkEvent *event, double *x, double *y)
{
    return gdk_event_get_position(event, x, y);
}

static inline gboolean
gdk_event_get_root_coords(GdkEvent *event, double *x, double *y)
{
    // GTK4 does not provide a way of obtaining screen-relative event coordinates, and even
    // on Wayland GTK3 cannot know where a surface is and will return the surface-relative
    // coordinates anyway, so do the same here.
    return gdk_event_get_position(event, x, y);
}

static inline gboolean
gdk_event_is_scroll_stop_event(GdkEvent* event)
{
    return gdk_scroll_event_is_stop(event);
}

static inline gboolean
gdk_event_get_scroll_direction(GdkEvent* event, GdkScrollDirection* direction)
{
    *direction = gdk_scroll_event_get_direction(event);
    // The GTK3 method returns TRUE if the scroll direction is not
    // GDK_SCROLL_SMOOTH, so do the same here.
    return *direction != GDK_SCROLL_SMOOTH;
}

static inline gboolean
gdk_event_get_scroll_deltas(GdkEvent* event, gdouble *x, gdouble *y)
{
    gdk_scroll_event_get_deltas(event, x, y);
    // The GTK3 method returns TRUE if the event is a smooth scroll
    // event, so do the same here.
    return gdk_scroll_event_get_direction(event) == GDK_SCROLL_SMOOTH;
}

static inline gboolean
gdk_event_get_button(GdkEvent* event)
{
    return gdk_button_event_get_button(event);
}
#endif // USE(GTK4)
