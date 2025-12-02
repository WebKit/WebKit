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
#include "GStreamerQuirks.h"
#include "GStreamerRegistryScanner.h"
#include "VideoFrameMetadataGStreamer.h"
#include <wtf/TZoneMallocInlines.h>

GST_DEBUG_CATEGORY(webkit_webrtc_incoming_track_processor_debug);
#define GST_CAT_DEFAULT webkit_webrtc_incoming_track_processor_debug

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GStreamerIncomingTrackProcessor);

GStreamerIncomingTrackProcessor::GStreamerIncomingTrackProcessor()
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_incoming_track_processor_debug, "webkitwebrtcincomingtrackprocessor", 0, "WebKit WebRTC Incoming Track Processor");
    });
}

void GStreamerIncomingTrackProcessor::configure(ThreadSafeWeakPtr<GStreamerMediaEndpoint>&& endPoint, GRefPtr<GstPad>&& pad)
{
    m_endPoint = WTFMove(endPoint);
    m_pad = WTFMove(pad);

    auto caps = adoptGRef(gst_pad_get_current_caps(m_pad.get()));
    if (!caps)
        caps = adoptGRef(gst_pad_query_caps(m_pad.get(), nullptr));

    ASCIILiteral typeName;
    if (doCapsHaveType(caps.get(), "audio"_s)) {
        typeName = "audio"_s;
        m_data.type = RealtimeMediaSource::Type::Audio;
    } else {
        typeName = "video"_s;
        m_data.type = RealtimeMediaSource::Type::Video;
    }
    m_data.caps = WTFMove(caps);

    GST_DEBUG_OBJECT(m_bin.get(), "Processing track with caps %" GST_PTR_FORMAT, m_data.caps.get());
    auto structure = gst_caps_get_structure(m_data.caps.get(), 0);
    if (auto ssrc = gstStructureGet<unsigned>(structure, "ssrc"_s)) {
        m_data.ssrc = *ssrc;
        auto msIdAttributeName = makeString("ssrc-"_s, *ssrc, "-msid"_s);
        if (auto msIdAttribute = gstStructureGetString(structure, CStringView::unsafeFromUTF8(msIdAttributeName.utf8().data()))) {
            auto components = String(msIdAttribute.span()).split(' ');
            if (components.size() == 2)
                m_sdpMsIdAndTrackId = { WTFMove(components[0]), WTFMove(components[1]) };
        }
    }

    if (auto mid = gstStructureGetString(structure, "a-mid"_s))
        m_data.mid = mid.span();

    m_data.mediaStreamBinName = makeString("incoming-"_s, typeName, "-track-"_s, m_data.ssrc, '-', unsafeSpan(GST_OBJECT_NAME(m_pad.get())));
    m_bin = gst_bin_new(m_data.mediaStreamBinName.ascii().data());

    g_object_get(m_pad.get(), "transceiver", &m_data.transceiver.outPtr(), nullptr);

    auto msIdAttribute = gstStructureGetString(structure, "a-msid"_s);
    if (!msIdAttribute.isEmpty()) {
        if (startsWith(msIdAttribute.span(), " "_s))
            m_sdpMsIdAndTrackId = { emptyString(), msIdAttribute.span().subspan(1) };
        else {
            auto components = String(msIdAttribute.span()).split(' ');
            if (components.size() == 2)
                m_sdpMsIdAndTrackId = { components[0], components[1] };
        }
    }

    if (m_sdpMsIdAndTrackId.second.isEmpty())
        retrieveMediaStreamAndTrackIdFromSDP();

    m_data.mediaStreamId = mediaStreamIdFromPad();

    if (!m_sdpMsIdAndTrackId.second.isEmpty())
        m_data.trackId = m_sdpMsIdAndTrackId.second;

    m_sink = gst_element_factory_make("fakesink", "sink");
    g_object_set(m_sink.get(), "sync", TRUE, "enable-last-sample", FALSE, "qos", TRUE, nullptr);
    auto queue = gst_element_factory_make("queue", "queue");

    auto trackProcessor = incomingTrackProcessor();

    gst_bin_add_many(GST_BIN_CAST(m_bin.get()), trackProcessor.get(), queue, m_sink.get(), nullptr);
    gst_element_link(queue, m_sink.get());

    auto sinkPad = adoptGRef(gst_element_get_static_pad(trackProcessor.get(), "sink"));
    gst_element_add_pad(m_bin.get(), gst_ghost_pad_new("sink", sinkPad.get()));

    if (m_data.type != RealtimeMediaSource::Type::Video || !m_isDecoding)
        return;

    auto sinkSinkPad = adoptGRef(gst_element_get_static_pad(m_sink.get(), "sink"));
    gst_pad_add_probe(sinkSinkPad.get(), GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM, reinterpret_cast<GstPadProbeCallback>(+[](GstPad*, GstPadProbeInfo* info, gpointer) -> GstPadProbeReturn {
        auto query = GST_PAD_PROBE_INFO_QUERY(info);
        if (GST_QUERY_TYPE(query) != GST_QUERY_ALLOCATION)
            return GST_PAD_PROBE_OK;

        gst_query_add_allocation_meta(query, GST_VIDEO_META_API_TYPE, nullptr);
        return GST_PAD_PROBE_HANDLED;
    }), nullptr, nullptr);
}

String GStreamerIncomingTrackProcessor::mediaStreamIdFromPad()
{
    // Look-up the mediastream ID, using the msid attribute, fall back to pad name if there is no msid.
    String mediaStreamId;
    if (gstObjectHasProperty(m_pad.get(), "msid"_s)) {
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
    if (!media) [[unlikely]]
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

    if (m_data.type == RealtimeMediaSource::Type::Video) {
        GST_DEBUG_OBJECT(m_bin.get(), "Requesting a key-frame");
        gst_pad_send_event(m_pad.get(), gst_video_event_new_upstream_force_key_unit(GST_CLOCK_TIME_NONE, TRUE, 1));
    }

    bool forceEarlyVideoDecoding = !g_strcmp0(g_getenv("WEBKIT_GST_WEBRTC_FORCE_EARLY_VIDEO_DECODING"), "1");
    GST_DEBUG_OBJECT(m_bin.get(), "Configuring for input caps: %" GST_PTR_FORMAT "%s", m_data.caps.get(), forceEarlyVideoDecoding ? " and early decoding" : "");
    if (!forceEarlyVideoDecoding) {
        auto structure = gst_caps_get_structure(m_data.caps.get(), 0);
        ASSERT(gst_structure_has_name(structure, "application/x-rtp"));
        auto encodingName = gstStructureGetString(structure, "encoding-name"_s);
        auto mediaType = makeString("video/x-"_s, String(encodingName.span()).convertToASCIILowercase());
        auto codecCaps = adoptGRef(gst_caps_new_empty_simple(mediaType.ascii().data()));

        auto& scanner = GStreamerRegistryScanner::singleton();
        if (scanner.areCapsSupported(GStreamerRegistryScanner::Configuration::Decoding, codecCaps, true)) {
            GST_DEBUG_OBJECT(m_bin.get(), "Hardware video decoder detected, deferring decoding to the source client");
            return createParser();
        }
    }

    GST_DEBUG_OBJECT(m_bin.get(), "Preparing video decoder for depayloaded RTP packets");
    GRefPtr<GstElement> decodebin = makeGStreamerElement("decodebin3"_s);
    m_isDecoding = true;

    g_signal_connect(decodebin.get(), "deep-element-added", G_CALLBACK(+[](GstBin*, GstBin*, GstElement* element, gpointer userData) {
        String elementClass = unsafeSpan(gst_element_get_metadata(element, GST_ELEMENT_METADATA_KLASS));
        auto classifiers = elementClass.split('/');
        if (!classifiers.contains("Depayloader"_s))
            return;

        configureVideoRTPDepayloader(element);
        auto self = reinterpret_cast<GStreamerIncomingTrackProcessor*>(userData);
        auto pad = adoptGRef(gst_element_get_static_pad(element, "sink"));
        self->installRtpBufferPadProbe(pad);
    }), this);

    g_signal_connect(decodebin.get(), "element-added", G_CALLBACK(+[](GstBin*, GstElement* element, gpointer userData) {
        String elementClass = unsafeSpan(gst_element_get_metadata(element, GST_ELEMENT_METADATA_KLASS));
        auto classifiers = elementClass.split('/');
        if (!classifiers.contains("Decoder"_s) || !classifiers.contains("Video"_s))
            return;

        configureMediaStreamVideoDecoder(element);
        webkitGstTraceProcessingTimeForElement(element);

        auto pad = adoptGRef(gst_element_get_static_pad(element, "src"));
        gst_pad_add_probe(pad.get(), static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM), [](GstPad* pad, GstPadProbeInfo* info, gpointer userData) -> GstPadProbeReturn {
            auto self = reinterpret_cast<GStreamerIncomingTrackProcessor*>(userData);
            if (info->type & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM) {
                auto event = GST_PAD_PROBE_INFO_EVENT(info);
                if (GST_EVENT_TYPE(event) == GST_EVENT_CAPS) {
                    GstCaps* caps;
                    gst_event_parse_caps(event, &caps);

                    GstVideoInfo info;
                    gst_video_info_init(&info);
                    if (!gst_video_info_from_caps(&info, caps))
                        return GST_PAD_PROBE_OK;

                    self->m_videoSize = { GST_VIDEO_INFO_WIDTH(&info), GST_VIDEO_INFO_HEIGHT(&info) };
                    gst_util_fraction_to_double(GST_VIDEO_INFO_FPS_N(&info), GST_VIDEO_INFO_FPS_D(&info), &self->m_frameRate);
                }
                return GST_PAD_PROBE_OK;
            }

            self->m_decodedVideoFrames++;

            auto decoder = adoptGRef(gst_pad_get_parent_element(pad));
            auto processingTime = webkitGstBufferGetProcessingTime(gst_pad_probe_info_get_buffer(info), decoder.get());
            if (processingTime.isInvalid())
                return GST_PAD_PROBE_OK;

            self->m_totalVideoDecodeTime += processingTime;
            return GST_PAD_PROBE_OK;
        }, userData, nullptr);
    }), this);

    g_signal_connect_swapped(decodebin.get(), "pad-added", G_CALLBACK(+[](GStreamerIncomingTrackProcessor* self, GstPad* pad) {
        auto queue = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(self->m_bin.get()), "queue"));
        auto sinkPad = adoptGRef(gst_element_get_static_pad(queue.get(), "sink"));
        gst_pad_link(pad, sinkPad.get());
        self->trackReady();
    }), this);
    return decodebin;
}

GRefPtr<GstElement> GStreamerIncomingTrackProcessor::createParser()
{
    GRefPtr<GstElement> parsebin = makeGStreamerElement("parsebin"_s);
    g_signal_connect(parsebin.get(), "element-added", G_CALLBACK(+[](GstBin*, GstElement* element, gpointer userData) {
        String elementClass = unsafeSpan(gst_element_get_metadata(element, GST_ELEMENT_METADATA_KLASS));
        auto classifiers = elementClass.split('/');
        if (!classifiers.contains("Depayloader"_s))
            return;

        configureVideoRTPDepayloader(element);
        auto self = reinterpret_cast<GStreamerIncomingTrackProcessor*>(userData);
        auto pad = adoptGRef(gst_element_get_static_pad(element, "sink"));
        self->installRtpBufferPadProbe(pad);
    }), this);

    auto& quirksManager = GStreamerQuirksManager::singleton();
    if (quirksManager.isEnabled()) {
        // Prevent auto-plugging of hardware-accelerated elements. Those will be used in the playback pipeline.
        g_signal_connect(parsebin.get(), "autoplug-select", G_CALLBACK(+[](GstElement*, GstPad*, GstCaps*, GstElementFactory* factory, gpointer) -> unsigned {
            static auto skipAutoPlug = gstGetAutoplugSelectResult("skip"_s);
            static auto tryAutoPlug = gstGetAutoplugSelectResult("try"_s);
            RELEASE_ASSERT(skipAutoPlug);
            RELEASE_ASSERT(tryAutoPlug);
            auto& quirksManager = GStreamerQuirksManager::singleton();
            auto isHardwareAccelerated = quirksManager.isHardwareAccelerated(factory).value_or(false);
            if (isHardwareAccelerated)
                return *skipAutoPlug;
            return *tryAutoPlug;
        }), nullptr);
    }

    g_signal_connect_swapped(parsebin.get(), "pad-added", G_CALLBACK(+[](GStreamerIncomingTrackProcessor* self, GstPad* pad) {
        auto queue = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(self->m_bin.get()), "queue"));
        auto sinkPad = adoptGRef(gst_element_get_static_pad(queue.get(), "sink"));
        gst_pad_link(pad, sinkPad.get());
        self->trackReady();
    }), this);
    return parsebin;
}

void GStreamerIncomingTrackProcessor::installRtpBufferPadProbe(const GRefPtr<GstPad>& pad)
{
    if (m_data.type == RealtimeMediaSource::Type::Audio)
        return;

    gst_pad_add_probe(pad.get(), static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_BUFFER), [](GstPad*, GstPadProbeInfo* info, gpointer userData) -> GstPadProbeReturn {
        VideoFrameTimeMetadata videoFrameTimeMetadata;
        videoFrameTimeMetadata.receiveTime = MonotonicTime::now().secondsSinceEpoch();

        auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);
        {
            GstMappedRtpBuffer rtpBuffer(buffer, GST_MAP_READ);
            if (!rtpBuffer) [[unlikely]]
                return GST_PAD_PROBE_OK;

            videoFrameTimeMetadata.rtpTimestamp = gst_rtp_buffer_get_timestamp(rtpBuffer.mappedData());
        }

        if (auto referenceTimestampMeta = gst_buffer_get_reference_timestamp_meta(buffer, GST_CAPS_CAST(userData))) {
            auto ntpTimestamp = referenceTimestampMeta->timestamp;
            videoFrameTimeMetadata.captureTime = Seconds::fromNanoseconds(gst_rtcp_ntp_to_unix(ntpTimestamp));
        }

        auto modifiedBuffer = webkitGstBufferSetVideoFrameMetadata(GRefPtr(buffer), videoFrameTimeMetadata);
        gst_pad_probe_info_set_buffer(info, modifiedBuffer.leakRef());
        return GST_PAD_PROBE_OK;
    }, gst_caps_new_empty_simple("timestamp/x-ntp"), reinterpret_cast<GDestroyNotify>(gst_caps_unref));
}

void GStreamerIncomingTrackProcessor::trackReady()
{
    auto endPoint = m_endPoint.get();
    if (!endPoint || endPoint->isStopped())
        return;

    m_isReady = true;
    GST_DEBUG_OBJECT(m_bin.get(), "MediaStream %s track %s on pad %" GST_PTR_FORMAT " is ready", m_data.mediaStreamId.utf8().data(), m_data.trackId.utf8().data(), m_pad.get());
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

    GUniqueOutPtr<GstStructure> stats;
    g_object_get(m_sink.get(), "stats", &stats.outPtr(), nullptr);

    auto droppedVideoFrames = gstStructureGet<uint64_t>(stats.get(), "dropped"_s).value_or(0);
    m_stats.reset(gst_structure_new("incoming-video-stats", "frames-decoded", G_TYPE_UINT64, m_decodedVideoFrames, "frames-dropped", G_TYPE_UINT64, droppedVideoFrames, nullptr));

    if (!m_videoSize.isZero())
        gst_structure_set(m_stats.get(), "frame-width", G_TYPE_UINT, static_cast<unsigned>(m_videoSize.width()), "frame-height", G_TYPE_UINT, static_cast<unsigned>(m_videoSize.height()), nullptr);

    auto averageRate = gstStructureGet<double>(stats.get(), "average-rate"_s);
    if (averageRate && m_frameRate)
        gst_structure_set(m_stats.get(), "frames-per-second", G_TYPE_DOUBLE, *averageRate * m_frameRate, nullptr);

    if (m_totalVideoDecodeTime.isValid())
        gst_structure_set(m_stats.get(), "total-decode-time", G_TYPE_DOUBLE, m_totalVideoDecodeTime.toDouble(), nullptr);

    return m_stats.get();
}

} // namespace WebCore

#undef GST_CAT_DEFAULT

#endif // USE(GSTREAMER_WEBRTC)
