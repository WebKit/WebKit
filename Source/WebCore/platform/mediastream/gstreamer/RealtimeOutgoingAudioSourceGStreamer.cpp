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

#include "GStreamerAudioRTPPacketizer.h"
#include "GStreamerCommon.h"
#include "GStreamerMediaStreamSource.h"
#include "GStreamerRegistryScanner.h"
#include "MediaStreamTrack.h"
#include <wtf/text/MakeString.h>

GST_DEBUG_CATEGORY(webkit_webrtc_outgoing_audio_debug);
#define GST_CAT_DEFAULT webkit_webrtc_outgoing_audio_debug

namespace WebCore {

RealtimeOutgoingAudioSourceGStreamer::RealtimeOutgoingAudioSourceGStreamer(const RefPtr<UniqueSSRCGenerator>& ssrcGenerator, const String& mediaStreamId, MediaStreamTrack& track)
    : RealtimeOutgoingMediaSourceGStreamer(RealtimeOutgoingMediaSourceGStreamer::Type::Audio, ssrcGenerator, mediaStreamId, track)
{
    initialize();
}

RealtimeOutgoingAudioSourceGStreamer::RealtimeOutgoingAudioSourceGStreamer(const RefPtr<UniqueSSRCGenerator>& ssrcGenerator)
    : RealtimeOutgoingMediaSourceGStreamer(RealtimeOutgoingMediaSourceGStreamer::Type::Audio, ssrcGenerator)
{
    initialize();

    m_outgoingSource = gst_element_factory_make("audiotestsrc", nullptr);
    gst_util_set_object_arg(G_OBJECT(m_outgoingSource.get()), "wave", "silence");
    g_object_set(m_outgoingSource.get(), "is-live", TRUE, "do-timestamp", TRUE, nullptr);
    gst_bin_add(GST_BIN_CAST(m_bin.get()), m_outgoingSource.get());
}

RealtimeOutgoingAudioSourceGStreamer::~RealtimeOutgoingAudioSourceGStreamer() = default;

void RealtimeOutgoingAudioSourceGStreamer::initialize()
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_outgoing_audio_debug, "webkitwebrtcoutgoingaudio", 0, "WebKit WebRTC outgoing audio");
    });
    static Atomic<uint64_t> sourceCounter = 0;
    gst_element_set_name(m_bin.get(), makeString("outgoing-audio-source-"_s, sourceCounter.exchangeAdd(1)).ascii().data());
}

RTCRtpCapabilities RealtimeOutgoingAudioSourceGStreamer::rtpCapabilities() const
{
    auto& registryScanner = GStreamerRegistryScanner::singleton();
    return registryScanner.audioRtpCapabilities(GStreamerRegistryScanner::Configuration::Encoding);
}

GRefPtr<GstPad> RealtimeOutgoingAudioSourceGStreamer::outgoingSourcePad() const
{
    if (WEBKIT_IS_MEDIA_STREAM_SRC(m_outgoingSource.get()))
        return adoptGRef(gst_element_get_static_pad(m_outgoingSource.get(), "audio_src0"));
    return adoptGRef(gst_element_get_static_pad(m_outgoingSource.get(), "src"));
}

RefPtr<GStreamerRTPPacketizer> RealtimeOutgoingAudioSourceGStreamer::createPacketizer(RefPtr<UniqueSSRCGenerator> ssrcGenerator, const GstStructure* codecParameters, GUniquePtr<GstStructure>&& encodingParameters)
{
    return GStreamerAudioRTPPacketizer::create(ssrcGenerator, codecParameters, WTFMove(encodingParameters));
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
