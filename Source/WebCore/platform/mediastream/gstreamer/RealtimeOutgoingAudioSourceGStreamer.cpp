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

#include "GStreamerRegistryScanner.h"
#include "MediaStreamTrack.h"

#include <wtf/text/StringToIntegerConversion.h>

GST_DEBUG_CATEGORY_EXTERN(webkit_webrtc_endpoint_debug);
#define GST_CAT_DEFAULT webkit_webrtc_endpoint_debug

namespace WebCore {

RealtimeOutgoingAudioSourceGStreamer::RealtimeOutgoingAudioSourceGStreamer(const String& mediaStreamId, MediaStreamTrack& track)
    : RealtimeOutgoingMediaSourceGStreamer(mediaStreamId, track)
{
    static Atomic<uint64_t> sourceCounter = 0;
    gst_element_set_name(m_bin.get(), makeString("outgoing-audio-source-", sourceCounter.exchangeAdd(1)).ascii().data());
    m_audioconvert = makeGStreamerElement("audioconvert", nullptr);
    m_audioresample = makeGStreamerElement("audioresample", nullptr);
    gst_bin_add_many(GST_BIN_CAST(m_bin.get()), m_audioconvert.get(), m_audioresample.get(), nullptr);
}

RTCRtpCapabilities RealtimeOutgoingAudioSourceGStreamer::rtpCapabilities() const
{
    auto& registryScanner = GStreamerRegistryScanner::singleton();
    return registryScanner.audioRtpCapabilities(GStreamerRegistryScanner::Configuration::Encoding);
}

bool RealtimeOutgoingAudioSourceGStreamer::setPayloadType(const GRefPtr<GstCaps>& caps)
{
    GST_DEBUG_OBJECT(m_bin.get(), "Setting payload caps: %" GST_PTR_FORMAT, caps.get());
    auto* structure = gst_caps_get_structure(caps.get(), 0);
    const char* encodingName = gst_structure_get_string(structure, "encoding-name");
    if (!encodingName) {
        GST_ERROR_OBJECT(m_bin.get(), "encoding-name not found");
        return false;
    }

    auto encoding = makeString(encodingName).convertToASCIILowercase();
    m_payloader = makeGStreamerElement(makeString("rtp"_s, encoding, "pay"_s).ascii().data(), nullptr);
    if (UNLIKELY(!m_payloader)) {
        GST_ERROR_OBJECT(m_bin.get(), "RTP payloader not found for encoding %s", encodingName);
        return false;
    }

    if (encoding == "opus"_s) {
        m_encoder = makeGStreamerElement("opusenc", nullptr);
        if (!m_encoder)
            return false;

        // FIXME: Enable dtx too?
        gst_util_set_object_arg(G_OBJECT(m_encoder.get()), "audio-type", "voice");

        if (const char* useInbandFec = gst_structure_get_string(structure, "useinbandfec")) {
            if (!strcmp(useInbandFec, "1"))
                g_object_set(m_encoder.get(), "inband-fec", true, nullptr);
        }
    } else if (encoding == "g722"_s)
        m_encoder = makeGStreamerElement("avenc_g722", nullptr);
    else if (encoding == "pcma"_s)
        m_encoder = makeGStreamerElement("alawenc", nullptr);
    else if (encoding == "pcmu"_s)
        m_encoder = makeGStreamerElement("mulawenc", nullptr);
    else if (encoding == "isac"_s)
        m_encoder = makeGStreamerElement("isacenc", nullptr);
    else {
        GST_ERROR_OBJECT(m_bin.get(), "Unsupported outgoing audio encoding: %s", encodingName);
        return false;
    }

    if (!m_encoder) {
        GST_ERROR_OBJECT(m_bin.get(), "Encoder not found for encoding %s", encodingName);
        return false;
    }

    // FIXME: Re-enable auto-header-extension. Currently triggers caps negotiation error.
    // Align MTU with libwebrtc implementation, also helping to reduce packet fragmentation.
    g_object_set(m_payloader.get(), "auto-header-extension", FALSE, "mtu", 1200, nullptr);

    if (const char* minPTime = gst_structure_get_string(structure, "minptime")) {
        auto time = String::fromLatin1(minPTime);
        if (auto value = parseIntegerAllowingTrailingJunk<int64_t>(time))
            g_object_set(m_payloader.get(), "min-ptime", *value * GST_MSECOND, nullptr);
    }

    int payloadType;
    if (gst_structure_get_int(structure, "payload", &payloadType))
        g_object_set(m_payloader.get(), "pt", payloadType, nullptr);

    g_object_set(m_capsFilter.get(), "caps", caps.get(), nullptr);

    gst_bin_add_many(GST_BIN_CAST(m_bin.get()), m_payloader.get(), m_encoder.get(), nullptr);

    auto preEncoderSinkPad = adoptGRef(gst_element_get_static_pad(m_preEncoderQueue.get(), "sink"));
    if (!gst_pad_is_linked(preEncoderSinkPad.get()))
        gst_element_link_many(m_outgoingSource.get(), m_valve.get(), m_audioconvert.get(), m_audioresample.get(), m_preEncoderQueue.get(), nullptr);

    gst_element_link_many(m_preEncoderQueue.get(), m_encoder.get(), m_payloader.get(), m_postEncoderQueue.get(), nullptr);
    gst_bin_sync_children_states(GST_BIN_CAST(m_bin.get()));
    return true;
}

void RealtimeOutgoingAudioSourceGStreamer::codecPreferencesChanged(const GRefPtr<GstCaps>& codecPreferences)
{
    gst_element_set_locked_state(m_bin.get(), TRUE);
    if (m_payloader) {
        gst_element_set_locked_state(m_payloader.get(), TRUE);
        gst_element_set_locked_state(m_encoder.get(), TRUE);
        gst_element_set_state(m_payloader.get(), GST_STATE_NULL);
        gst_element_set_state(m_encoder.get(), GST_STATE_NULL);
        gst_element_unlink_many(m_preEncoderQueue.get(), m_encoder.get(), m_payloader.get(), m_postEncoderQueue.get(), nullptr);
        gst_bin_remove_many(GST_BIN_CAST(m_bin.get()), m_payloader.get(), m_encoder.get(), nullptr);
        m_payloader.clear();
        m_encoder.clear();
    }
    setPayloadType(codecPreferences);
    gst_element_set_locked_state(m_bin.get(), FALSE);
    gst_element_sync_state_with_parent(m_bin.get());
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN_CAST(m_bin.get()), GST_DEBUG_GRAPH_SHOW_ALL, "outgoing-audio-new-codec-prefs");
}

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
