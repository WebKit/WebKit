/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "GdkSkiaUtilities.h"

#if USE(SKIA)

#if !USE(GTK4)
#include <graphics/cairo/RefPtrCairo.h>
#endif

IGNORE_CLANG_WARNINGS_BEGIN("cast-align")
#include <skia/core/SkPixmap.h>
IGNORE_CLANG_WARNINGS_END

namespace WebCore {

#if USE(GTK4)
GRefPtr<GdkTexture> skiaImageToGdkTexture(SkImage& image)
{
    SkPixmap pixmap;
    if (!image.peekPixels(&pixmap))
        return { };

    GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new_with_free_func(pixmap.addr(), pixmap.computeByteSize(), [](gpointer data) {
        static_cast<SkImage*>(data)->unref();
    }, SkRef(&image)));

    return adoptGRef(gdk_memory_texture_new(pixmap.width(), pixmap.height(), GDK_MEMORY_DEFAULT, bytes.get(), pixmap.rowBytes()));
}

#else

RefPtr<cairo_surface_t> skiaImageToCairoSurface(SkImage& image)
{
    SkPixmap pixmap;
    if (!image.peekPixels(&pixmap))
        return { };

    RefPtr<cairo_surface_t> surface = adoptRef(cairo_image_surface_create_for_data(pixmap.writable_addr8(0, 0), CAIRO_FORMAT_ARGB32, pixmap.width(), pixmap.height(), pixmap.rowBytes()));
    if (cairo_surface_status(surface.get()) != CAIRO_STATUS_SUCCESS)
        return { };

    static cairo_user_data_key_t surfaceDataKey;
    cairo_surface_set_user_data(surface.get(), &surfaceDataKey, SkRef(&image), [](void* data) {
        static_cast<SkImage*>(data)->unref();
    });

    return surface;
}
#endif

GRefPtr<GdkPixbuf> skiaImageToGdkPixbuf(SkImage& image)
{
#if USE(GTK4)
    auto texture = skiaImageToGdkTexture(image);
    if (!texture)
        return { };

    return adoptGRef(gdk_pixbuf_get_from_texture(texture.get()));
#else
    RefPtr surface = skiaImageToCairoSurface(image);
    if (!surface)
        return { };

    return adoptGRef(gdk_pixbuf_get_from_surface(surface.get(), 0, 0, cairo_image_surface_get_width(surface.get()), cairo_image_surface_get_height(surface.get())));
#endif
}

} // namespace WebCore

#endif // #if USE(SKIA)
