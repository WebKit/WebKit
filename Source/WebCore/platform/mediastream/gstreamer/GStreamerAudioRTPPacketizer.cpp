/*
 *  Copyright (C) 2024 Igalia S.L. All rights reserved.
 *  Copyright (C) 2024 Metrological Group B.V.
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
#include "GStreamerAudioRTPPacketizer.h"

#if USE(GSTREAMER_WEBRTC)

#include "GStreamerCommon.h"
#include "GStreamerRegistryScanner.h"
#include <gst/rtp/rtp.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

GST_DEBUG_CATEGORY(webkit_webrtc_audio_rtp_packetizer_debug);
#define GST_CAT_DEFAULT webkit_webrtc_audio_rtp_packetizer_debug

RefPtr<GStreamerAudioRTPPacketizer> GStreamerAudioRTPPacketizer::create(RefPtr<UniqueSSRCGenerator> ssrcGenerator, const GstStructure* codecParameters, GUniquePtr<GstStructure>&& encodingParameters)
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_audio_rtp_packetizer_debug, "webkitwebrtcrtppacketizeraudio", 0, "WebKit WebRTC Audio RTP Packetizer");
    });

    GST_DEBUG("Creating packetizer for codec: %" GST_PTR_FORMAT " and encoding parameters %" GST_PTR_FORMAT, codecParameters, encodingParameters.get());
    String encoding;
    if (auto encodingName = gstStructureGetString(codecParameters, "encoding-name"_s))
        encoding = encodingName.convertToASCIILowercase();
    else {
        GST_ERROR("encoding-name not found");
        return nullptr;
    }

    auto& registryScanner = GStreamerRegistryScanner::singleton();
    auto lookupResult = registryScanner.isRtpPacketizerSupported(encoding);
    if (!lookupResult) {
        GST_ERROR("RTP payloader not found for encoding %s", encoding.ascii().data());
        return nullptr;
    }
    GRefPtr<GstElement> payloader = gst_element_factory_create(lookupResult.factory.get(), nullptr);
    GST_DEBUG("Using %" GST_PTR_FORMAT " for %s RTP packetizing", payloader.get(), encoding.ascii().data());

    auto inputCaps = adoptGRef(gst_caps_new_any());
    GUniquePtr<GstStructure> structure(gst_structure_copy(codecParameters));

    auto ssrc = ssrcGenerator->generateSSRC();
    if (ssrc != std::numeric_limits<uint32_t>::max())
        gst_structure_set(structure.get(), "ssrc", G_TYPE_UINT, ssrc, nullptr);

    GRefPtr<GstElement> encoder;
    if (encoding == "opus"_s) {
        encoder = makeGStreamerElement("opusenc", nullptr);
        if (!encoder)
            return nullptr;

        gst_structure_set(structure.get(), "encoding-name", G_TYPE_STRING, "OPUS", nullptr);

        // FIXME: Enable dtx too?
        gst_util_set_object_arg(G_OBJECT(encoder.get()), "audio-type", "voice");
        g_object_set(encoder.get(), "perfect-timestamp", TRUE, nullptr);

        if (auto useInbandFec = gstStructureGetString(structure.get(), "useinbandfec"_s)) {
            if (useInbandFec == "1"_s)
                g_object_set(encoder.get(), "inband-fec", TRUE, nullptr);
            gst_structure_remove_field(structure.get(), "useinbandfec");
        }

        if (auto isStereo = gstStructureGetString(structure.get(), "stereo"_s)) {
            if (isStereo == "1"_s)
                inputCaps = adoptGRef(gst_caps_new_simple("audio/x-raw", "channels", G_TYPE_INT, 2, nullptr));
            gst_structure_remove_field(structure.get(), "stereo");
        }

        if (gst_caps_is_any(inputCaps.get())) {
            if (auto encodingParameters = gstStructureGetString(structure.get(), "encoding-params"_s)) {
                if (auto channels = parseIntegerAllowingTrailingJunk<int>(encodingParameters))
                    inputCaps = adoptGRef(gst_caps_new_simple("audio/x-raw", "channels", G_TYPE_INT, *channels, nullptr));
            }
        }
    } else if (encoding == "g722"_s)
        encoder = makeGStreamerElement("avenc_g722", nullptr);
    else if (encoding == "pcma"_s)
        encoder = makeGStreamerElement("alawenc", nullptr);
    else if (encoding == "pcmu"_s)
        encoder = makeGStreamerElement("mulawenc", nullptr);
    else {
        GST_ERROR("Unsupported outgoing audio encoding: %s", encoding.ascii().data());
        return nullptr;
    }

    if (!encoder) {
        GST_ERROR("Encoder not found for encoding %s", encoding.ascii().data());
        return nullptr;
    }

    // Align MTU with libwebrtc implementation, also helping to reduce packet fragmentation.
    g_object_set(payloader.get(), "auto-header-extension", TRUE, "mtu", 1200, nullptr);

    if (auto minPTime = gstStructureGetString(structure.get(), "minptime"_s)) {
        if (auto value = parseIntegerAllowingTrailingJunk<int64_t>(minPTime)) {
            if (gstObjectHasProperty(payloader.get(), "min-ptime"))
                g_object_set(payloader.get(), "min-ptime", *value * GST_MSECOND, nullptr);
            else
                GST_WARNING_OBJECT(payloader.get(), "min-ptime property not supported");
        }
        gst_structure_remove_field(structure.get(), "minptime");
    }

    auto payloadType = gstStructureGet<int>(codecParameters, "payload"_s);
    if (payloadType)
        g_object_set(payloader.get(), "pt", *payloadType, nullptr);
    else {
        payloadType = gstStructureGet<int>(encodingParameters.get(), "payload"_s);
        if (payloadType)
            g_object_set(payloader.get(), "pt", *payloadType, nullptr);
    }

    auto rtpCaps = adoptGRef(gst_caps_new_empty());

    // When not present in caps, the vad support of the ssrc-audio-level extension should be
    // enabled. In order to prevent caps negotiation issues with downstream, explicitely set it.
    setSsrcAudioLevelVadOn(structure.get());

    gst_caps_append_structure(rtpCaps.get(), structure.release());
    return adoptRef(*new GStreamerAudioRTPPacketizer(WTFMove(inputCaps), WTFMove(encoder), WTFMove(payloader), WTFMove(encodingParameters), WTFMove(rtpCaps)));
}

GStreamerAudioRTPPacketizer::GStreamerAudioRTPPacketizer(GRefPtr<GstCaps>&& inputCaps, GRefPtr<GstElement>&& encoder, GRefPtr<GstElement>&& payloader, GUniquePtr<GstStructure>&& encodingParameters, GRefPtr<GstCaps>&& rtpCaps)
    : GStreamerRTPPacketizer(WTFMove(encoder), WTFMove(payloader), WTFMove(encodingParameters))
{
    g_object_set(m_capsFilter.get(), "caps", rtpCaps.get(), nullptr);
    GST_DEBUG_OBJECT(m_bin.get(), "RTP caps: %" GST_PTR_FORMAT, rtpCaps.get());

    m_audioconvert = makeGStreamerElement("audioconvert", nullptr);
    m_audioresample = makeGStreamerElement("audioresample", nullptr);
    m_inputCapsFilter = gst_element_factory_make("capsfilter", nullptr);
    g_object_set(m_inputCapsFilter.get(), "caps", inputCaps.get(), nullptr);

    gst_bin_add_many(GST_BIN_CAST(m_bin.get()), m_audioconvert.get(), m_audioresample.get(), m_inputCapsFilter.get(), nullptr);
    gst_element_link_many(m_inputQueue.get(), m_audioconvert.get(), m_audioresample.get(), m_inputCapsFilter.get(), m_encoder.get(), m_payloader.get(), m_capsFilter.get(), m_outputQueue.get(), m_valve.get(), nullptr);
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
