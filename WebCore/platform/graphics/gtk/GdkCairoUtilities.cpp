/*
 * Copyright (C) 2010 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GdkCairoUtilities.h"

#include <cairo.h>
#include <gtk/gtk.h>

static inline unsigned char* getCairoSurfacePixel(unsigned char* data, unsigned x, unsigned y, unsigned rowStride)
{
    return data + (y * rowStride) + x * 4;
}

static inline guchar* getGdkPixbufPixel(guchar* data, unsigned x, unsigned y, unsigned rowStride)
{
    return data + (y * rowStride) + x * 4;
}

GdkPixbuf* cairoImageSurfaceToGdkPixbuf(cairo_surface_t* surface)
{
    int width = cairo_image_surface_get_width(surface);
    int height = cairo_image_surface_get_height(surface);
    unsigned char* surfaceData = cairo_image_surface_get_data(surface);
    int surfaceRowStride = cairo_image_surface_get_stride(surface);

    GdkPixbuf* dest = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, width, height);
    if (!dest)
        return 0;

    guchar* pixbufData = gdk_pixbuf_get_pixels(dest);
    int pixbufRowStride = gdk_pixbuf_get_rowstride(dest);

    /* From: http://cairographics.org/manual/cairo-image-surface.html#cairo-format-t
     * "CAIRO_FORMAT_ARGB32: each pixel is a 32-bit quantity, with alpha in
     * the upper 8 bits, then red, then green, then blue. The 32-bit
     * quantities are stored native-endian. Pre-multiplied alpha is used.
     * (That is, 50% transparent red is 0x80800000, not 0x80ff0000.)"
     *
     * See http://developer.gimp.org/api/2.0/gdk-pixbuf/gdk-pixbuf-gdk-pixbuf.html#GdkPixbuf
     * for information on the structure of GdkPixbufs stored with GDK_COLORSPACE_RGB.
     *
     * RGB color channels in CAIRO_FORMAT_ARGB32 are stored based on the
     * endianness of the machine and are also multiplied by the alpha channel.
     * To properly transfer the data from the Cairo surface we must divide each
     * of the RGB channels by the alpha channel and then reorder all channels
     * if this machine is little-endian.
     */
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned char* source = getCairoSurfacePixel(surfaceData, x, y, surfaceRowStride);
            guchar* dest = getGdkPixbufPixel(pixbufData, x, y, pixbufRowStride);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
            guchar alpha = source[3];
            dest[0] = alpha ? ((source[2] * 255) / alpha) : 0;
            dest[1] = alpha ? ((source[1] * 255) / alpha) : 0;
            dest[2] = alpha ? ((source[0] * 255) / alpha) : 0;
            dest[3] = alpha;
#else
            guchar alpha = source[0];
            dest[0] = alpha ? ((source[1] * 255) / alpha) : 0;
            dest[1] = alpha ? ((source[2] * 255) / alpha) : 0;
            dest[2] = alpha ? ((source[3] * 255) / alpha) : 0;
            dest[3] = alpha;
#endif
        }
    }

    return dest;
}

