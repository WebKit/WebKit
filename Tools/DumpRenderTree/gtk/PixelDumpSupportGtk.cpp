/*
 * Copyright (C) 2009 Zan Dobersek <zandobersek@gmail.com>
 * Copyright (C) 2010 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "DumpRenderTree.h"
#include "PixelDumpSupportCairo.h"
#include <webkit/webkit.h>

PassRefPtr<BitmapContext> createBitmapContextFromWebView(bool, bool, bool, bool)
{
    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    gint width, height;
#ifdef GTK_API_VERSION_2
    GdkPixmap* pixmap = gtk_widget_get_snapshot(GTK_WIDGET(view), 0);
    gdk_drawable_get_size(GDK_DRAWABLE(pixmap), &width, &height);
#else
    width = gtk_widget_get_allocated_width(GTK_WIDGET(view));
    height = gtk_widget_get_allocated_height(GTK_WIDGET(view));
#endif

    cairo_surface_t* imageSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t* context = cairo_create(imageSurface);
#ifdef GTK_API_VERSION_2
    gdk_cairo_set_source_pixmap(context, pixmap, 0, 0);
    cairo_paint(context);
    g_object_unref(pixmap);
#else
    gtk_widget_draw(GTK_WIDGET(view), context);
#endif

    return BitmapContext::createByAdoptingBitmapAndContext(0, context);
}
