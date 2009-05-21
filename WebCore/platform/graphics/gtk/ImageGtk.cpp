/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "BitmapImage.h"
#include "CString.h"
#include "GOwnPtr.h"

#include <cairo.h>
#include <gtk/gtk.h>

namespace WTF {

template <> void freeOwnedGPtr<GtkIconInfo>(GtkIconInfo* info)
{
    if (info)
        gtk_icon_info_free(info);
}

}

namespace WebCore {

static CString getIconFileNameOrFallback(const char* name, const char* fallback)
{
    GOwnPtr<GtkIconInfo> info(gtk_icon_theme_lookup_icon(gtk_icon_theme_get_default(),
                                                         name, 16, GTK_ICON_LOOKUP_NO_SVG));
    if (!info)
        return String::format("%s/webkit-1.0/images/%s.png", DATA_DIR, fallback).utf8();

    return CString(gtk_icon_info_get_filename(info.get()));
}

static PassRefPtr<SharedBuffer> loadResourceSharedBuffer(const char* name)
{
    CString fileName;

    // Find the path for the image
    if (strcmp("missingImage", name) == 0)
        fileName = getIconFileNameOrFallback(GTK_STOCK_MISSING_IMAGE, "missingImage");
    else
        fileName = String::format("%s/webkit-1.0/images/%s.png", DATA_DIR, name).utf8();

    GOwnPtr<gchar> content;
    gsize length;
    if (!g_file_get_contents(fileName.data(), &content.outPtr(), &length, 0))
        return SharedBuffer::create();

    return SharedBuffer::create(content.get(), length);
}

void BitmapImage::initPlatformData()
{
}

void BitmapImage::invalidatePlatformData()
{
}

PassRefPtr<Image> Image::loadPlatformResource(const char* name)
{
    RefPtr<BitmapImage> img = BitmapImage::create();
    RefPtr<SharedBuffer> buffer = loadResourceSharedBuffer(name);
    img->setData(buffer.release(), true);
    return img.release();
}

GdkPixbuf* BitmapImage::getGdkPixbuf()
{
    int width = cairo_image_surface_get_width(frameAtIndex(currentFrame()));
    int height = cairo_image_surface_get_height(frameAtIndex(currentFrame()));

    int bestDepth = gdk_visual_get_best_depth();
    GdkColormap* cmap = gdk_colormap_new(gdk_visual_get_best_with_depth(bestDepth), true);

    GdkPixmap* pixmap = gdk_pixmap_new(0, width, height, bestDepth);
    gdk_drawable_set_colormap(GDK_DRAWABLE(pixmap), cmap);
    cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(pixmap));
    cairo_set_source_surface(cr, frameAtIndex(currentFrame()), 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);

    GdkPixbuf* pixbuf = gdk_pixbuf_get_from_drawable(0, GDK_DRAWABLE(pixmap), 0, 0, 0, 0, 0, width, height);
    g_object_unref(pixmap);
    g_object_unref(cmap);

    return pixbuf;
}

}
