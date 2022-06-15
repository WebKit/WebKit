/*
 *  Copyright (C) 2017-2022 Igalia S.L. All rights reserved.
 *  Copyright (C) 2022 Metrological Group B.V.
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
#include "RealtimeOutgoingVideoSourceGStreamer.h"

#if USE(GSTREAMER_WEBRTC)

#include "GStreamerVideoCaptureSource.h"

GST_DEBUG_CATEGORY_EXTERN(webkit_webrtc_endpoint_debug);
#define GST_CAT_DEFAULT webkit_webrtc_endpoint_debug

namespace WebCore {

RealtimeOutgoingVideoSourceGStreamer::RealtimeOutgoingVideoSourceGStreamer(Ref<MediaStreamTrackPrivate>&& source)
    : RealtimeOutgoingMediaSourceGStreamer()
{
    registerWebKitGStreamerElements();

    m_videoConvert = gst_element_factory_make("videoconvert", nullptr);

    m_videoFlip = gst_element_factory_make("videoflip", nullptr);
    gst_util_set_object_arg(G_OBJECT(m_videoFlip.get()), "method", "automatic");

    m_encoder = gst_element_factory_make("webrtcvideoencoder", nullptr);
    gst_bin_add_many(GST_BIN_CAST(m_bin.get()), m_videoFlip.get(), m_videoConvert.get(), m_encoder.get(), nullptr);

    auto* padTemplate = gst_element_get_pad_template(m_encoder.get(), "src");
    auto caps = adoptGRef(gst_pad_template_get_caps(padTemplate));
    m_allowedCaps = adoptGRef(gst_caps_new_empty());
    gst_caps_foreach(caps.get(), reinterpret_cast<GstCapsForeachFunc>(+[](GstCapsFeatures*, GstStructure* structure, gpointer userData) -> gboolean {
        auto* source = reinterpret_cast<RealtimeOutgoingVideoSourceGStreamer*>(userData);
        const char* name = gst_structure_get_name(structure);
        const char* encodingName = nullptr;
        if (!strcmp(name, "video/x-vp8"))
            encodingName = "VP8";
        else if (!strcmp(name, "video/x-vp9"))
            encodingName = "VP9";
        else if (!strcmp(name, "video/x-h264"))
            encodingName = "H264";
        if (encodingName)
            gst_caps_append_structure(source->m_allowedCaps.get(), gst_structure_new("application/x-rtp", "media", G_TYPE_STRING, "video", "encoding-name", G_TYPE_STRING, encodingName, "clock-rate", G_TYPE_INT, 90000, nullptr));
        return TRUE;
    }), this);

    setSource(WTFMove(source));
}

bool RealtimeOutgoingVideoSourceGStreamer::setPayloadType(const GRefPtr<GstCaps>& caps)
{
    GST_DEBUG_OBJECT(m_bin.get(), "Outgoing video source payload caps: %" GST_PTR_FORMAT, caps.get());
    auto* structure = gst_caps_get_structure(caps.get(), 0);
    GRefPtr<GstCaps> encoderCaps;
    if (const char* encodingName = gst_structure_get_string(structure, "encoding-name")) {
        if (!strcmp(encodingName, "VP8")) {
            encoderCaps = adoptGRef(gst_caps_new_empty_simple("video/x-vp8"));
            m_payloader = makeGStreamerElement("rtpvp8pay", nullptr);
        } else if (!strcmp(encodingName, "VP9")) {
            encoderCaps = adoptGRef(gst_caps_new_empty_simple("video/x-vp9"));
            m_payloader = makeGStreamerElement("rtpvp9pay", nullptr);
        } else if (!strcmp(encodingName, "H264")) {
            encoderCaps = adoptGRef(gst_caps_new_empty_simple("video/x-h264"));
            m_payloader = makeGStreamerElement("rtph264pay", nullptr);
            // FIXME: https://gitlab.freedesktop.org/gstreamer/gst-plugins-good/-/issues/893
            // gst_util_set_object_arg(G_OBJECT(m_payloader.get()), "aggregate-mode", "zero-latency");
            // g_object_set(m_payloader.get(), "config-interval", -1, nullptr);
        } else {
            GST_ERROR_OBJECT(m_bin.get(), "Unsupported outgoing video encoding: %s", encodingName);
            return false;
        }
    } else {
        GST_ERROR_OBJECT(m_bin.get(), "encoding-name not found");
        return false;
    }

    if (!m_payloader)
        return false;

    // FIXME: Re-enable this. Currently triggers caps negotiation error.
    g_object_set(m_payloader.get(), "auto-header-extension", FALSE, nullptr);

    g_object_set(m_encoder.get(), "format", encoderCaps.get(), nullptr);

    int payloadType;
    if (gst_structure_get_int(structure, "payload", &payloadType))
        g_object_set(m_payloader.get(), "pt", payloadType, nullptr);

    auto filteredCaps = adoptGRef(gst_caps_new_full(gst_structure_copy(structure), nullptr));
    g_object_set(m_capsFilter.get(), "caps", filteredCaps.get(), nullptr);

    gst_bin_add(GST_BIN_CAST(m_bin.get()), m_payloader.get());
    gst_element_link_many(m_outgoingSource.get(), m_valve.get(), m_videoFlip.get(), m_videoConvert.get(), m_preEncoderQueue.get(),
        m_encoder.get(), m_payloader.get(), m_postEncoderQueue.get(), nullptr);
    return true;
}

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
