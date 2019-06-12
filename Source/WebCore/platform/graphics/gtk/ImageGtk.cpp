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
#include "GUniquePtrGtk.h"
#include "GdkCairoUtilities.h"
#include "SharedBuffer.h"
#include <cairo.h>
#include <gtk/gtk.h>
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

static Ref<SharedBuffer> loadResourceSharedBuffer(const char* filename)
{
    GUniqueOutPtr<gchar> content;
    gsize length;
    if (!g_file_get_contents(filename, &content.outPtr(), &length, nullptr))
        return SharedBuffer::create();

    return SharedBuffer::create(content.get(), length);
}

void BitmapImage::invalidatePlatformData()
{
}

static Ref<Image> loadMissingImageIconFromTheme(const char* name)
{
    int iconSize = g_str_has_suffix(name, "@2x") ? 32 : 16;
    auto icon = BitmapImage::create();
    GUniquePtr<GtkIconInfo> iconInfo(gtk_icon_theme_lookup_icon(gtk_icon_theme_get_default(), "image-missing", iconSize, GTK_ICON_LOOKUP_NO_SVG));
    if (iconInfo) {
        auto buffer = loadResourceSharedBuffer(gtk_icon_info_get_filename(iconInfo.get()));
        icon->setData(WTFMove(buffer), true);
        return icon;
    }

    return loadImageFromGResource(name);
}

Ref<Image> Image::loadPlatformResource(const char* name)
{
    return g_str_has_prefix(name, "missingImage") ? loadMissingImageIconFromTheme(name) : loadImageFromGResource(name);
}

GdkPixbuf* BitmapImage::getGdkPixbuf()
{
    RefPtr<cairo_surface_t> surface = nativeImageForCurrentFrame();
    return surface ? cairoSurfaceToGdkPixbuf(surface.get()) : 0;
}

}
