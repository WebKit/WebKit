/*
 * Copyright (C) 2026 Igalia S.L
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
#include "GStreamerQuirkQualcomm.h"

#if USE(GSTREAMER)

#include "GStreamerCommon.h"

namespace WebCore {

GST_DEBUG_CATEGORY_STATIC(webkit_qualcomm_quirks_debug);
#define GST_CAT_DEFAULT webkit_qualcomm_quirks_debug

GStreamerQuirkQualcomm::GStreamerQuirkQualcomm()
{
    GST_DEBUG_CATEGORY_INIT(webkit_qualcomm_quirks_debug, "webkitquirksqualcomm", 0, "WebKit Qualcomm Quirks");
}

bool GStreamerQuirkQualcomm::isPlatformSupported() const
{
    auto factory = adoptGRef(gst_element_factory_find("qtic2vdec"));
    if (!factory)
        return false;

    // Make sure decodebin will not auto-plug any of these elements. The Qualcomm decoder already
    // has primary rank.
    static const std::array<ASCIILiteral, 5> s_disabledDecoders {
        "avdec_h264"_s,
        "avdec_h265"_s,
        "v4l2h264dec"_s,
        "v4l2h265dec"_s,
        "v4l2vp9dec"_s,
    };
    for (const auto& factoryName : s_disabledDecoders) {
        if (auto factory = adoptGRef(gst_element_factory_find(factoryName.characters())))
            gst_plugin_feature_set_rank(GST_PLUGIN_FEATURE_CAST(factory.get()), GST_RANK_NONE);
    }

    // Non-standard caps format of the video decoder src pad. glupload and glcolorconvert don't
    // accept this, so we need to avoid them in our GL sink.
    m_glCaps = adoptGRef(gst_caps_from_string("video/x-raw(memory:GBM), format=(string){NV12, NV12_10LE32, P010_10LE}"));

    GST_DEBUG("Qualcomm quirk configured and enabled");
    return true;
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER)
