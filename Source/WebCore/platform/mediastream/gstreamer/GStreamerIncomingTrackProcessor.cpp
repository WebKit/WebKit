/*
 *  Copyright (C) 2024 Igalia S.L.
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
#include "GStreamerIncomingTrackProcessor.h"

#if USE(GSTREAMER_WEBRTC)

#include "GStreamerCommon.h"
#include "GStreamerRegistryScanner.h"

GST_DEBUG_CATEGORY(webkit_webrtc_incoming_track_processor_debug);
#define GST_CAT_DEFAULT webkit_webrtc_incoming_track_processor_debug

namespace WebCore {

GStreamerIncomingTrackProcessor::GStreamerIncomingTrackProcessor(ThreadSafeWeakPtr<GStreamerMediaEndpoint>&& endPoint, GRefPtr<GstPad>&& pad)
    : m_endPoint(WTFMove(endPoint))
    , m_pad(WTFMove(pad))
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_incoming_track_processor_debug, "webkitwebrtcincomingtrackprocessor", 0, "WebKit WebRTC Incoming Track Processor");
    });

    m_data.mediaStreamBinName = makeString(GST_OBJECT_NAME(m_pad.get()));
    m_bin = gst_bin_new(m_data.mediaStreamBinName.ascii().data());

    auto caps = adoptGRef(gst_pad_get_current_caps(m_pad.get()));
    if (!caps)
        caps = adoptGRef(gst_pad_query_caps(m_pad.get(), nullptr));

    GST_DEBUG_OBJECT(m_bin.get(), "Processing track with caps %" GST_PTR_FORMAT, caps.get());
    m_data.type = doCapsHaveType(caps.get(), "audio") ? RealtimeMediaSource::Type::Audio : RealtimeMediaSource::Type::Video;
    m_data.caps = WTFMove(caps);

    g_object_get(m_pad.get(), "transceiver", &m_data.transceiver.outPtr(), nullptr);
    retrieveMediaStreamAndTrackIdFromSDP();
    m_data.mediaStreamId = mediaStreamIdFromPad();

    if (!m_sdpMsIdAndTrackId.second.isEmpty())
        m_data.trackId = m_sdpMsIdAndTrackId.second;

    m_tee = gst_element_factory_make("tee", "tee");
    g_object_set(m_tee.get(), "allow-not-linked", TRUE, nullptr);

    auto trackProcessor = incomingTrackProcessor();
    m_data.isUpstreamDecoding = m_isDecoding;

    gst_bin_add_many(GST_BIN_CAST(m_bin.get()), m_tee.get(), trackProcessor.get(), nullptr);
    auto sinkPad = adoptGRef(gst_element_get_static_pad(trackProcessor.get(), "sink"));
    gst_element_add_pad(m_bin.get(), gst_ghost_pad_new("sink", sinkPad.get()));
}

String GStreamerIncomingTrackProcessor::mediaStreamIdFromPad()
{
    // Look-up the mediastream ID, using the msid attribute, fall back to pad name if there is no msid.
    String mediaStreamId;
    if (gstObjectHasProperty(m_pad.get(), "msid")) {
        GUniqueOutPtr<char> msid;
        g_object_get(m_pad.get(), "msid", &msid.outPtr(), nullptr);
        if (msid) {
            mediaStreamId = String::fromUTF8(msid.get());
            GST_DEBUG_OBJECT(m_bin.get(), "msid set from pad msid property: %s", mediaStreamId.utf8().data());
        }
    }

    if (!mediaStreamId.isEmpty())
        return mediaStreamId;

    if (!m_sdpMsIdAndTrackId.first.isEmpty()) {
        GST_DEBUG_OBJECT(m_bin.get(), "msid set from SDP media msid attribute: '%s'", m_sdpMsIdAndTrackId.first.utf8().data());
        return m_sdpMsIdAndTrackId.first;
    }

    GUniquePtr<gchar> name(gst_pad_get_name(m_pad.get()));
    mediaStreamId = String::fromLatin1(name.get());
    GST_DEBUG_OBJECT(m_bin.get(), "msid set from webrtcbin src pad name: %s", mediaStreamId.utf8().data());
    return mediaStreamId;
}

void GStreamerIncomingTrackProcessor::retrieveMediaStreamAndTrackIdFromSDP()
{
    auto endPoint = m_endPoint.get();
    if (!endPoint)
        return;

    GUniqueOutPtr<GstWebRTCSessionDescription> description;
    g_object_get(endPoint->webrtcBin(), "remote-description", &description.outPtr(), nullptr);

    unsigned mLineIndex;
    g_object_get(m_data.transceiver.get(), "mlineindex", &mLineIndex, nullptr);
    const auto media = gst_sdp_message_get_media(description->sdp, mLineIndex);
    if (UNLIKELY(!media))
        return;

    const char* msidAttribute = gst_sdp_media_get_attribute_val(media, "msid");
    if (!msidAttribute)
        return;

    GST_LOG_OBJECT(m_bin.get(), "SDP media msid attribute value: %s", msidAttribute);
    auto components = String::fromUTF8(msidAttribute).split(' ');
    if (components.size() != 2)
        return;

    m_sdpMsIdAndTrackId = { components[0], components[1] };
}

GRefPtr<GstElement> GStreamerIncomingTrackProcessor::incomingTrackProcessor()
{
    if (m_data.type == RealtimeMediaSource::Type::Audio)
        return createParser();

    bool forceEarlyVideoDecoding = !g_strcmp0(g_getenv("WEBKIT_GST_WEBRTC_FORCE_EARLY_VIDEO_DECODING"), "1");
    GST_DEBUG_OBJECT(m_bin.get(), "Configuring for input caps: %" GST_PTR_FORMAT "%s", m_data.caps.get(), forceEarlyVideoDecoding ? " and early decoding" : "");
    if (!forceEarlyVideoDecoding) {
        auto structure = gst_caps_get_structure(m_data.caps.get(), 0);
        ASSERT(gst_structure_has_name(structure, "application/x-rtp"));
        auto encodingNameValue = makeString(gst_structure_get_string(structure, "encoding-name"));
        auto mediaType = makeString("video/x-"_s, encodingNameValue.convertToASCIILowercase());
        auto codecCaps = adoptGRef(gst_caps_new_empty_simple(mediaType.ascii().data()));

        auto& scanner = GStreamerRegistryScanner::singleton();
        if (scanner.areCapsSupported(GStreamerRegistryScanner::Configuration::Decoding, codecCaps, true)) {
            GST_DEBUG_OBJECT(m_bin.get(), "Hardware video decoder detected, deferring decoding to the source client");
            return createParser();
        }
    }

    GST_DEBUG_OBJECT(m_bin.get(), "Preparing video decoder for depayloaded RTP packets");
    GRefPtr<GstElement> decodebin = makeGStreamerElement("decodebin3", nullptr);
    m_isDecoding = true;

    m_queue = gst_element_factory_make("queue", nullptr);
    m_fakeVideoSink = makeGStreamerElement("fakevideosink", nullptr);
    g_object_set(m_fakeVideoSink.get(), "enable-last-sample", FALSE, nullptr);
    gst_bin_add_many(GST_BIN_CAST(m_bin.get()), m_queue.get(), m_fakeVideoSink.get(), nullptr);
    gst_element_link(m_queue.get(), m_fakeVideoSink.get());

    g_signal_connect(decodebin.get(), "deep-element-added", G_CALLBACK(+[](GstBin*, GstBin*, GstElement* element, gpointer) {
        auto elementClass = makeString(gst_element_get_metadata(element, GST_ELEMENT_METADATA_KLASS));
        auto classifiers = elementClass.split('/');
        if (!classifiers.contains("Depayloader"_s))
            return;

        configureVideoRTPDepayloader(element);
    }), nullptr);

    g_signal_connect(decodebin.get(), "element-added", G_CALLBACK(+[](GstBin*, GstElement* element, gpointer userData) {
        auto elementClass = makeString(gst_element_get_metadata(element, GST_ELEMENT_METADATA_KLASS));
        auto classifiers = elementClass.split('/');
        if (!classifiers.contains("Decoder"_s) || !classifiers.contains("Video"_s))
            return;

        configureMediaStreamVideoDecoder(element);

        auto pad = adoptGRef(gst_element_get_static_pad(element, "src"));
        gst_pad_add_probe(pad.get(), static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM), [](GstPad*, GstPadProbeInfo* info, gpointer userData) -> GstPadProbeReturn {
            auto self = reinterpret_cast<GStreamerIncomingTrackProcessor*>(userData);
            if (info->type & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM) {
                auto event = GST_PAD_PROBE_INFO_EVENT(info);
                if (GST_EVENT_TYPE(event) == GST_EVENT_CAPS) {
                    GstCaps* caps;
                    gst_event_parse_caps(event, &caps);
                    self->m_videoSize = getVideoResolutionFromCaps(caps).value_or(FloatSize { 0, 0 });
                }
                return GST_PAD_PROBE_OK;
            }
            self->m_decodedVideoFrames++;
            return GST_PAD_PROBE_OK;
        }, userData, nullptr);
    }), this);

    g_signal_connect_swapped(decodebin.get(), "pad-added", G_CALLBACK(+[](GStreamerIncomingTrackProcessor* self, GstPad* pad) {
        auto sinkPad = adoptGRef(gst_element_get_static_pad(self->m_tee.get(), "sink"));
        gst_pad_link(pad, sinkPad.get());

        gst_element_link(self->m_tee.get(), self->m_queue.get());
        gst_element_sync_state_with_parent(self->m_tee.get());
        gst_element_sync_state_with_parent(self->m_queue.get());
        gst_element_sync_state_with_parent(self->m_fakeVideoSink.get());
        self->trackReady();
    }), this);
    return decodebin;
}

GRefPtr<GstElement> GStreamerIncomingTrackProcessor::createParser()
{
    GRefPtr<GstElement> parsebin = makeGStreamerElement("parsebin", nullptr);
    g_signal_connect(parsebin.get(), "element-added", G_CALLBACK(+[](GstBin*, GstElement* element, gpointer) {
        auto elementClass = makeString(gst_element_get_metadata(element, GST_ELEMENT_METADATA_KLASS));
        auto classifiers = elementClass.split('/');
        if (!classifiers.contains("Depayloader"_s))
            return;

        configureVideoRTPDepayloader(element);
    }), nullptr);

    g_signal_connect_swapped(parsebin.get(), "pad-added", G_CALLBACK(+[](GStreamerIncomingTrackProcessor* self, GstPad* pad) {
        auto sinkPad = adoptGRef(gst_element_get_static_pad(self->m_tee.get(), "sink"));
        gst_pad_link(pad, sinkPad.get());
        gst_element_sync_state_with_parent(self->m_tee.get());
        self->trackReady();
    }), this);
    return parsebin;
}

void GStreamerIncomingTrackProcessor::trackReady()
{
    auto endPoint = m_endPoint.get();
    if (!endPoint || endPoint->isStopped())
        return;

    m_isReady = true;
    GST_DEBUG_OBJECT(m_bin.get(), "Track %s on pad %" GST_PTR_FORMAT " is ready", m_data.mediaStreamId.utf8().data(), m_pad.get());
    callOnMainThread([endPoint = Ref { *endPoint }, this] {
        if (endPoint->isStopped())
            return;
        endPoint->connectIncomingTrack(m_data);
    });
}

const GstStructure* GStreamerIncomingTrackProcessor::stats()
{
    if (m_data.type == RealtimeMediaSource::Type::Audio)
        return nullptr;

    if (!m_isDecoding)
        return nullptr;

    m_stats.reset(gst_structure_new_empty("incoming-video-stats"));
    uint64_t droppedVideoFrames = 0;
    GUniqueOutPtr<GstStructure> stats;
    g_object_get(m_fakeVideoSink.get(), "stats", &stats.outPtr(), nullptr);
    if (!gst_structure_get_uint64(stats.get(), "dropped", &droppedVideoFrames))
        return m_stats.get();

    gst_structure_set(m_stats.get(), "frames-decoded", G_TYPE_UINT64, m_decodedVideoFrames, "frames-dropped", G_TYPE_UINT64, droppedVideoFrames, nullptr);
    if (!m_videoSize.isZero())
        gst_structure_set(m_stats.get(), "frame-width", G_TYPE_UINT, static_cast<unsigned>(m_videoSize.width()), "frame-height", G_TYPE_UINT, static_cast<unsigned>(m_videoSize.height()), nullptr);
    return m_stats.get();
}

} // namespace WebCore

#undef GST_CAT_DEFAULT

#endif // USE(GSTREAMER_WEBRTC)
