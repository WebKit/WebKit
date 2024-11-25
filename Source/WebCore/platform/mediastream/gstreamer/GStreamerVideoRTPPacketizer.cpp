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
#include "GStreamerVideoRTPPacketizer.h"

#if USE(GSTREAMER_WEBRTC)

#include "AV1Utilities.h"
#include "GStreamerCommon.h"
#include "GStreamerRegistryScanner.h"
#include "HEVCUtilities.h"
#include "VP9Utilities.h"
#include "VideoEncoderPrivateGStreamer.h"
#include <gst/rtp/rtp.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

GST_DEBUG_CATEGORY(webkit_webrtc_video_rtp_packetizer_debug);
#define GST_CAT_DEFAULT webkit_webrtc_video_rtp_packetizer_debug

RefPtr<GStreamerVideoRTPPacketizer> GStreamerVideoRTPPacketizer::create(RefPtr<UniqueSSRCGenerator> ssrcGenerator, const GstStructure* codecParameters, GUniquePtr<GstStructure>&& encodingParameters)
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_video_rtp_packetizer_debug, "webkitwebrtcrtppacketizervideo", 0, "WebKit WebRTC Video RTP Packetizer");
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

    auto codec = emptyString();
    if (encoding == "vp8"_s) {
        if (gstObjectHasProperty(payloader.get(), "picture-id-mode"))
            gst_util_set_object_arg(G_OBJECT(payloader.get()), "picture-id-mode", "15-bit");

        codec = "vp8"_s;
    } else if (encoding == "vp9"_s) {
        if (gstObjectHasProperty(payloader.get(), "picture-id-mode"))
            gst_util_set_object_arg(G_OBJECT(payloader.get()), "picture-id-mode", "15-bit");

        VPCodecConfigurationRecord record;
        record.codecName = "vp09"_s;
        if (auto vp9Profile = gstStructureGetString(codecParameters, "profile-id"_s)) {
            if (auto profile = parseInteger<uint8_t>(vp9Profile))
                record.profile = *profile;
        }
        codec = createVPCodecParametersString(record);
    } else if (encoding == "h264"_s) {
        gst_util_set_object_arg(G_OBJECT(payloader.get()), "aggregate-mode", "zero-latency");
        g_object_set(payloader.get(), "config-interval", -1, nullptr);

        auto profileValue = gstStructureGetString(codecParameters, "profile"_s);
        auto profile = profileValue ? profileValue : "constrained-baseline"_s;

        AVCParameters parameters;
        if (profile == "baseline"_s)
            parameters.profileIDC = 66;
        else if (profile == "constrained-baseline"_s) {
            parameters.profileIDC = 66;
            parameters.constraintsFlags |= 1 << 6;
        } else if (profile == "main"_s)
            parameters.profileIDC = 77;

        codec = createAVCCodecParametersString(parameters);
    } else if (encoding == "h265"_s) {
        gst_util_set_object_arg(G_OBJECT(payloader.get()), "aggregate-mode", "zero-latency");
        g_object_set(payloader.get(), "config-interval", -1, nullptr);
        // FIXME: profile tier level?
        codec = createHEVCCodecParametersString({ });
    } else if (encoding == "av1"_s)
        codec = createAV1CodecParametersString({ });
    else {
        GST_ERROR("Unsupported outgoing video encoding: %s", encoding.ascii().data());
        return nullptr;
    }

    // Align MTU with libwebrtc implementation, also helping to reduce packet fragmentation.
    g_object_set(payloader.get(), "auto-header-extension", TRUE, "mtu", 1200, nullptr);

    auto payloadType = gstStructureGet<int>(codecParameters, "payload"_s);
    if (payloadType)
        g_object_set(payloader.get(), "pt", *payloadType, nullptr);
    else {
        payloadType = gstStructureGet<int>(encodingParameters.get(), "payload"_s);
        if (payloadType)
            g_object_set(payloader.get(), "pt", *payloadType, nullptr);
    }

    GRefPtr<GstElement> encoder = gst_element_factory_make("webkitvideoencoder", nullptr);
    if (!videoEncoderSetCodec(WEBKIT_VIDEO_ENCODER(encoder.get()), WTFMove(codec))) {
        GST_ERROR("Unable to set encoder format");
        return nullptr;
    }

    GUniquePtr<GstStructure> structure(gst_structure_copy(codecParameters));

    auto ssrc = ssrcGenerator->generateSSRC();
    if (ssrc != std::numeric_limits<uint32_t>::max())
        gst_structure_set(structure.get(), "ssrc", G_TYPE_UINT, ssrc, nullptr);

    auto rtpCaps = adoptGRef(gst_caps_new_empty());
    gst_caps_append_structure(rtpCaps.get(), structure.release());
    return adoptRef(*new GStreamerVideoRTPPacketizer(WTFMove(encoder), WTFMove(payloader), WTFMove(encodingParameters), WTFMove(rtpCaps)));
}

GStreamerVideoRTPPacketizer::GStreamerVideoRTPPacketizer(GRefPtr<GstElement>&& encoder, GRefPtr<GstElement>&& payloader, GUniquePtr<GstStructure>&& encodingParameters, GRefPtr<GstCaps>&& rtpCaps)
    : GStreamerRTPPacketizer(WTFMove(encoder), WTFMove(payloader), WTFMove(encodingParameters))
{
    GST_DEBUG_OBJECT(m_bin.get(), "RTP caps: %" GST_PTR_FORMAT, rtpCaps.get());
    g_object_set(m_capsFilter.get(), "caps", rtpCaps.get(), nullptr);

    GST_DEBUG_OBJECT(m_bin.get(), "RTP encoding parameters: %" GST_PTR_FORMAT, m_encodingParameters.get());

    m_videoRate = makeGStreamerElement("videorate", nullptr);
    // https://gitlab.freedesktop.org/gstreamer/gst-plugins-base/issues/97#note_56575
    g_object_set(m_videoRate.get(), "skip-to-first", TRUE, "drop-only", TRUE, "average-period", UINT64_C(1), nullptr);
    m_frameRateCapsFilter = makeGStreamerElement("capsfilter", nullptr);
    gst_bin_add_many(GST_BIN_CAST(m_bin.get()), m_videoRate.get(), m_frameRateCapsFilter.get(), nullptr);

    auto lastIdentifier = findLastExtensionId(rtpCaps.get());
    auto rtpStreamId = this->rtpStreamId();
    if (!rtpStreamId.isEmpty()) {
        GST_DEBUG_OBJECT(m_bin.get(), "Configuring rtp-stream-id extension for rid: %s", rtpStreamId.ascii().data());
        auto extension = adoptGRef(gst_rtp_header_extension_create_from_uri(GST_RTP_HDREXT_BASE "sdes:rtp-stream-id"));
        lastIdentifier++;
        gst_rtp_header_extension_set_id(extension.get(), lastIdentifier);
        g_object_set(extension.get(), "rid", rtpStreamId.ascii().data(), nullptr);
        g_signal_emit_by_name(m_payloader.get(), "add-extension", extension.get());
    }

    auto extension = adoptGRef(gst_rtp_header_extension_create_from_uri(GST_RTP_HDREXT_BASE "sdes:mid"));
    lastIdentifier++;
    gst_rtp_header_extension_set_id(extension.get(), lastIdentifier);
    g_signal_emit_by_name(m_payloader.get(), "add-extension", extension.get());

    // TODO: Repaired stream-id extension?

    if (m_encodingParameters)
        configure(m_encodingParameters.get());

    gst_element_link_many(m_inputQueue.get(), m_videoRate.get(), m_frameRateCapsFilter.get(), m_encoder.get(), m_payloader.get(), m_capsFilter.get(), m_outputQueue.get(), m_valve.get(), nullptr);
}

void GStreamerVideoRTPPacketizer::configure(const GstStructure* encodingParameters) const
{
    ASSERT(encodingParameters);
    GST_DEBUG_OBJECT(m_bin.get(), "Configuring with encoding parameters: %" GST_PTR_FORMAT, encodingParameters);

    auto maxFrameRate = gstStructureGet<unsigned>(encodingParameters, "max-framerate"_s).value_or(0);
    if (maxFrameRate) {
        if (!m_videoRate)
            GST_WARNING_OBJECT(m_bin.get(), "Unable to configure max-framerate");
        else {
            // Some decoder(s), like FFMpeg don't handle 1 FPS framerate, so set a minimum more likely to be accepted.
            if (maxFrameRate < 2)
                maxFrameRate = 2;

            int numerator, denominator;
            gst_util_double_to_fraction(static_cast<double>(maxFrameRate), &numerator, &denominator);

            auto caps = adoptGRef(gst_caps_new_simple("video/x-raw", "framerate", GST_TYPE_FRACTION, numerator, denominator, nullptr));
            g_object_set(m_frameRateCapsFilter.get(), "caps", caps.get(), nullptr);
        }
    }

    if (auto maxBitrate = gstStructureGet<unsigned>(encodingParameters, "max-bitrate"_s)) {
        // maxBitrate is expessed in bits/s but the encoder property is in Kbit/s.
        if (*maxBitrate >= 1000) {
            if (*maxBitrate / 1000 < UINT_MAX)
                g_object_set(m_encoder.get(), "bitrate", static_cast<uint32_t>(*maxBitrate / 1000), nullptr);
        }
    }

    if (auto scaleResolutionDownBy = gstStructureGet<double>(encodingParameters, "scale-resolution-down-by"_s))
        g_object_set(m_encoder.get(), "scale-resolution-down-by", *scaleResolutionDownBy, nullptr);
}

void GStreamerVideoRTPPacketizer::updateStats()
{
    if (UNLIKELY(!m_encoder))
        return;

    auto framesSent = gstStructureGet<uint64_t>(m_stats.get(), "frames-sent"_s).value_or(0);
    framesSent++;

    uint32_t bitrate;
    g_object_get(m_encoder.get(), "bitrate", &bitrate, nullptr);
    gst_structure_set(m_stats.get(), "bitrate", G_TYPE_DOUBLE, static_cast<double>(bitrate * 1000), nullptr);

    auto pad = adoptGRef(gst_element_get_static_pad(m_encoder.get(), "src"));
    auto caps = adoptGRef(gst_pad_get_current_caps(pad.get()));
    if (caps && !gst_caps_is_empty(caps.get())) {
        auto structure = gst_caps_get_structure(caps.get(), 0);
        if (auto width = gstStructureGet<int>(structure, "width"_s))
            gst_structure_set(m_stats.get(), "frame-width", G_TYPE_UINT, *width, nullptr);
        if (auto height = gstStructureGet<int>(structure, "height"_s))
            gst_structure_set(m_stats.get(), "frame-height", G_TYPE_UINT, *height, nullptr);

        int numerator, denominator;
        if (gst_structure_get_fraction(structure, "framerate", &numerator, &denominator)) {
            double framerate;
            gst_util_fraction_to_double(numerator, denominator, &framerate);
            gst_structure_set(m_stats.get(), "frames-per-second", G_TYPE_DOUBLE, framerate, nullptr);
        }
    }

    gst_structure_set(m_stats.get(), "frames-sent", G_TYPE_UINT64, framesSent, "frames-encoded", G_TYPE_UINT64, framesSent, nullptr);
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
