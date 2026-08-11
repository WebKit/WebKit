/*
 * Copyright (C) 2025 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebExtension.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "Logging.h"
#include <WebCore/LocalizedStrings.h>
#include <gtk/gtk.h>
#include <wtf/glib/GRefPtr.h>

namespace WebKit {

Expected<Ref<WebCore::Icon>, RefPtr<API::Error>> WebExtension::iconForPath(const String& path, WebCore::FloatSize sizeForResizing, std::optional<double> idealDisplayScale)
{
    ASSERT(path);

    auto dataResult = resourceDataForPath(path);
    if (!dataResult)
        return makeUnexpected(dataResult.error());

    Ref imageData = dataResult.value();
    auto gimageBytes = adoptGRef(g_bytes_new(imageData->span().data(), imageData->size()));

    if (!sizeForResizing.isZero()) {
        GUniqueOutPtr<GError> error;

        auto loader = adoptGRef(gdk_pixbuf_loader_new());
        gdk_pixbuf_loader_write_bytes(loader.get(), gimageBytes.get(), &error.outPtr());
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Unknown error when loading an icon: %s", error.get()->message);
            return makeUnexpected(createError(Error::Unknown));
        }
        if (!gdk_pixbuf_loader_close(loader.get(), &error.outPtr()) && error) {
            RELEASE_LOG_ERROR(Extensions, "Unknown error when loading an icon: %s", error.get()->message);
            return makeUnexpected(createError(Error::Unknown));
        }
        auto pixbuf = adoptGRef(gdk_pixbuf_copy(gdk_pixbuf_loader_get_pixbuf(loader.get())));
        if (!pixbuf)
            return makeUnexpected(nullptr);

        // Proportionally scale the size
        auto originalWidth = gdk_pixbuf_get_width(pixbuf.get());
        auto originalHeight = gdk_pixbuf_get_height(pixbuf.get());
        auto aspectWidth = originalWidth ? (sizeForResizing.width() / originalWidth) : 0;
        auto aspectHeight = originalHeight ? (sizeForResizing.height() / originalHeight) : 0;
        auto aspectRatio = std::min(aspectWidth, aspectHeight);

        gdk_pixbuf_scale_simple(pixbuf.get(), originalWidth * aspectRatio, originalHeight * aspectRatio, GDK_INTERP_BILINEAR);

        gchar* buffer;
        gsize bufferSize;
        if (!gdk_pixbuf_save_to_buffer(pixbuf.get(), &buffer, &bufferSize, "png", &error.outPtr(), nullptr) && error) {
            RELEASE_LOG_ERROR(Extensions, "Unknown error when loading an icon: %s", error.get()->message);
            return makeUnexpected(createError(Error::Unknown));
        }

        gimageBytes = adoptGRef(g_bytes_new_take(buffer, bufferSize));
    }

    GRefPtr<GIcon> image = adoptGRef(g_bytes_icon_new(gimageBytes.get()));

    if (RefPtr iconResult = WebCore::Icon::create(WTF::move(image)))
        return iconResult.releaseNonNull();
    return makeUnexpected(nullptr);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
