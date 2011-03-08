/*
 *  Copyright (C) 2010 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include "config.h"
#include "ImageBuffer.h"

#include "Base64.h"
#include "GdkCairoUtilities.h"
#include "GOwnPtr.h"
#include "GRefPtrGtk.h"
#include "MIMETypeRegistry.h"
#include <cairo.h>
#include <gtk/gtk.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringConcatenate.h>

namespace WebCore {

String ImageBuffer::toDataURL(const String& mimeType, const double* quality) const
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    if (!mimeType.startsWith("image/"))
        return "data:,";

    // List of supported image types comes from the GdkPixbuf documentation.
    // http://library.gnome.org/devel/gdk-pixbuf/stable/gdk-pixbuf-file-saving.html#gdk-pixbuf-save-to-bufferv
    String type = mimeType.substring(sizeof "image");
    if (type != "jpeg" && type != "png" && type != "tiff" && type != "ico" && type != "bmp")
        return "data:,";

    GRefPtr<GdkPixbuf> pixbuf = cairoImageSurfaceToGdkPixbuf(m_data.m_surface);
    if (!pixbuf)
        return "data:,";

    GOwnPtr<gchar> buffer(0);
    gsize bufferSize;
    GError* error = 0;
    gboolean success = FALSE;
    if (type == "jpeg" && quality && *quality >= 0.0 && *quality <= 1.0) {
        String qualityString = String::format("%f", *quality * 100.0);
        success = gdk_pixbuf_save_to_buffer(pixbuf.get(), &buffer.outPtr(), &bufferSize,
            type.utf8().data(), &error, "quality", qualityString.utf8().data(), NULL);
    } else {
        success = gdk_pixbuf_save_to_buffer(pixbuf.get(), &buffer.outPtr(), &bufferSize, type.utf8().data(), &error, NULL);
    }

    if (!success)
        return "data:,";

    Vector<char> out;
    base64Encode(reinterpret_cast<const char*>(buffer.get()), bufferSize, out);

    return makeString("data:", mimeType, ";base64,", out);
}

}
