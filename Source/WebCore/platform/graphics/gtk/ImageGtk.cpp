/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
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

#include "BitmapImage.h"
#include "GdkCairoUtilities.h"
#include "SharedBuffer.h"
#include <cairo.h>
#include <gdk/gdk.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

static Ref<Image> loadImageFromGResource(const char* iconName)
{
    auto icon = BitmapImage::create();
    GUniquePtr<char> path(g_strdup_printf("/org/webkitgtk/resources/images/%s", iconName));
    GRefPtr<GBytes> data = adoptGRef(g_resources_lookup_data(path.get(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr));
    ASSERT(data);
    icon->setData(SharedBuffer::create(static_cast<const unsigned char*>(g_bytes_get_data(data.get(), nullptr)), g_bytes_get_size(data.get())), true);
    return icon;
}

void BitmapImage::invalidatePlatformData()
{
}

Ref<Image> Image::loadPlatformResource(const char* name)
{
    return loadImageFromGResource(name);
}

GdkPixbuf* BitmapImage::getGdkPixbuf()
{
    if (auto nativeImage = nativeImageForCurrentFrame()) {
        auto& surface = nativeImage->platformImage();
        return cairoSurfaceToGdkPixbuf(surface.get());
    }
    return nullptr;
}

#if USE(GTK4)
GdkTexture* BitmapImage::gdkTexture()
{
    auto nativeImage = nativeImageForCurrentFrame();
    if (!nativeImage)
        return nullptr;

    auto& surface = nativeImage->platformImage();

    ASSERT(cairo_image_surface_get_format(surface.get()) == CAIRO_FORMAT_ARGB32);
    auto width = cairo_image_surface_get_width(surface.get());
    auto height = cairo_image_surface_get_height(surface.get());
    auto stride = cairo_image_surface_get_stride(surface.get());
    auto* data = cairo_image_surface_get_data(surface.get());
    GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new_with_free_func(data, height * stride, [](gpointer data) {
        cairo_surface_destroy(static_cast<cairo_surface_t*>(data));
    }, const_cast<PlatformImagePtr&>(surface).leakRef()));
    return gdk_memory_texture_new(width, height, GDK_MEMORY_DEFAULT, bytes.get(), stride);
}
#endif

}
