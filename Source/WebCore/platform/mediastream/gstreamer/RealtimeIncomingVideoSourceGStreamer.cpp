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

#if USE(GSTREAMER_WEBRTC)
#include "RealtimeIncomingVideoSourceGStreamer.h"

#include "GStreamerCommon.h"
#include "GStreamerRegistryScanner.h"
#include "GStreamerWebRTCUtils.h"
#include "VideoFrameGStreamer.h"
#include "VideoFrameMetadataGStreamer.h"

GST_DEBUG_CATEGORY(webkit_webrtc_incoming_video_debug);
#define GST_CAT_DEFAULT webkit_webrtc_incoming_video_debug

namespace WebCore {

RealtimeIncomingVideoSourceGStreamer::RealtimeIncomingVideoSourceGStreamer(AtomString&& videoTrackId)
    : RealtimeIncomingSourceGStreamer(CaptureDevice { WTFMove(videoTrackId), CaptureDevice::DeviceType::Camera, emptyString() })
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_incoming_video_debug, "webkitwebrtcincomingvideo", 0, "WebKit WebRTC incoming video");
    });
    static Atomic<uint64_t> sourceCounter = 0;
    gst_element_set_name(bin(), makeString("incoming-video-source-", sourceCounter.exchangeAdd(1)).ascii().data());
    GST_DEBUG_OBJECT(bin(), "New incoming video source created");

    auto sinkPad = adoptGRef(gst_element_get_static_pad(bin(), "sink"));
    gst_pad_add_probe(sinkPad.get(), static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_BUFFER), [](GstPad*, GstPadProbeInfo* info, gpointer) -> GstPadProbeReturn {
        auto videoFrameTimeMetadata = std::make_optional<VideoFrameTimeMetadata>({ });
        videoFrameTimeMetadata->receiveTime = MonotonicTime::now().secondsSinceEpoch();

        auto* buffer = GST_BUFFER_CAST(GST_PAD_PROBE_INFO_DATA(info));
        {
            GstMappedRtpBuffer rtpBuffer(buffer, GST_MAP_READ);
            if (rtpBuffer)
                videoFrameTimeMetadata->rtpTimestamp = gst_rtp_buffer_get_timestamp(rtpBuffer.mappedData());
        }

        buffer = webkitGstBufferSetVideoFrameTimeMetadata(buffer, WTFMove(videoFrameTimeMetadata));
        GST_PAD_PROBE_INFO_DATA(info) = buffer;
        return GST_PAD_PROBE_OK;
    }, nullptr, nullptr);

    start();
}

void RealtimeIncomingVideoSourceGStreamer::configureForInputCaps(const GRefPtr<GstCaps>& caps)
{
    bool forceEarlyVideoDecoding = !g_strcmp0(g_getenv("WEBKIT_GST_WEBRTC_FORCE_EARLY_VIDEO_DECODING"), "1");
    GST_DEBUG_OBJECT(bin(), "Configuring for input caps: %" GST_PTR_FORMAT "%s", caps.get(), forceEarlyVideoDecoding ? " and early decoding" : "");
    if (!forceEarlyVideoDecoding) {
        auto structure = gst_caps_get_structure(caps.get(), 0);
        ASSERT(gst_structure_has_name(structure, "application/x-rtp"));
        auto encodingNameValue = makeString(gst_structure_get_string(structure, "encoding-name"));
        auto mediaType = makeString("video/x-"_s, encodingNameValue.convertToASCIILowercase());
        auto codecCaps = adoptGRef(gst_caps_new_empty_simple(mediaType.ascii().data()));

        auto& scanner = GStreamerRegistryScanner::singleton();
        if (scanner.areCapsSupported(GStreamerRegistryScanner::Configuration::Decoding, codecCaps, true)) {
            GST_DEBUG_OBJECT(bin(), "Hardware video decoder detected, deferring decoding to the source client");
            createParser();
            return;
        }
    }

    GST_DEBUG_OBJECT(bin(), "Preparing video decoder for depayloaded RTP packets");
    auto decodebin = makeGStreamerElement("decodebin3", nullptr);

    g_signal_connect(decodebin, "deep-element-added", G_CALLBACK(+[](GstBin*, GstBin*, GstElement* element, gpointer) {
        auto elementClass = makeString(gst_element_get_metadata(element, GST_ELEMENT_METADATA_KLASS));
        auto classifiers = elementClass.split('/');
        if (!classifiers.contains("Depayloader"_s))
            return;

        configureVideoRTPDepayloader(element);
    }), nullptr);

    g_signal_connect(decodebin, "element-added", G_CALLBACK(+[](GstBin*, GstElement* element, gpointer userData) {
        auto elementClass = makeString(gst_element_get_metadata(element, GST_ELEMENT_METADATA_KLASS));
        auto classifiers = elementClass.split('/');
        if (!classifiers.contains("Decoder"_s) || !classifiers.contains("Video"_s))
            return;

        configureMediaStreamVideoDecoder(element);
        auto pad = adoptGRef(gst_element_get_static_pad(element, "src"));
        gst_pad_add_probe(pad.get(), static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM), [](GstPad*, GstPadProbeInfo* info, gpointer userData) -> GstPadProbeReturn {
            auto self = reinterpret_cast<RealtimeIncomingVideoSourceGStreamer*>(userData);
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

    g_signal_connect_swapped(decodebin, "pad-added", G_CALLBACK(+[](RealtimeIncomingVideoSourceGStreamer* source, GstPad* pad) {
        auto sinkPad = adoptGRef(gst_element_get_static_pad(source->m_tee.get(), "sink"));
        gst_pad_link(pad, sinkPad.get());

        gst_element_sync_state_with_parent(source->m_tee.get());
        gst_element_sync_state_with_parent(source->m_queue.get());
        gst_element_sync_state_with_parent(source->m_fakeVideoSink.get());
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN_CAST(source->bin()), GST_DEBUG_GRAPH_SHOW_ALL, GST_OBJECT_NAME(source->bin()));
    }), this);

    m_queue = makeGStreamerElement("queue", nullptr);
    m_fakeVideoSink = makeGStreamerElement("fakevideosink", nullptr);
    g_object_set(m_fakeVideoSink.get(), "enable-last-sample", FALSE, nullptr);

    gst_bin_add_many(GST_BIN_CAST(bin()), decodebin, m_queue.get(), m_fakeVideoSink.get(), nullptr);

    gst_element_link_many(m_tee.get(), m_queue.get(), m_fakeVideoSink.get(), nullptr);
    gst_element_sync_state_with_parent(m_queue.get());
    gst_element_sync_state_with_parent(m_fakeVideoSink.get());

    gst_element_link(m_valve.get(), decodebin);
    m_isDecoding = true;
}

const RealtimeMediaSourceSettings& RealtimeIncomingVideoSourceGStreamer::settings()
{
    if (m_currentSettings)
        return m_currentSettings.value();

    RealtimeMediaSourceSettings settings;
    RealtimeMediaSourceSupportedConstraints constraints;

    auto& size = this->size();
    if (!size.isZero()) {
        constraints.setSupportsWidth(true);
        constraints.setSupportsHeight(true);
        settings.setWidth(size.width());
        settings.setHeight(size.height());
    }

    if (double frameRate = this->frameRate()) {
        constraints.setSupportsFrameRate(true);
        settings.setFrameRate(frameRate);
    }

    settings.setSupportedConstraints(constraints);

    m_currentSettings = WTFMove(settings);
    return m_currentSettings.value();
}

void RealtimeIncomingVideoSourceGStreamer::settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag> settings)
{
    if (settings.containsAny({ RealtimeMediaSourceSettings::Flag::Width, RealtimeMediaSourceSettings::Flag::Height, RealtimeMediaSourceSettings::Flag::FrameRate }))
        m_currentSettings = std::nullopt;
}

void RealtimeIncomingVideoSourceGStreamer::ensureSizeAndFramerate(const GRefPtr<GstCaps>& caps)
{
    if (auto size = getVideoResolutionFromCaps(caps.get()))
        setSize({ static_cast<int>(size->width()), static_cast<int>(size->height()) });

    int frameRateNumerator, frameRateDenominator;
    auto* structure = gst_caps_get_structure(caps.get(), 0);
    if (!gst_structure_get_fraction(structure, "framerate", &frameRateNumerator, &frameRateDenominator))
        return;

    double framerate;
    gst_util_fraction_to_double(frameRateNumerator, frameRateDenominator, &framerate);
    setFrameRate(framerate);
}

void RealtimeIncomingVideoSourceGStreamer::dispatchSample(GRefPtr<GstSample>&& sample)
{
    ASSERT(isMainThread());
    auto* buffer = gst_sample_get_buffer(sample.get());
    auto* caps = gst_sample_get_caps(sample.get());
    ensureSizeAndFramerate(GRefPtr<GstCaps>(caps));

    videoFrameAvailable(VideoFrameGStreamer::create(WTFMove(sample), size(), fromGstClockTime(GST_BUFFER_PTS(buffer))), { });
}

const GstStructure* RealtimeIncomingVideoSourceGStreamer::stats()
{
    m_stats.reset(gst_structure_new_empty("incoming-video-stats"));

    if (m_isDecoding) {
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

    forEachVideoFrameObserver([&](auto& observer) {
        auto stats = observer.queryAdditionalStats();
        if (!stats)
            return;

        gst_structure_foreach(stats.get(), reinterpret_cast<GstStructureForeachFunc>(+[](GQuark fieldId, const GValue* value, gpointer userData) -> gboolean {
            auto* source = reinterpret_cast<RealtimeIncomingVideoSourceGStreamer*>(userData);
            gst_structure_set_value(source->m_stats.get(), g_quark_to_string(fieldId), value);
            return TRUE;
        }), this);
    });
    return m_stats.get();
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
