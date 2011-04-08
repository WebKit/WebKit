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
    GdkDeviceManager *manager =  gdk_display_get_device_manager(gdk_window_get_display(window));
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

#ifdef GTK_API_VERSION_2
static cairo_format_t
gdk_cairo_format_for_content(cairo_content_t content)
{
    switch (content) {
    case CAIRO_CONTENT_COLOR:
        return CAIRO_FORMAT_RGB24;
    case CAIRO_CONTENT_ALPHA:
        return CAIRO_FORMAT_A8;
    case CAIRO_CONTENT_COLOR_ALPHA:
    default:
        return CAIRO_FORMAT_ARGB32;
    }
}

static cairo_surface_t*
gdk_cairo_surface_coerce_to_image(cairo_surface_t* surface,
                                  cairo_content_t content,
                                  int width,
                                  int height)
{
    cairo_surface_t * copy;
    cairo_t * cr;

    if (cairo_surface_get_type(surface) == CAIRO_SURFACE_TYPE_IMAGE
        && cairo_surface_get_content(surface) == content
        && cairo_image_surface_get_width(surface) >= width
        && cairo_image_surface_get_height(surface) >= height)
        return cairo_surface_reference(surface);

    copy = cairo_image_surface_create(gdk_cairo_format_for_content(content),
                                      width,
                                      height);

    cr = cairo_create(copy);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);

    return copy;
}

static void
convert_alpha(guchar * destData, int destStride,
              guchar * srcData, int srcStride,
              int srcX, int srcY, int width, int height)
{
    int x, y;

    srcData += srcStride * srcY + srcY * 4;

    for (y = 0; y < height; y++) {
        guint32 * src = (guint32 *) srcData;

        for (x = 0; x < width; x++) {
            guint alpha = src[x] >> 24;

            if (!alpha) {
                destData[x * 4 + 0] = 0;
                destData[x * 4 + 1] = 0;
                destData[x * 4 + 2] = 0;
            } else {
                destData[x * 4 + 0] = (((src[x] & 0xff0000) >> 16) * 255 + alpha / 2) / alpha;
                destData[x * 4 + 1] = (((src[x] & 0x00ff00) >>  8) * 255 + alpha / 2) / alpha;
                destData[x * 4 + 2] = (((src[x] & 0x0000ff) >>  0) * 255 + alpha / 2) / alpha;
            }
            destData[x * 4 + 3] = alpha;
        }

        srcData += srcStride;
        destData += destStride;
    }
}

static void
convert_no_alpha(guchar * destData, int destStride, guchar * srcData,
                 int srcStride, int srcX, int srcY,
                 int width, int height)
{
    int x, y;

    srcData += srcStride * srcY + srcX * 4;

    for (y = 0; y < height; y++) {
        guint32 * src = (guint32 *) srcData;

        for (x = 0; x < width; x++) {
            destData[x * 3 + 0] = src[x] >> 16;
            destData[x * 3 + 1] = src[x] >>  8;
            destData[x * 3 + 2] = src[x];
        }

        srcData += srcStride;
        destData += destStride;
    }
}

/**
 * gdk_pixbuf_get_from_surface:
 * @surface: surface to copy from
 * @src_x: Source X coordinate within @surface
 * @src_y: Source Y coordinate within @surface
 * @width: Width in pixels of region to get
 * @height: Height in pixels of region to get
 *
 * Transfers image data from a #cairo_surface_t and converts it to an RGB(A)
 * representation inside a #GdkPixbuf. This allows you to efficiently read
 * individual pixels from cairo surfaces. For #GdkWindows, use
 * gdk_pixbuf_get_from_window() instead.
 *
 * This function will create an RGB pixbuf with 8 bits per channel. The pixbuf
 * will contain an alpha channel if the @surface contains one.
 *
 * Return value: (transfer full): A newly-created pixbuf with a reference count
 * of 1, or %NULL on error
 **/
GdkPixbuf*
gdk_pixbuf_get_from_surface(cairo_surface_t * surface,
                            int srcX, int srcY,
                            int width, int height)
{
    cairo_content_t content;
    GdkPixbuf * dest;

    /* General sanity checks */
    g_return_val_if_fail(surface, NULL);
    g_return_val_if_fail(srcX >= 0 && srcY >= 0, NULL);
    g_return_val_if_fail(width > 0 && height > 0, NULL);

    content = cairo_surface_get_content(surface) | CAIRO_CONTENT_COLOR;
    dest = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                          !!(content & CAIRO_CONTENT_ALPHA),
                          8,
                          width, height);

    surface = gdk_cairo_surface_coerce_to_image(surface, content, srcX + width, srcY + height);
    cairo_surface_flush(surface);
    if (cairo_surface_status(surface) || !dest) {
        cairo_surface_destroy(surface);
        return NULL;
    }

    if (gdk_pixbuf_get_has_alpha(dest))
        convert_alpha(gdk_pixbuf_get_pixels(dest),
                       gdk_pixbuf_get_rowstride(dest),
                       cairo_image_surface_get_data(surface),
                       cairo_image_surface_get_stride(surface),
                       srcX, srcY,
                       width, height);
    else
        convert_no_alpha(gdk_pixbuf_get_pixels(dest),
                          gdk_pixbuf_get_rowstride(dest),
                          cairo_image_surface_get_data(surface),
                          cairo_image_surface_get_stride(surface),
                          srcX, srcY,
                          width, height);

    cairo_surface_destroy(surface);
    return dest;
}

#endif // GTK_API_VERSION_2

#if !GLIB_CHECK_VERSION(2, 27, 1)
gboolean g_signal_accumulator_first_wins(GSignalInvocationHint *invocationHint, GValue *returnAccumulator, const GValue *handlerReturn, gpointer data)
{
    g_value_copy(handlerReturn, returnAccumulator);
    return FALSE;
}
#endif

#if !GTK_CHECK_VERSION(2, 22, 0)
cairo_surface_t *gdk_window_create_similar_surface(GdkWindow *window, cairo_content_t content, int width, int height)
{
    cairo_t *cairoContext = gdk_cairo_create(window);
    cairo_surface_t *cairoSurface = cairo_get_target(cairoContext);
    cairo_surface_t *newSurface = cairo_surface_create_similar(cairoSurface, content, width, height);
    cairo_destroy(cairoContext);
    return newSurface;
}
#endif // GTK_CHECK_VERSION(2, 22, 0)
