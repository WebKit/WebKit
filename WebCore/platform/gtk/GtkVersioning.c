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

#include "GtkVersioning.h"

#include <gtk/gtk.h>

#if !GTK_CHECK_VERSION(2, 14, 0)
void gtk_adjustment_set_value(GtkAdjustment* adjusment, gdouble value)
{
        m_adjustment->value = m_currentPos;
        gtk_adjustment_value_changed(m_adjustment);
}

void gtk_adjustment_configure(GtkAdjustment* adjustment, gdouble value, gdouble lower, gdouble upper,
                              gdouble stepIncrement, gdouble pageIncrement, gdouble pageSize)
{
    g_object_freeze_notify(G_OBJECT(adjustment));

    g_object_set(adjustment,
                 "lower", lower,
                 "upper", upper,
                 "step-increment", stepIncrement,
                 "page-increment", pageIncrement,
                 "page-size", pageSize,
                 NULL);

    g_object_thaw_notify(G_OBJECT(adjustment));

    gtk_adjustment_changed(adjustment);
    gtk_adjustment_value_changed(adjustment);
}
#endif

GdkDevice *getDefaultGDKPointerDevice(GdkWindow* window)
{
#ifndef GTK_API_VERSION_2
    GdkDeviceManager *manager =  gdk_display_get_device_manager(gdk_drawable_get_display(window));
    return gdk_device_manager_get_client_pointer(manager);
#else
    return gdk_device_get_core_pointer();
#endif // GTK_API_VERSION_2
}

#if !GTK_CHECK_VERSION(2, 17, 3)
void gdk_window_get_root_coords(GdkWindow* window, gint x, gint y, gint* rootX, gint* rootY)
{
    gdk_window_get_root_origin(window, rootX, rootY);
    *rootX = *rootX + x;
    *rootY = *rootY + y;
}
#endif

GdkCursor * blankCursor()
{
#if GTK_CHECK_VERSION(2, 16, 0)
    return gdk_cursor_new(GDK_BLANK_CURSOR);
#else
    GdkCursor * cursor;
    GdkPixmap * source;
    GdkPixmap * mask;
    GdkColor foreground = { 0, 65535, 0, 0 }; // Red.
    GdkColor background = { 0, 0, 0, 65535 }; // Blue.
    static gchar cursorBits[] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

    source = gdk_bitmap_create_from_data(0, cursorBits, 8, 8);
    mask = gdk_bitmap_create_from_data(0, cursorBits, 8, 8);
    cursor = gdk_cursor_new_from_pixmap(source, mask, &foreground, &background, 8, 8);
    gdk_pixmap_unref(source);
    gdk_pixmap_unref(mask);
    return cursor;
#endif // GTK_CHECK_VERSION(2, 16, 0)
}

#if !GTK_CHECK_VERSION(2, 16, 0)
const gchar* gtk_menu_item_get_label(GtkMenuItem* menuItem)
{
    GtkWidget * label = gtk_bin_get_child(GTK_BIN(menuItem));
    if (GTK_IS_LABEL(label))
        return gtk_label_get_text(GTK_LABEL(label));
    return 0;
}
#endif // GTK_CHECK_VERSION(2, 16, 0)
