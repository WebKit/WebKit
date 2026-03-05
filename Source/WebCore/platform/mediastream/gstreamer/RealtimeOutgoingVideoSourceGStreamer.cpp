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
#include "ContextDestructionObserverInlines.h"
#include "GStreamerCommon.h"
#include "GStreamerMediaStreamSource.h"
#include "GStreamerRegistryScanner.h"
#include "GStreamerVideoRTPPacketizer.h"
#include "MediaStreamTrack.h"
#include <wtf/text/MakeString.h>

GST_DEBUG_CATEGORY(webkit_webrtc_outgoing_video_debug);
#define GST_CAT_DEFAULT webkit_webrtc_outgoing_video_debug

namespace WebCore {

RealtimeOutgoingVideoSourceGStreamer::RealtimeOutgoingVideoSourceGStreamer(const RefPtr<UniqueSSRCGenerator>& ssrcGenerator, const String& mediaStreamId, MediaStreamTrack& track)
    : RealtimeOutgoingMediaSourceGStreamer(RealtimeOutgoingMediaSourceGStreamer::Type::Video, ssrcGenerator, mediaStreamId, track)
{
    initialize();
}

RealtimeOutgoingVideoSourceGStreamer::RealtimeOutgoingVideoSourceGStreamer(const RefPtr<UniqueSSRCGenerator>& ssrcGenerator)
    : RealtimeOutgoingMediaSourceGStreamer(RealtimeOutgoingMediaSourceGStreamer::Type::Video, ssrcGenerator)
{
    initialize();
}

RealtimeOutgoingVideoSourceGStreamer::~RealtimeOutgoingVideoSourceGStreamer() = default;

void RealtimeOutgoingVideoSourceGStreamer::initialize()
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_outgoing_video_debug, "webkitwebrtcoutgoingvideo", 0, "WebKit WebRTC outgoing video");
    });
    registerWebKitGStreamerElements();

    static Atomic<uint64_t> sourceCounter = 0;
    gst_element_set_name(m_bin.get(), makeString("outgoing-video-source-"_s, sourceCounter.exchangeAdd(1)).ascii().data());

    m_fallbackSource = gst_element_factory_make("videotestsrc", nullptr);
    gst_util_set_object_arg(G_OBJECT(m_fallbackSource.get()), "pattern", "black");
    g_object_set(m_fallbackSource.get(), "is-live", TRUE, "do-timestamp", TRUE, nullptr);
    gst_bin_add(GST_BIN_CAST(m_bin.get()), m_fallbackSource.get());
    gst_element_link(m_fallbackSource.get(), m_inputSelector.get());
}

RTCRtpCapabilities RealtimeOutgoingVideoSourceGStreamer::rtpCapabilities() const
{
    auto& registryScanner = GStreamerRegistryScanner::singleton();
    return registryScanner.videoRtpCapabilities(GStreamerRegistryScanner::Configuration::Encoding);
}

GRefPtr<GstPad> RealtimeOutgoingVideoSourceGStreamer::outgoingSourcePad() const
{
    if (WEBKIT_IS_MEDIA_STREAM_SRC(m_outgoingSource.get()))
        return adoptGRef(gst_element_get_static_pad(m_outgoingSource.get(), "video_src0"));
    return adoptGRef(gst_element_get_static_pad(m_fallbackSource.get(), "src"));
}

RefPtr<GStreamerRTPPacketizer> RealtimeOutgoingVideoSourceGStreamer::createPacketizer(RefPtr<UniqueSSRCGenerator> ssrcGenerator, const GstStructure* codecParameters, GUniquePtr<GstStructure>&& encodingParameters)
{
    return GStreamerVideoRTPPacketizer::create(ssrcGenerator, codecParameters, WTF::move(encodingParameters));
}

void RealtimeOutgoingVideoSourceGStreamer::dispatchBitrateRequest(uint32_t bitrate)
{
    gst_element_send_event(m_bin.get(), gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM_OOB, gst_structure_new("encoder-bitrate-change-request", "bitrate", G_TYPE_UINT, static_cast<uint32_t>(bitrate / 1000), nullptr)));
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
