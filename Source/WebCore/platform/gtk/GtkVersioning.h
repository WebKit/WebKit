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

#define GDK_MOD1_MASK GDK_ALT_MASK

typedef GdkKeyEvent GdkEventKey;
typedef GdkFocusEvent GdkEventFocus;

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
gtk_widget_destroy(GtkWidget* widget)
{
    ASSERT(GTK_IS_WINDOW(widget));
    gtk_window_destroy(GTK_WINDOW(widget));
}

static inline void
gtk_window_get_position(GtkWindow*, int* x, int* y)
{
    *x = *y = 0;
}

static inline void
gtk_window_move(GtkWindow*, int, int)
{
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

static inline GdkEvent*
gdk_event_copy(GdkEvent* event)
{
    return gdk_event_ref(event);
}

static inline void
gtk_widget_size_allocate(GtkWidget* widget, GtkAllocation* allocation)
{
    gtk_widget_size_allocate(widget, allocation, -1);
}

static inline void
gtk_widget_queue_resize_no_redraw(GtkWidget* widget)
{
    gtk_widget_queue_resize(widget);
}

static inline void
gtk_entry_set_text(GtkEntry* entry, const char* text)
{
    gtk_editable_set_text(GTK_EDITABLE(entry), text);
}

static inline const char*
gtk_entry_get_text(GtkEntry* entry)
{
    return gtk_editable_get_text(GTK_EDITABLE(entry));
}

static inline void
gtk_label_set_line_wrap(GtkLabel* label, gboolean enable)
{
    gtk_label_set_wrap(label, enable);
}

static inline void
gtk_window_set_default(GtkWindow* window, GtkWidget* widget)
{
    gtk_window_set_default_widget(window, widget);
}

static inline gboolean
gdk_event_get_state(const GdkEvent *event, GdkModifierType *state)
{
    *state = gdk_event_get_modifier_state(const_cast<GdkEvent*>(event));
    // The GTK3 method returns TRUE if there is a state, otherwise
    // FALSE.
    return !!*state;
}

static inline gboolean
gdk_event_get_coords(const GdkEvent *event, double *x, double *y)
{
    return gdk_event_get_position(const_cast<GdkEvent*>(event), x, y);
}

static inline gboolean
gdk_event_get_root_coords(const GdkEvent *event, double *x, double *y)
{
    // GTK4 does not provide a way of obtaining screen-relative event coordinates, and even
    // on Wayland GTK3 cannot know where a surface is and will return the surface-relative
    // coordinates anyway, so do the same here.
    return gdk_event_get_position(const_cast<GdkEvent*>(event), x, y);
}

static inline gboolean
gdk_event_is_scroll_stop_event(const GdkEvent* event)
{
    return gdk_scroll_event_is_stop(const_cast<GdkEvent*>(event));
}

static inline gboolean
gdk_event_get_scroll_direction(const GdkEvent* event, GdkScrollDirection* direction)
{
    *direction = gdk_scroll_event_get_direction(const_cast<GdkEvent*>(event));
    // The GTK3 method returns TRUE if the scroll direction is not
    // GDK_SCROLL_SMOOTH, so do the same here.
    return *direction != GDK_SCROLL_SMOOTH;
}

static inline gboolean
gdk_event_get_scroll_deltas(const GdkEvent* event, gdouble *x, gdouble *y)
{
    gdk_scroll_event_get_deltas(const_cast<GdkEvent*>(event), x, y);
    // The GTK3 method returns TRUE if the event is a smooth scroll
    // event, so do the same here.
    return gdk_scroll_event_get_direction(const_cast<GdkEvent*>(event)) == GDK_SCROLL_SMOOTH;
}

static inline gboolean
gdk_event_get_button(const GdkEvent* event, guint* button)
{
    if (button)
        *button = gdk_button_event_get_button(const_cast<GdkEvent*>(event));
    return true;
}

static inline gboolean
gdk_event_get_keyval(const GdkEvent* event, guint* keyval)
{
    if (keyval)
        *keyval = gdk_key_event_get_keyval(const_cast<GdkEvent*>(event));
    return TRUE;
}

static inline gboolean
gdk_event_get_keycode(const GdkEvent* event, guint16* keycode)
{
    if (keycode)
        *keycode = gdk_key_event_get_keycode(const_cast<GdkEvent*>(event));
    return TRUE;
}

static inline int
gtk_native_dialog_run(GtkNativeDialog* dialog)
{
    struct RunDialogContext {
        GMainLoop *loop;
        int response;
    } context = { g_main_loop_new(nullptr, FALSE), 0 };

    gtk_native_dialog_show(dialog);
    g_signal_connect(dialog, "response", G_CALLBACK(+[](GtkNativeDialog*, int response, RunDialogContext* context) {
        context->response = response;
        g_main_loop_quit(context->loop);
    }), &context);
    g_main_loop_run(context.loop);
    g_main_loop_unref(context.loop);

    return context.response;
}

static inline int
gtk_dialog_run(GtkDialog* dialog)
{
    struct RunDialogContext {
        GMainLoop *loop;
        int response;
    } context = { g_main_loop_new(nullptr, FALSE), 0 };

    gtk_widget_show(GTK_WIDGET(dialog));
    g_signal_connect(dialog, "response", G_CALLBACK(+[](GtkDialog*, int response, RunDialogContext* context) {
        context->response = response;
        g_main_loop_quit(context->loop);
    }), &context);
    g_main_loop_run(context.loop);
    g_main_loop_unref(context.loop);

    return context.response;
}

static inline void
gtk_tree_view_column_cell_get_size(GtkTreeViewColumn* column, const GdkRectangle*, gint* xOffset, gint* yOffset, gint* width, gint* height)
{
    gtk_tree_view_column_cell_get_size(column, xOffset, yOffset, width, height);
}

#else // USE(GTK4)

static inline void
gtk_widget_add_css_class(GtkWidget* widget, const char* name)
{
    gtk_style_context_add_class(gtk_widget_get_style_context(widget), name);
}

static inline void
gtk_window_minimize(GtkWindow* window)
{
    gtk_window_iconify(window);
}

static inline void
gtk_window_unminimize(GtkWindow* window)
{
    gtk_window_deiconify(window);
}

#endif // USE(GTK4)
