/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Holger Hans Peter Freyther <zecke@selfish.org>
 * Copyright (C) 2008, 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 * Copyright (C) 2010 Igalia S.L.
 * Copyright (C) 2020 Apple Inc.  All rights reserved.
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
#include "ImageBufferUtilitiesCairo.h"

#if USE(CAIRO)

#include "CairoUtilities.h"
#include "MIMETypeRegistry.h"
#include "RefPtrCairo.h"
#include <cairo.h>
#include <wtf/text/Base64.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(GTK)
#include "GRefPtrGtk.h"
#include "GdkCairoUtilities.h"
#include <gtk/gtk.h>
#include <wtf/glib/GUniquePtr.h>
#endif

namespace WebCore {

#if !PLATFORM(GTK)
static cairo_status_t writeFunction(void* output, const unsigned char* data, unsigned length)
{
    if (!reinterpret_cast<Vector<uint8_t>*>(output)->tryAppend(data, length))
        return CAIRO_STATUS_WRITE_ERROR;
    return CAIRO_STATUS_SUCCESS;
}

static bool encodeImage(cairo_surface_t* image, const String& mimeType, Vector<uint8_t>* output)
{
    ASSERT_UNUSED(mimeType, mimeType == "image/png"); // Only PNG output is supported for now.

    return cairo_surface_write_to_png_stream(image, writeFunction, output) == CAIRO_STATUS_SUCCESS;
}

Vector<uint8_t> data(cairo_surface_t* image, const String& mimeType, Optional<double>)
{
    Vector<uint8_t> encodedImage;
    if (!image || !encodeImage(image, mimeType, &encodedImage))
        return { };
    return encodedImage;
}
#else
static bool encodeImage(cairo_surface_t* surface, const String& mimeType, Optional<double> quality, GUniqueOutPtr<gchar>& buffer, gsize& bufferSize)
{
    // List of supported image encoding types comes from the GdkPixbuf documentation.
    // http://developer.gnome.org/gdk-pixbuf/stable/gdk-pixbuf-File-saving.html#gdk-pixbuf-save-to-bufferv

    String type = mimeType.substring(sizeof "image");
    if (type != "jpeg" && type != "png" && type != "tiff" && type != "ico" && type != "bmp")
        return false;

    GRefPtr<GdkPixbuf> pixbuf;
    if (type == "jpeg") {
        // JPEG doesn't support alpha channel. The <canvas> spec states that toDataURL() must encode a Porter-Duff
        // composite source-over black for image types that do not support alpha.
        RefPtr<cairo_surface_t> newSurface;
        if (cairo_surface_get_type(surface) == CAIRO_SURFACE_TYPE_IMAGE) {
            newSurface = adoptRef(cairo_image_surface_create_for_data(cairo_image_surface_get_data(surface),
                CAIRO_FORMAT_RGB24,
                cairo_image_surface_get_width(surface),
                cairo_image_surface_get_height(surface),
                cairo_image_surface_get_stride(surface)));
        } else {
            IntSize size = cairoSurfaceSize(surface);
            newSurface = adoptRef(cairo_image_surface_create(CAIRO_FORMAT_RGB24, size.width(), size.height()));
            RefPtr<cairo_t> cr = adoptRef(cairo_create(newSurface.get()));
            cairo_set_source_surface(cr.get(), surface, 0, 0);
            cairo_paint(cr.get());
        }
        pixbuf = adoptGRef(cairoSurfaceToGdkPixbuf(newSurface.get()));
    } else
        pixbuf = adoptGRef(cairoSurfaceToGdkPixbuf(surface));
    if (!pixbuf)
        return false;

    GUniqueOutPtr<GError> error;
    if (type == "jpeg" && quality && *quality >= 0.0 && *quality <= 1.0) {
        String qualityString = String::number(static_cast<int>(*quality * 100.0 + 0.5));
        gdk_pixbuf_save_to_buffer(pixbuf.get(), &buffer.outPtr(), &bufferSize, type.utf8().data(), &error.outPtr(), "quality", qualityString.utf8().data(), NULL);
    } else
        gdk_pixbuf_save_to_buffer(pixbuf.get(), &buffer.outPtr(), &bufferSize, type.utf8().data(), &error.outPtr(), NULL);

    return !error;
}

Vector<uint8_t> data(cairo_surface_t* image, const String& mimeType, Optional<double> quality)
{
    GUniqueOutPtr<gchar> buffer;
    gsize bufferSize;
    if (!encodeImage(image, mimeType, quality, buffer, bufferSize))
        return { };

    Vector<uint8_t> imageData;
    imageData.append(buffer.get(), bufferSize);
    return imageData;
}
#endif // !PLATFORM(GTK)

} // namespace WebCore

#endif // USE(CAIRO)
