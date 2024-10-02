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

#include "AV1Utilities.h"
#include "GStreamerCommon.h"
#include "GStreamerRegistryScanner.h"
#include "HEVCUtilities.h"
#include "MediaStreamTrack.h"
#include "VP9Utilities.h"
#include "VideoEncoderPrivateGStreamer.h"
#include <wtf/glib/WTFGType.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>

GST_DEBUG_CATEGORY(webkit_webrtc_outgoing_video_debug);
#define GST_CAT_DEFAULT webkit_webrtc_outgoing_video_debug

namespace WebCore {

struct RealtimeOutgoingVideoSourceHolder {
    RefPtr<RealtimeOutgoingVideoSourceGStreamer> source;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(RealtimeOutgoingVideoSourceHolder)


RealtimeOutgoingVideoSourceGStreamer::RealtimeOutgoingVideoSourceGStreamer(const RefPtr<UniqueSSRCGenerator>& ssrcGenerator, const String& mediaStreamId, MediaStreamTrack& track)
    : RealtimeOutgoingMediaSourceGStreamer(RealtimeOutgoingMediaSourceGStreamer::Type::Video, ssrcGenerator, mediaStreamId, track)
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_outgoing_video_debug, "webkitwebrtcoutgoingvideo", 0, "WebKit WebRTC outgoing video");
    });
    registerWebKitGStreamerElements();

    static Atomic<uint64_t> sourceCounter = 0;
    gst_element_set_name(m_bin.get(), makeString("outgoing-video-source-"_s, sourceCounter.exchangeAdd(1)).ascii().data());

    m_stats.reset(gst_structure_new_empty("webrtc-outgoing-video-stats"));

    m_videoConvert = makeGStreamerElement("videoconvert", nullptr);

    m_videoFlip = makeGStreamerElement("videoflip", nullptr);
    gst_util_set_object_arg(G_OBJECT(m_videoFlip.get()), "method", "automatic");

    m_videoRate = makeGStreamerElement("videorate", nullptr);
    m_frameRateCapsFilter = makeGStreamerElement("capsfilter", nullptr);

    // https://gitlab.freedesktop.org/gstreamer/gst-plugins-base/issues/97#note_56575
    g_object_set(m_videoRate.get(), "skip-to-first", TRUE, "drop-only", TRUE, "average-period", UINT64_C(1), nullptr);

    m_encoder = gst_element_factory_make("webkitvideoencoder", nullptr);
    gst_bin_add_many(GST_BIN_CAST(m_bin.get()), m_videoRate.get(), m_frameRateCapsFilter.get(), m_videoFlip.get(), m_videoConvert.get(), m_encoder.get(), nullptr);
    startUpdatingStats();
}

RTCRtpCapabilities RealtimeOutgoingVideoSourceGStreamer::rtpCapabilities() const
{
    auto& registryScanner = GStreamerRegistryScanner::singleton();
    return registryScanner.videoRtpCapabilities(GStreamerRegistryScanner::Configuration::Encoding);
}

void RealtimeOutgoingVideoSourceGStreamer::updateStats(GstBuffer*)
{
    uint64_t framesSent = 0;
    gst_structure_get_uint64(m_stats.get(), "frames-sent", &framesSent);
    framesSent++;

    if (m_encoder) {
        uint32_t bitrate;
        g_object_get(m_encoder.get(), "bitrate", &bitrate, nullptr);
        gst_structure_set(m_stats.get(), "bitrate", G_TYPE_DOUBLE, static_cast<double>(bitrate * 1000), nullptr);
    }

    gst_structure_set(m_stats.get(), "frames-sent", G_TYPE_UINT64, framesSent, "frames-encoded", G_TYPE_UINT64, framesSent, nullptr);
}

void RealtimeOutgoingVideoSourceGStreamer::teardown()
{
    RealtimeOutgoingMediaSourceGStreamer::teardown();
    m_videoConvert.clear();
    m_videoFlip.clear();
    m_videoRate.clear();
    m_frameRateCapsFilter.clear();
    stopUpdatingStats();
    m_stats.reset();
}

bool RealtimeOutgoingVideoSourceGStreamer::setPayloadType(const GRefPtr<GstCaps>& caps)
{
    GST_DEBUG_OBJECT(m_bin.get(), "Setting payload caps: %" GST_PTR_FORMAT, caps.get());
    // FIXME: We use only the first structure of the caps. This not be the right approach specially
    // we don't have a payloader or encoder for that format.
    GUniquePtr<GstStructure> structure(gst_structure_copy(gst_caps_get_structure(caps.get(), 0)));
    String encoding;
    if (auto encodingName = gstStructureGetString(structure.get(), "encoding-name"_s))
        encoding = encodingName.convertToASCIILowercase();
    else {
        GST_ERROR_OBJECT(m_bin.get(), "encoding-name not found");
        return false;
    }

    auto& registryScanner = GStreamerRegistryScanner::singleton();
    auto lookupResult = registryScanner.isRtpPacketizerSupported(encoding);
    if (!lookupResult) {
        GST_ERROR_OBJECT(m_bin.get(), "RTP payloader not found for encoding %s", encoding.ascii().data());
        return false;
    }
    m_payloader = gst_element_factory_create(lookupResult.factory.get(), nullptr);
    GST_DEBUG_OBJECT(m_bin.get(), "Using %" GST_PTR_FORMAT " for %s RTP packetizing", m_payloader.get(), encoding.ascii().data());

    auto codec = emptyString();
    if (encoding == "vp8"_s) {
        if (gstObjectHasProperty(m_payloader.get(), "picture-id-mode"))
            gst_util_set_object_arg(G_OBJECT(m_payloader.get()), "picture-id-mode", "15-bit");

        codec = "vp8"_s;
    } else if (encoding == "vp9"_s) {
        if (gstObjectHasProperty(m_payloader.get(), "picture-id-mode"))
            gst_util_set_object_arg(G_OBJECT(m_payloader.get()), "picture-id-mode", "15-bit");

        VPCodecConfigurationRecord record;
        record.codecName = "vp09"_s;
        if (auto vp9Profile = gstStructureGetString(structure.get(), "profile-id"_s)) {
            if (auto profile = parseInteger<uint8_t>(vp9Profile))
                record.profile = *profile;
        }
        codec = createVPCodecParametersString(record);
    } else if (encoding == "h264"_s) {
        gst_util_set_object_arg(G_OBJECT(m_payloader.get()), "aggregate-mode", "zero-latency");
        g_object_set(m_payloader.get(), "config-interval", -1, nullptr);

        auto profileValue = gstStructureGetString(structure.get(), "profile"_s);
        auto profile = profileValue ? profileValue : "baseline"_s;

        AVCParameters parameters;
        if (profile == "baseline"_s)
            parameters.profileIDC = 66;
        else if (profile == "constrained-baseline"_s) {
            parameters.profileIDC = 66;
            parameters.constraintsFlags |= 0x40 << 6;
        } else if (profile == "main"_s)
            parameters.profileIDC = 77;

        codec = createAVCCodecParametersString(parameters);
    } else if (encoding == "h265"_s) {
        gst_util_set_object_arg(G_OBJECT(m_payloader.get()), "aggregate-mode", "zero-latency");
        g_object_set(m_payloader.get(), "config-interval", -1, nullptr);
        // FIXME: profile tier level?
        codec = createHEVCCodecParametersString({ });
    } else if (encoding == "av1"_s)
        codec = createAV1CodecParametersString({ });
    else {
        GST_ERROR_OBJECT(m_bin.get(), "Unsupported outgoing video encoding: %s", encoding.ascii().data());
        return false;
    }

    // Align MTU with libwebrtc implementation, also helping to reduce packet fragmentation.
    g_object_set(m_payloader.get(), "auto-header-extension", TRUE, "mtu", 1200, nullptr);

    if (!videoEncoderSetCodec(WEBKIT_VIDEO_ENCODER(m_encoder.get()), WTFMove(codec))) {
        GST_ERROR_OBJECT(m_bin.get(), "Unable to set encoder format");
        return false;
    }

    if (auto payloadType = gstStructureGet<int>(structure.get(), "payload"_s))
        g_object_set(m_payloader.get(), "pt", *payloadType, nullptr);

    if (m_payloaderState) {
        g_object_set(m_payloader.get(), "seqnum-offset", m_payloaderState->seqnum, nullptr);
        m_payloaderState.reset();
    }

    auto rtpCaps = adoptGRef(gst_caps_new_empty());
    gst_caps_append_structure(rtpCaps.get(), structure.release());

    g_object_set(m_capsFilter.get(), "caps", rtpCaps.get(), nullptr);
    GST_DEBUG_OBJECT(m_bin.get(), "RTP caps: %" GST_PTR_FORMAT, rtpCaps.get());

    gst_bin_add(GST_BIN_CAST(m_bin.get()), m_payloader.get());

    auto encoderSinkPad = adoptGRef(gst_element_get_static_pad(m_encoder.get(), "sink"));
    if (!gst_pad_is_linked(encoderSinkPad.get())) {
        if (!gst_element_link_many(m_outgoingSource.get(), m_liveSync.get(), m_videoFlip.get(), nullptr)) {
            GST_ERROR_OBJECT(m_bin.get(), "Unable to link outgoing source to videoflip");
            return false;
        }

        GstElement* tail = m_videoFlip.get();
        if (m_videoRate) {
            if (!gst_element_link_many(m_videoFlip.get(), m_videoRate.get(), m_frameRateCapsFilter.get(), nullptr)) {
                GST_ERROR_OBJECT(m_bin.get(), "Unable to link outgoing source to videorate");
                return false;
            }
            tail = m_frameRateCapsFilter.get();
        }
        if (!gst_element_link_many(tail, m_videoConvert.get(), m_preEncoderQueue.get(), m_encoder.get(), nullptr)) {
            GST_ERROR_OBJECT(m_bin.get(), "Unable to link outgoing source to encoder");
            return false;
        }
    }

    return gst_element_link_many(m_encoder.get(), m_payloader.get(), m_postEncoderQueue.get(), nullptr);
}

void RealtimeOutgoingVideoSourceGStreamer::startUpdatingStats()
{
    GST_DEBUG_OBJECT(m_bin.get(), "Starting buffer monitoring for stats gathering");
    auto holder = createRealtimeOutgoingVideoSourceHolder();
    holder->source = this;
    auto pad = adoptGRef(gst_element_get_static_pad(m_encoder.get(), "src"));
    m_statsPadProbeId = gst_pad_add_probe(pad.get(), GST_PAD_PROBE_TYPE_BUFFER, [](GstPad*, GstPadProbeInfo* info, gpointer userData) -> GstPadProbeReturn {
        auto* holder = static_cast<RealtimeOutgoingVideoSourceHolder*>(userData);
        auto* buffer = GST_PAD_PROBE_INFO_BUFFER(info);
        holder->source->updateStats(buffer);
        return GST_PAD_PROBE_OK;
    }, holder, reinterpret_cast<GDestroyNotify>(destroyRealtimeOutgoingVideoSourceHolder));
}

void RealtimeOutgoingVideoSourceGStreamer::stopUpdatingStats()
{
    if (!m_statsPadProbeId)
        return;

    GST_DEBUG_OBJECT(m_bin.get(), "Stopping buffer monitoring for stats gathering");
    auto pad = adoptGRef(gst_element_get_static_pad(m_encoder.get(), "src"));
    gst_pad_remove_probe(pad.get(), m_statsPadProbeId);
    m_statsPadProbeId = 0;
}

void RealtimeOutgoingVideoSourceGStreamer::sourceEnabledChanged()
{
    RealtimeOutgoingMediaSourceGStreamer::sourceEnabledChanged();
    if (m_enabled)
        startUpdatingStats();
    else
        stopUpdatingStats();
}

void RealtimeOutgoingVideoSourceGStreamer::flush()
{
    GST_DEBUG_OBJECT(m_bin.get(), "Requesting key-frame");
    gst_element_send_event(m_outgoingSource.get(), gst_video_event_new_downstream_force_key_unit(GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE, FALSE, 1));
}

void RealtimeOutgoingVideoSourceGStreamer::setParameters(GUniquePtr<GstStructure>&& parameters)
{
    m_parameters = WTFMove(parameters);
    GST_DEBUG_OBJECT(m_bin.get(), "New encoding parameters: %" GST_PTR_FORMAT, m_parameters.get());

    auto* encodingsValue = gst_structure_get_value(m_parameters.get(), "encodings");
    RELEASE_ASSERT(GST_VALUE_HOLDS_LIST(encodingsValue));
    if (UNLIKELY(!gst_value_list_get_size(encodingsValue))) {
        GST_WARNING_OBJECT(m_bin.get(), "Encodings list is empty, cancelling configuration");
        return;
    }

    auto* firstEncoding = gst_value_list_get_value(encodingsValue, 0);
    RELEASE_ASSERT(GST_VALUE_HOLDS_STRUCTURE(firstEncoding));
    auto* structure = gst_value_get_structure(firstEncoding);

    if (gst_structure_has_field(structure, "max-framerate")) {
        if (!m_videoRate)
            GST_WARNING_OBJECT(m_bin.get(), "Unable to configure max-framerate");
        else {
            unsigned long maxFrameRate;
            gst_structure_get(structure, "max-framerate", G_TYPE_ULONG, &maxFrameRate, nullptr);

            // Some decoder(s), like FFMpeg don't handle 1 FPS framerate, so set a minimum more likely to be accepted.
            if (maxFrameRate < 2)
                maxFrameRate = 2;

            int numerator, denominator;
            gst_util_double_to_fraction(static_cast<double>(maxFrameRate), &numerator, &denominator);

            auto caps = adoptGRef(gst_caps_new_simple("video/x-raw", "framerate", GST_TYPE_FRACTION, numerator, denominator, nullptr));
            g_object_set(m_frameRateCapsFilter.get(), "caps", caps.get(), nullptr);
        }
    }

    if (UNLIKELY(!m_encoder) || !gst_structure_has_field(structure, "max-bitrate"))
        return;

    unsigned long maxBitrate;
    gst_structure_get(structure, "max-bitrate", G_TYPE_ULONG, &maxBitrate, nullptr);

    // maxBitrate is expessed in bits/s but the encoder property is in Kbit/s.
    if (maxBitrate >= 1000)
        g_object_set(m_encoder.get(), "bitrate", static_cast<uint32_t>(maxBitrate / 1000), nullptr);
}

void RealtimeOutgoingVideoSourceGStreamer::fillEncodingParameters(const GUniquePtr<GstStructure>& encodingParameters)
{
    if (m_videoRate) {
        GRefPtr<GstCaps> caps;
        g_object_get(m_frameRateCapsFilter.get(), "caps", &caps.outPtr(), nullptr);
        double maxFrameRate = 30.0;
        if (!gst_caps_is_any(caps.get())) {
            if (auto* structure = gst_caps_get_structure(caps.get(), 0)) {
                int numerator, denominator;
                if (gst_structure_get_fraction(structure, "framerate", &numerator, &denominator))
                    gst_util_fraction_to_double(numerator, denominator, &maxFrameRate);
            }
        }

        gst_structure_set(encodingParameters.get(), "max-framerate", G_TYPE_DOUBLE, maxFrameRate, nullptr);
    }

    unsigned long maxBitrate = 2048 * 1000;
    if (m_encoder) {
        uint32_t bitrate;
        g_object_get(m_encoder.get(), "bitrate", &bitrate, nullptr);
        maxBitrate = bitrate * 1000;
    }

    gst_structure_set(encodingParameters.get(), "max-bitrate", G_TYPE_ULONG, maxBitrate, nullptr);
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
