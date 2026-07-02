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
#include "WebExtensionLocalization.h"
#include "WebExtensionUtilities.h"
#include <WebCore/DataURLDecoder.h>
#include <WebCore/LocalizedStrings.h>
#include <WebCore/platform/graphics/Icon.h>
#include <wtf/FileSystem.h>
#include <wtf/Language.h>
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

    // FIXME: We don't have a way to scale the image on WPE, as that would require being able to
    // have some sort of pixel buffer that allows for  scaling an image
    GRefPtr<GIcon> image = adoptGRef(g_bytes_icon_new(gimageBytes.get()));

    if (RefPtr iconResult = WebCore::Icon::create(WTF::move(image)))
        return iconResult.releaseNonNull();
    return makeUnexpected(nullptr);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
