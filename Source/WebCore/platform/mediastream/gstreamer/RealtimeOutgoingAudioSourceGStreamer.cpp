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
    gst_element_set_name(m_bin.get(), makeString("outgoing-audio-source-", sourceCounter.exchangeAdd(1)).ascii().data());
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

    m_inputCaps = adoptGRef(gst_caps_new_any());

    if (encoding == "opus"_s) {
        m_encoder = makeGStreamerElement("opusenc", nullptr);
        if (!m_encoder)
            return false;

        // FIXME: Enable dtx too?
        gst_util_set_object_arg(G_OBJECT(m_encoder.get()), "audio-type", "voice");

        const char* useInbandFec = gst_structure_get_string(structure, "useinbandfec");
        if (!g_strcmp0(useInbandFec, "1"))
            g_object_set(m_encoder.get(), "inband-fec", true, nullptr);

        const char* isStereo = gst_structure_get_string(structure, "stereo");
        if (!g_strcmp0(isStereo, "1"))
            m_inputCaps = adoptGRef(gst_caps_new_simple("audio/x-raw", "channels", G_TYPE_INT, 2, nullptr));

    } else if (encoding == "g722"_s)
        m_encoder = makeGStreamerElement("avenc_g722", nullptr);
    else if (encoding == "pcma"_s)
        m_encoder = makeGStreamerElement("alawenc", nullptr);
    else if (encoding == "pcmu"_s)
        m_encoder = makeGStreamerElement("mulawenc", nullptr);
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

    if (m_payloaderState) {
        g_object_set(m_payloader.get(), "seqnum-offset", m_payloaderState->seqnum, nullptr);
        m_payloaderState.reset();
    }

    g_object_set(m_inputCapsFilter.get(), "caps", m_inputCaps.get(), nullptr);
    g_object_set(m_capsFilter.get(), "caps", caps.get(), nullptr);

    gst_bin_add_many(GST_BIN_CAST(m_bin.get()), m_payloader.get(), m_encoder.get(), nullptr);

    auto preEncoderSinkPad = adoptGRef(gst_element_get_static_pad(m_preEncoderQueue.get(), "sink"));
    if (!gst_pad_is_linked(preEncoderSinkPad.get())) {
        if (!gst_element_link_many(m_outgoingSource.get(), m_inputSelector.get(), m_liveSync.get(), m_audioconvert.get(), m_audioresample.get(), m_inputCapsFilter.get(), m_preEncoderQueue.get(), nullptr)) {
            GST_ERROR_OBJECT(m_bin.get(), "Unable to link outgoing source to pre-encoder queue");
            return false;
        }
    }

    return gst_element_link_many(m_preEncoderQueue.get(), m_encoder.get(), m_payloader.get(), m_postEncoderQueue.get(), nullptr);
}

void RealtimeOutgoingAudioSourceGStreamer::connectFallbackSource()
{
    if (!m_fallbackPad) {
        m_fallbackSource = makeGStreamerElement("audiotestsrc", nullptr);
        if (!m_fallbackSource) {
            WTFLogAlways("Unable to connect fallback audiotestsrc element, expect broken behavior. Please install gst-plugins-base.");
            return;
        }

        gst_util_set_object_arg(G_OBJECT(m_fallbackSource.get()), "wave", "silence");

        gst_bin_add(GST_BIN_CAST(m_bin.get()), m_fallbackSource.get());

        m_fallbackPad = adoptGRef(gst_element_request_pad_simple(m_inputSelector.get(), "sink_%u"));

        auto srcPad = adoptGRef(gst_element_get_static_pad(m_fallbackSource.get(), "src"));
        gst_pad_link(srcPad.get(), m_fallbackPad.get());
        gst_element_sync_state_with_parent(m_fallbackSource.get());
    }

    g_object_set(m_inputSelector.get(), "active-pad", m_fallbackPad.get(), nullptr);
}

void RealtimeOutgoingAudioSourceGStreamer::unlinkOutgoingSource()
{
    auto srcPad = adoptGRef(gst_element_get_static_pad(m_outgoingSource.get(), "audio_src0"));
    auto peerPad = adoptGRef(gst_pad_get_peer(srcPad.get()));
    if (!peerPad)
        return;

    gst_pad_unlink(srcPad.get(), peerPad.get());
    gst_element_release_request_pad(m_inputSelector.get(), peerPad.get());
}

void RealtimeOutgoingAudioSourceGStreamer::linkOutgoingSource()
{
    auto srcPad = adoptGRef(gst_element_get_static_pad(m_outgoingSource.get(), "audio_src0"));
    auto sinkPad = adoptGRef(gst_element_request_pad_simple(m_inputSelector.get(), "sink_%u"));
    gst_pad_link(srcPad.get(), sinkPad.get());
    g_object_set(m_inputSelector.get(), "active-pad", sinkPad.get(), nullptr);
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
