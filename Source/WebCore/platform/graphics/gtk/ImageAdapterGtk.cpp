/*
 * Copyright (C) 2006-2024 Apple Inc.  All rights reserved.
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
#include "ImageAdapter.h"

#include "BitmapImage.h"
#include "GdkCairoUtilities.h"
#include "GdkSkiaUtilities.h"
#include "SharedBuffer.h"
#include <cairo.h>
#include <gdk/gdk.h>
#include <wtf/glib/GSpanExtras.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

static Ref<Image> loadImageFromGResource(const char* iconName)
{
    auto icon = BitmapImage::create();
    GUniquePtr<char> path(g_strdup_printf("/org/webkitgtk/resources/images/%s", iconName));
    GRefPtr<GBytes> data = adoptGRef(g_resources_lookup_data(path.get(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr));
    ASSERT(data);
    icon->setData(SharedBuffer::create(span(data)), true);
    return icon;
}

Ref<Image> ImageAdapter::loadPlatformResource(const char* name)
{
    return loadImageFromGResource(name);
}

void ImageAdapter::invalidate()
{
}

GRefPtr<GdkPixbuf> ImageAdapter::gdkPixbuf()
{
    RefPtr nativeImage = image().currentNativeImage();
    if (!nativeImage)
        return nullptr;

    auto& surface = nativeImage->platformImage();
#if USE(CAIRO)
    return cairoSurfaceToGdkPixbuf(surface.get());
#elif USE(SKIA)
    return skiaImageToGdkPixbuf(*surface.get());
#endif
}

#if USE(GTK4)
GRefPtr<GdkTexture> ImageAdapter::gdkTexture()
{
    RefPtr nativeImage = image().currentNativeImage();
    if (!nativeImage)
        return nullptr;

    auto& surface = nativeImage->platformImage();
#if USE(CAIRO)
    return cairoSurfaceToGdkTexture(surface.get());
#elif USE(SKIA)
    return skiaImageToGdkTexture(*surface.get());
#endif
}
#endif

} // namespace WebCore
