/*
 *  Copyright (C) 2024 Igalia S.L.
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

#include "GStreamerWebRTCCommon.h"
#include "GStreamerCommon.h"

GST_DEBUG_CATEGORY(webkit_webrtc_common_debug);
#define GST_CAT_DEFAULT webkit_webrtc_common_debug

namespace WebCore {

static void ensureWebRTCCommonDebugCategoryInitialized()
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_common_debug, "webkitwebrtccommon", 0, "WebKit WebRTC Common");
    });
}

void gstPayloaderSetPayloadType(const GRefPtr<GstElement>& payloader, int pt)
{
    ensureWebRTCCommonDebugCategoryInitialized();
    auto ptSpec = g_object_class_find_property(G_OBJECT_GET_CLASS(G_OBJECT(payloader.get())), "pt");

    GValue value = G_VALUE_INIT;
    g_object_get_property(G_OBJECT(payloader.get()), "pt", &value);

    if (G_VALUE_TYPE(&value) != G_TYPE_INT && G_VALUE_TYPE(&value) != G_TYPE_UINT) {
        GST_ERROR_OBJECT(payloader.get(), "pt property is not integer or unsigned");
        return;
    }

    if (G_VALUE_TYPE(&value) == G_TYPE_INT) {
        auto intSpec = G_PARAM_SPEC_INT(ptSpec);
        if (pt > intSpec->maximum || pt < intSpec->minimum) {
            GST_ERROR_OBJECT(payloader.get(), "pt %d outside of valid range [%d, %d]", pt, intSpec->minimum, intSpec->maximum);
            return;
        }
        g_object_set(payloader.get(), "pt", pt, nullptr);
        return;
    }

    if (G_VALUE_TYPE(&value) == G_TYPE_UINT) {
        auto uintSpec = G_PARAM_SPEC_UINT(ptSpec);
        unsigned ptValue = static_cast<unsigned>(pt);
        if (ptValue > uintSpec->maximum || ptValue < uintSpec->minimum) {
            GST_ERROR_OBJECT(payloader.get(), "pt %u outside of valid range [%u, %u]", ptValue, uintSpec->minimum, uintSpec->maximum);
            return;
        }
        g_object_set(payloader.get(), "pt", ptValue, nullptr);
    }
}

} // namespace WebCore

#undef GST_CAT_DEFAULT
