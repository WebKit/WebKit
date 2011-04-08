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
#endif

#if !GTK_CHECK_VERSION(2, 23, 4)
#define gdk_pixmap_get_size gdk_drawable_get_size
#endif // GTK_CHECK_VERSION(2, 23, 4)

#if !GTK_CHECK_VERSION(2, 23, 0)
#define gdk_window_get_display(window) gdk_drawable_get_display(window)
#define gdk_window_get_visual gdk_drawable_get_visual
#endif // GTK_CHECK_VERSION(2, 23, 0)

#if !GTK_CHECK_VERSION(2, 22, 0)
cairo_surface_t* gdk_window_create_similar_surface(GdkWindow* window, cairo_content_t content, int width, int height);
#endif // GTK_CHECK_VERSION(2, 22, 0)

#if !GTK_CHECK_VERSION(2, 21, 2)
#define gdk_visual_get_depth(visual) (visual)->depth
#define gdk_visual_get_bits_per_rgb(visual) (visual)->bits_per_rgb
#define gdk_drag_context_get_selected_action(context) (context)->action
#define gdk_drag_context_get_actions(context) (context)->actions
#endif // GTK_CHECK_VERSION(2, 21, 2)

#if !GTK_CHECK_VERSION(2, 20, 0)
#define gtk_widget_get_realized(widget) GTK_WIDGET_REALIZED(widget)
#define gtk_widget_set_realized(widget, TRUE) GTK_WIDGET_SET_FLAGS((widget), GTK_REALIZED)
#define gtk_range_get_min_slider_size(range) (range)->min_slider_size
#endif // GTK_CHECK_VERSION(2, 20, 0)

#if !GTK_CHECK_VERSION(2, 19, 0)
#define gtk_widget_is_toplevel(widget) GTK_WIDGET_TOPLEVEL(widget)
#define gtk_widget_get_realized(widget) GTK_WIDGET_REALIZED(widget)
#define gtk_widget_get_has_window(widget) !GTK_WIDGET_NO_WINDOW(widget)
#define gtk_widget_get_can_focus(widget) GTK_WIDGET_CAN_FOCUS(widget)
#define gtk_widget_is_sensitive(widget) GTK_WIDGET_IS_SENSITIVE(widget)
#endif // GTK_CHECK_VERSION(2, 19, 0)

#if !GTK_CHECK_VERSION(2, 18, 0)
#define gtk_widget_set_visible(widget, FALSE) GTK_WIDGET_UNSET_FLAGS((widget), GTK_VISIBLE)
#define gtk_widget_get_visible(widget) (GTK_WIDGET_FLAGS(widget) & GTK_VISIBLE)

#define gtk_widget_set_window(widget, new_window) (widget)->window = (new_window)
#define gtk_widget_set_can_focus(widget, TRUE) GTK_WIDGET_SET_FLAGS((widget), GTK_CAN_FOCUS)
#define gtk_widget_get_allocation(widget, alloc) (*(alloc) = (widget)->allocation)
#define gtk_widget_set_allocation(widget, alloc) ((widget)->allocation = *(alloc))
#endif // GTK_CHECK_VERSION(2, 18, 0)

#if !GTK_CHECK_VERSION(2, 17, 3)
void gdk_window_get_root_coords(GdkWindow* window, gint x, gint y, gint* rootX, gint* rootY);
#endif // GTK_CHECK_VERSION(2, 17, 3)

#if !GTK_CHECK_VERSION(2, 16, 0)
const gchar* gtk_menu_item_get_label(GtkMenuItem*);
#endif // GTK_CHECK_VERSION(2, 16, 0)


#if !GTK_CHECK_VERSION(2, 14, 0)
#define gtk_widget_get_window(widget) (widget)->window
#define gtk_adjustment_get_value(adj) (adj)->value
#define gtk_dialog_get_content_area(dialog) (dialog)->vbox
#define gtk_dialog_get_action_area(dialog) (dialog)->action_area
#define gtk_selection_data_get_length(data) (data)->length
#define gtk_selection_data_get_data(data) (data)->data
#define gtk_selection_data_get_target(data) (data)->target
#define gtk_adjustment_set_page_size(adj, newValue) ((adj)->page_size = newValue)
#define gtk_adjustment_set_value(adj, newValue) ((adj)->value = newValue)
#define gtk_adjustment_set_lower(adj, newValue) ((adj)->lower = newValue)
#define gtk_adjustment_set_upper(adj, newValue) ((adj)->upper = newValue)

void gtk_adjustment_configure(GtkAdjustment* adjustment, gdouble value, gdouble lower, gdouble upper,
                              gdouble stepIncrement, gdouble pageIncrement, gdouble pageSize);

void gtk_adjustment_set_value(GtkAdjustment* adjusment, gdouble value);
#endif // GTK_CHECK_VERSION(2, 14, 0)

GdkDevice* getDefaultGDKPointerDevice(GdkWindow* window);
GdkCursor* blankCursor();

#if !GLIB_CHECK_VERSION(2, 27, 1)
gboolean g_signal_accumulator_first_wins(GSignalInvocationHint* invocationHint, GValue* returnAccumulator, const GValue* handlerReturn, gpointer data);
#endif

G_END_DECLS

#endif // GtkVersioning_h
