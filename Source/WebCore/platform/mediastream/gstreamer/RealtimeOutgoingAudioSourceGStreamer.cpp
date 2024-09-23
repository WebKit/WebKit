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

#include "GStreamerCommon.h"
#include "GStreamerRegistryScanner.h"
#include "MediaStreamTrack.h"
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>

GST_DEBUG_CATEGORY(webkit_webrtc_outgoing_audio_debug);
#define GST_CAT_DEFAULT webkit_webrtc_outgoing_audio_debug

namespace WebCore {

RealtimeOutgoingAudioSourceGStreamer::RealtimeOutgoingAudioSourceGStreamer(const RefPtr<UniqueSSRCGenerator>& ssrcGenerator, const String& mediaStreamId, MediaStreamTrack& track)
    : RealtimeOutgoingMediaSourceGStreamer(RealtimeOutgoingMediaSourceGStreamer::Type::Audio, ssrcGenerator, mediaStreamId, track)
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_outgoing_audio_debug, "webkitwebrtcoutgoingaudio", 0, "WebKit WebRTC outgoing audio");
    });
    static Atomic<uint64_t> sourceCounter = 0;
    gst_element_set_name(m_bin.get(), makeString("outgoing-audio-source-"_s, sourceCounter.exchangeAdd(1)).ascii().data());
    m_audioconvert = makeGStreamerElement("audioconvert", nullptr);
    m_audioresample = makeGStreamerElement("audioresample", nullptr);
    m_inputCapsFilter = gst_element_factory_make("capsfilter", nullptr);
    gst_bin_add_many(GST_BIN_CAST(m_bin.get()), m_audioconvert.get(), m_audioresample.get(), m_inputCapsFilter.get(), nullptr);
}

RTCRtpCapabilities RealtimeOutgoingAudioSourceGStreamer::rtpCapabilities() const
{
    auto& registryScanner = GStreamerRegistryScanner::singleton();
    return registryScanner.audioRtpCapabilities(GStreamerRegistryScanner::Configuration::Encoding);
}

bool RealtimeOutgoingAudioSourceGStreamer::setPayloadType(const GRefPtr<GstCaps>& codecPreferences)
{
    auto caps = adoptGRef(gst_caps_copy(codecPreferences.get()));
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

    m_inputCaps = adoptGRef(gst_caps_new_any());

    if (encoding == "opus"_s) {
        m_encoder = makeGStreamerElement("opusenc", nullptr);
        if (!m_encoder)
            return false;

        gst_structure_set(structure.get(), "encoding-name", G_TYPE_STRING, "OPUS", nullptr);

        // FIXME: Enable dtx too?
        gst_util_set_object_arg(G_OBJECT(m_encoder.get()), "audio-type", "voice");
        g_object_set(m_encoder.get(), "perfect-timestamp", TRUE, nullptr);

        if (auto useInbandFec = gstStructureGetString(structure.get(), "useinbandfec"_s)) {
            if (useInbandFec == "1"_s)
                g_object_set(m_encoder.get(), "inband-fec", true, nullptr);
            gst_structure_remove_field(structure.get(), "useinbandfec");
        }

        if (auto isStereo = gstStructureGetString(structure.get(), "stereo"_s)) {
            if (isStereo == "1"_s)
                m_inputCaps = adoptGRef(gst_caps_new_simple("audio/x-raw", "channels", G_TYPE_INT, 2, nullptr));
            gst_structure_remove_field(structure.get(), "stereo");
        }

        if (gst_caps_is_any(m_inputCaps.get())) {
            if (auto encodingParameters = gstStructureGetString(structure.get(), "encoding-params"_s)) {
                if (auto channels = parseIntegerAllowingTrailingJunk<int>(encodingParameters))
                    m_inputCaps = adoptGRef(gst_caps_new_simple("audio/x-raw", "channels", G_TYPE_INT, *channels, nullptr));
            }
        }
    } else if (encoding == "g722"_s)
        m_encoder = makeGStreamerElement("avenc_g722", nullptr);
    else if (encoding == "pcma"_s)
        m_encoder = makeGStreamerElement("alawenc", nullptr);
    else if (encoding == "pcmu"_s)
        m_encoder = makeGStreamerElement("mulawenc", nullptr);
    else {
        GST_ERROR_OBJECT(m_bin.get(), "Unsupported outgoing audio encoding: %s", encoding.ascii().data());
        return false;
    }

    if (!m_encoder) {
        GST_ERROR_OBJECT(m_bin.get(), "Encoder not found for encoding %s", encoding.ascii().data());
        return false;
    }

    // Align MTU with libwebrtc implementation, also helping to reduce packet fragmentation.
    g_object_set(m_payloader.get(), "auto-header-extension", TRUE, "mtu", 1200, nullptr);

    if (auto minPTime = gstStructureGetString(structure.get(), "minptime"_s)) {
        if (auto value = parseIntegerAllowingTrailingJunk<int64_t>(minPTime)) {
            if (gstObjectHasProperty(m_payloader.get(), "min-ptime"))
                g_object_set(m_payloader.get(), "min-ptime", *value * GST_MSECOND, nullptr);
            else
                GST_WARNING_OBJECT(m_payloader.get(), "min-ptime property not supported");
        }
        gst_structure_remove_field(structure.get(), "minptime");
    }

    if (auto payloadType = gstStructureGet<int>(structure.get(), "payload"_s)) {
        g_object_set(m_payloader.get(), "pt", *payloadType, nullptr);
        gst_structure_remove_field(structure.get(), "payload");
    }

    if (m_payloaderState) {
        g_object_set(m_payloader.get(), "seqnum-offset", m_payloaderState->seqnum, nullptr);
        m_payloaderState.reset();
    }

    auto rtpCaps = adoptGRef(gst_caps_new_empty());

    // When not present in caps, the vad support of the ssrc-audio-level extension should be
    // enabled. In order to prevent caps negotiation issues with downstream, explicitely set it.
    setSsrcAudioLevelVadOn(structure.get());

    gst_caps_append_structure(rtpCaps.get(), structure.release());

    g_object_set(m_inputCapsFilter.get(), "caps", m_inputCaps.get(), nullptr);
    g_object_set(m_capsFilter.get(), "caps", rtpCaps.get(), nullptr);
    GST_DEBUG_OBJECT(m_bin.get(), "RTP caps: %" GST_PTR_FORMAT, rtpCaps.get());

    gst_bin_add_many(GST_BIN_CAST(m_bin.get()), m_payloader.get(), m_encoder.get(), nullptr);

    auto preEncoderSinkPad = adoptGRef(gst_element_get_static_pad(m_preEncoderQueue.get(), "sink"));
    if (!gst_pad_is_linked(preEncoderSinkPad.get())) {
        if (!gst_element_link_many(m_outgoingSource.get(), m_liveSync.get(), m_audioconvert.get(), m_audioresample.get(), m_inputCapsFilter.get(), m_preEncoderQueue.get(), nullptr)) {
            GST_ERROR_OBJECT(m_bin.get(), "Unable to link outgoing source to pre-encoder queue");
            return false;
        }
    }

    return gst_element_link_many(m_preEncoderQueue.get(), m_encoder.get(), m_payloader.get(), m_postEncoderQueue.get(), nullptr);
}

void RealtimeOutgoingAudioSourceGStreamer::setParameters(GUniquePtr<GstStructure>&& parameters)
{
    m_parameters = WTFMove(parameters);
}

void RealtimeOutgoingAudioSourceGStreamer::teardown()
{
    RealtimeOutgoingMediaSourceGStreamer::teardown();
    m_audioconvert.clear();
    m_audioresample.clear();
    m_inputCaps.clear();
    m_inputCaps.clear();
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
