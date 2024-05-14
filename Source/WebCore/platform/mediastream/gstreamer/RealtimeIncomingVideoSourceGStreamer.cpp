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
    gst_element_set_name(bin(), makeString("incoming-video-source-"_s, sourceCounter.exchangeAdd(1)).ascii().data());
    GST_DEBUG_OBJECT(bin(), "New incoming video source created with ID %s", persistentID().ascii().data());
}

void RealtimeIncomingVideoSourceGStreamer::setUpstreamBin(const GRefPtr<GstElement>& bin)
{
    RealtimeIncomingSourceGStreamer::setUpstreamBin(bin);

    auto tee = adoptGRef(gst_bin_get_by_name(GST_BIN_CAST(m_upstreamBin.get()), "tee"));
    auto sinkPad = adoptGRef(gst_element_get_static_pad(tee.get(), "sink"));
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
        setIntrinsicSize({ static_cast<int>(size->width()), static_cast<int>(size->height()) });

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

    videoFrameAvailable(VideoFrameGStreamer::create(WTFMove(sample), intrinsicSize(), fromGstClockTime(GST_BUFFER_PTS(buffer))), { });
}

const GstStructure* RealtimeIncomingVideoSourceGStreamer::stats()
{
    m_stats.reset(gst_structure_new_empty("incoming-video-stats"));

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
