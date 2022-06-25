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
#include "RealtimeOutgoingAudioSourceGStreamer.h"

#if USE(GSTREAMER_WEBRTC)

#include "GStreamerAudioCaptureSource.h"
#include <wtf/text/StringToIntegerConversion.h>

GST_DEBUG_CATEGORY_EXTERN(webkit_webrtc_endpoint_debug);
#define GST_CAT_DEFAULT webkit_webrtc_endpoint_debug

namespace WebCore {

RealtimeOutgoingAudioSourceGStreamer::RealtimeOutgoingAudioSourceGStreamer(Ref<MediaStreamTrackPrivate>&& source)
    : RealtimeOutgoingMediaSourceGStreamer()
{
    Vector<std::pair<const char*, int>> encodings;
    encodings.reserveInitialCapacity(4);
    encodings.uncheckedAppend({ "OPUS", 48000 });
    encodings.uncheckedAppend({ "G722", 8000 });
    encodings.uncheckedAppend({ "PCMA", 8000 });
    encodings.uncheckedAppend({ "PCMU", 8000 });
    m_allowedCaps = adoptGRef(gst_caps_new_empty());
    for (auto& [encodingName, clockRate] : encodings)
        gst_caps_append_structure(m_allowedCaps.get(), gst_structure_new("application/x-rtp", "media", G_TYPE_STRING, "audio", "encoding-name", G_TYPE_STRING, encodingName, "clock-rate", G_TYPE_INT, clockRate, nullptr));

    m_audioconvert = gst_element_factory_make("audioconvert", nullptr);
    m_audioresample = gst_element_factory_make("audioresample", nullptr);
    gst_bin_add_many(GST_BIN_CAST(m_bin.get()), m_audioconvert.get(), m_audioresample.get(), nullptr);
    setSource(WTFMove(source));
}

bool RealtimeOutgoingAudioSourceGStreamer::setPayloadType(const GRefPtr<GstCaps>& caps)
{
    GST_DEBUG_OBJECT(m_bin.get(), "Outgoing audio source payload caps: %" GST_PTR_FORMAT, caps.get());
    auto* structure = gst_caps_get_structure(caps.get(), 0);
    if (const char* encodingName = gst_structure_get_string(structure, "encoding-name")) {
        if (!strcmp(encodingName, "OPUS")) {
            m_encoder = makeGStreamerElement("opusenc", nullptr);
            m_payloader = makeGStreamerElement("rtpopuspay", nullptr);
            if (!m_encoder || !m_payloader)
                return false;

            // FIXME: Enable dtx too?
            gst_util_set_object_arg(G_OBJECT(m_encoder.get()), "audio-type", "voice");

            if (const char* useInbandFec = gst_structure_get_string(structure, "useinbandfec")) {
                if (!strcmp(useInbandFec, "1"))
                    g_object_set(m_encoder.get(), "inband-fec", true, nullptr);
            }
        } else if (!strcmp(encodingName, "G722")) {
            m_payloader = makeGStreamerElement("rtpg722pay", nullptr);
            if (!m_payloader)
                return false;
        } else if (!strcmp(encodingName, "PCMA") || !strcmp(encodingName, "PCMU")) {
            auto payloaderName = makeString("rtp", encodingName, "pay");
            m_payloader = makeGStreamerElement(payloaderName.convertToASCIILowercase().ascii().data(), nullptr);
            m_encoder = makeGStreamerElement("alawenc", nullptr);
            if (!m_payloader || !m_encoder)
                return false;
        } else {
            GST_ERROR_OBJECT(m_bin.get(), "Unsupported outgoing audio encoding: %s", encodingName);
            return false;
        }
    } else {
        GST_ERROR_OBJECT(m_bin.get(), "encoding-name not found");
        return false;
    }

    g_object_set(m_payloader.get(), "auto-header-extension", FALSE, nullptr);

    if (const char* minPTime = gst_structure_get_string(structure, "minptime")) {
        auto time = String::fromLatin1(minPTime);
        if (auto value = parseIntegerAllowingTrailingJunk<int64_t>(time))
            g_object_set(m_payloader.get(), "min-ptime", *value * GST_MSECOND, nullptr);
    }

    int payloadType;
    if (gst_structure_get_int(structure, "payload", &payloadType))
        g_object_set(m_payloader.get(), "pt", payloadType, nullptr);

    g_object_set(m_capsFilter.get(), "caps", caps.get(), nullptr);

    gst_bin_add(GST_BIN_CAST(m_bin.get()), m_payloader.get());
    if (m_encoder) {
        gst_bin_add(GST_BIN_CAST(m_bin.get()), m_encoder.get());
        gst_element_link_many(m_outgoingSource.get(), m_valve.get(), m_audioconvert.get(), m_audioresample.get(),
            m_preEncoderQueue.get(), m_encoder.get(), m_payloader.get(), m_postEncoderQueue.get(), nullptr);
    } else {
        gst_element_link_many(m_outgoingSource.get(), m_valve.get(), m_audioconvert.get(), m_audioresample.get(),
            m_payloader.get(), m_postEncoderQueue.get(), nullptr);
    }
    return true;
}

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
