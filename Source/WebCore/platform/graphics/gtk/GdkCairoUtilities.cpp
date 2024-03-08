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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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

#if USE(CAIRO)

#include "CairoUtilities.h"
#include "IntSize.h"
#include <cairo.h>
#include <gtk/gtk.h>
#include <mutex>
#include <wtf/NeverDestroyed.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

GRefPtr<GdkPixbuf> cairoSurfaceToGdkPixbuf(cairo_surface_t* surface)
{
    IntSize size = cairoSurfaceSize(surface);
    return adoptGRef(gdk_pixbuf_get_from_surface(surface, 0, 0, size.width(), size.height()));
}

#if USE(GTK4)
GRefPtr<GdkTexture> cairoSurfaceToGdkTexture(cairo_surface_t* surface)
{
    ASSERT(cairo_image_surface_get_format(surface) == CAIRO_FORMAT_ARGB32);
    auto width = cairo_image_surface_get_width(surface);
    auto height = cairo_image_surface_get_height(surface);
    if (width <= 0 || height <= 0)
        return nullptr;
    auto stride = cairo_image_surface_get_stride(surface);
    auto* data = cairo_image_surface_get_data(surface);
    GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new_with_free_func(data, height * stride, [](gpointer data) {
        cairo_surface_destroy(static_cast<cairo_surface_t*>(data));
    }, cairo_surface_reference(surface)));
    return adoptGRef(gdk_memory_texture_new(width, height, GDK_MEMORY_DEFAULT, bytes.get(), stride));
}
#endif

} // namespace WebCore

#endif // #if USE(CAIRO)
