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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebExtensionUtilities.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include <gtk/gtk.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>

namespace TestWebKitAPI {
namespace Util {

GRefPtr<GBytes> makePNGData(int width, int height, int color)
{
    GRefPtr<GdkPixbuf> pixbuf = adoptGRef(gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height));

    if (!pixbuf)
        return { };

    gdk_pixbuf_fill(pixbuf.get(), color);

    GUniqueOutPtr<GError> error;
    gchar* buffer;
    gsize bufferSize;
    if (!gdk_pixbuf_save_to_buffer(pixbuf.get(), &buffer, &bufferSize, "png", &error.outPtr(), nullptr))
        return { };

    return adoptGRef(g_bytes_new_take(buffer, bufferSize));
}

} // namespace Util
} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
