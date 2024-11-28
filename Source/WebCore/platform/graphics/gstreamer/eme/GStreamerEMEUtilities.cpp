/*
 * Copyright (C) 2021 Igalia S.L
 * Copyright (C) 2021 Metrological Group B.V.
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
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "GStreamerEMEUtilities.h"

#include <wtf/StdLibExtras.h>
#include <wtf/text/Base64.h>

#if ENABLE(ENCRYPTED_MEDIA) && USE(GSTREAMER)

GST_DEBUG_CATEGORY_EXTERN(webkit_media_common_encryption_decrypt_debug_category);
#define GST_CAT_DEFAULT webkit_media_common_encryption_decrypt_debug_category

namespace WebCore {

struct GMarkupParseContextUserData {
    bool isParsingPssh { false };
    RefPtr<SharedBuffer> pssh;
};

static void markupStartElement(GMarkupParseContext*, const gchar* elementName, const gchar**, const gchar**, gpointer userDataPtr, GError**)
{
    GMarkupParseContextUserData* userData = static_cast<GMarkupParseContextUserData*>(userDataPtr);
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GLib port
    if (g_str_has_suffix(elementName, "pssh"))
        userData->isParsingPssh = true;
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

static void markupEndElement(GMarkupParseContext*, const gchar* elementName, gpointer userDataPtr, GError**)
{
    GMarkupParseContextUserData* userData = static_cast<GMarkupParseContextUserData*>(userDataPtr);
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GLib port
    if (g_str_has_suffix(elementName, "pssh")) {
        ASSERT(userData->isParsingPssh);
        userData->isParsingPssh = false;
    }
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

static void markupText(GMarkupParseContext*, const gchar* text, gsize textLength, gpointer userDataPtr, GError**)
{
    GMarkupParseContextUserData* userData = static_cast<GMarkupParseContextUserData*>(userDataPtr);
    if (userData->isParsingPssh) {
        auto pssh = base64Decode({ reinterpret_cast<const uint8_t*>(text), textLength });
        if (pssh.has_value())
            userData->pssh = SharedBuffer::create(WTFMove(*pssh));
    }
}

static void markupPassthrough(GMarkupParseContext*, const gchar*, gsize, gpointer, GError**)
{
}

static void markupError(GMarkupParseContext*, GError*, gpointer)
{
}

static GMarkupParser markupParser { markupStartElement, markupEndElement, markupText, markupPassthrough, markupError };

RefPtr<SharedBuffer> InitData::extractCencIfNeeded(RefPtr<SharedBuffer>&& unparsedPayload)
{
    RefPtr<SharedBuffer> payload = WTFMove(unparsedPayload);
    if (!payload || !payload->size())
        return payload;

    GMarkupParseContextUserData userData;
    GUniquePtr<GMarkupParseContext> markupParseContext(g_markup_parse_context_new(&markupParser, (GMarkupParseFlags) 0, &userData, nullptr));

    auto payloadData = spanReinterpretCast<const char>(payload->span());
    if (g_markup_parse_context_parse(markupParseContext.get(), payloadData.data(), payloadData.size(), nullptr)) {
        if (userData.pssh)
            payload = WTFMove(userData.pssh);
        else
            GST_WARNING("XML was parsed but we could not find a viable base64 encoded pssh box");
    }

    return payload;
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA) && USE(GSTREAMER)
