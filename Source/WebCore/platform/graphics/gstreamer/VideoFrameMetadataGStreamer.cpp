/*
 * Copyright (C) 2021 Igalia S.L
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "VideoFrameMetadataGStreamer.h"

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "GStreamerCommon.h"
#include <wtf/HashMap.h>
#include <wtf/glib/WTFGType.h>

GST_DEBUG_CATEGORY_STATIC(webkit_video_frame_meta_debug);
#define GST_CAT_DEFAULT webkit_video_frame_meta_debug

using namespace WebCore;

struct VideoFrameMetadataPrivate {
    std::optional<VideoFrameTimeMetadata> videoSampleMetadata;
    Lock lock;
    HashMap<GstElement*, std::pair<GstClockTime, GstClockTime>> processingTimes WTF_GUARDED_BY_LOCK(lock);
};

WEBKIT_DEFINE_ASYNC_DATA_STRUCT(VideoFrameMetadataPrivate);

typedef struct _VideoFrameMetadataGStreamer {
    GstMeta meta;
    VideoFrameMetadataPrivate* priv;
} VideoFrameMetadataGStreamer;

GType videoFrameMetadataAPIGetType()
{
    static GType type;
    static const gchar* tags[] = { nullptr };
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        type = gst_meta_api_type_register("WebKitVideoFrameMetadataAPI", tags);
    });
    return type;
}

#define VIDEO_FRAME_METADATA_API_TYPE videoFrameMetadataAPIGetType()
#define VIDEO_FRAME_METADATA_CAST(p) reinterpret_cast<VideoFrameMetadataGStreamer*>(p)
#define getInternalVideoFrameMetadata(buffer) VIDEO_FRAME_METADATA_CAST(gst_buffer_get_meta(buffer, VIDEO_FRAME_METADATA_API_TYPE))

const GstMetaInfo* videoFrameMetadataGetInfo();

std::pair<GstBuffer*, VideoFrameMetadataGStreamer*> ensureVideoFrameMetadata(GstBuffer* buffer)
{
    auto* meta = getInternalVideoFrameMetadata(buffer);
    if (meta)
        return { buffer, meta };

    buffer = gst_buffer_make_writable(buffer);
    return { buffer, VIDEO_FRAME_METADATA_CAST(gst_buffer_add_meta(buffer, videoFrameMetadataGetInfo(), nullptr)) };
}

const GstMetaInfo* videoFrameMetadataGetInfo()
{
    static const GstMetaInfo* metaInfo = nullptr;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        metaInfo = gst_meta_register(VIDEO_FRAME_METADATA_API_TYPE, "WebKitVideoFrameMetadata", sizeof(VideoFrameMetadataGStreamer),
            [](GstMeta* meta, gpointer, GstBuffer*) -> gboolean {
                auto* frameMeta = VIDEO_FRAME_METADATA_CAST(meta);
                frameMeta->priv = createVideoFrameMetadataPrivate();
                return TRUE;
            },
            [](GstMeta* meta, GstBuffer*) {
                auto* frameMeta = VIDEO_FRAME_METADATA_CAST(meta);
                destroyVideoFrameMetadataPrivate(frameMeta->priv);
            },
            [](GstBuffer* buffer, GstMeta* meta, GstBuffer*, GQuark type, gpointer) -> gboolean {
                if (!GST_META_TRANSFORM_IS_COPY(type))
                    return FALSE;

                auto* frameMeta = VIDEO_FRAME_METADATA_CAST(meta);
                auto [buf, copyMeta] = ensureVideoFrameMetadata(buffer);
                copyMeta->priv->videoSampleMetadata = frameMeta->priv->videoSampleMetadata;
                copyMeta->priv->processingTimes = frameMeta->priv->processingTimes;
                return TRUE;
            });
    });
    return metaInfo;
}

GstBuffer* webkitGstBufferSetVideoFrameTimeMetadata(GstBuffer* buffer, std::optional<VideoFrameTimeMetadata>&& metadata)
{
    if (!GST_IS_BUFFER(buffer))
        return nullptr;

    auto [modifiedBuffer, meta] = ensureVideoFrameMetadata(buffer);
    meta->priv->videoSampleMetadata = WTFMove(metadata);
    return modifiedBuffer;
}

void webkitGstTraceProcessingTimeForElement(GstElement* element)
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        GST_DEBUG_CATEGORY_INIT(webkit_video_frame_meta_debug, "webkitvideoframemeta", 0, "Video frame processing metrics");
    });

    auto sinkPad = adoptGRef(gst_element_get_static_pad(element, "sink"));
    auto srcPad = adoptGRef(gst_element_get_static_pad(element, "src"));
    if (!sinkPad || !srcPad) {
        GST_WARNING("Can't add the processing time probes for %s", GST_OBJECT_NAME(element));
        ASSERT_NOT_REACHED();
        return;
    }

    GST_DEBUG("Tracing processing time for %" GST_PTR_FORMAT, element);

    static auto probeType = static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_PUSH | GST_PAD_PROBE_TYPE_BUFFER);

    gst_pad_add_probe(sinkPad.get(), probeType, [](GstPad*, GstPadProbeInfo* info, gpointer userData) -> GstPadProbeReturn {
        auto [modifiedBuffer, meta] = ensureVideoFrameMetadata(GST_PAD_PROBE_INFO_BUFFER(info));
        GST_PAD_PROBE_INFO_DATA(info) = modifiedBuffer;
        Locker locker { meta->priv->lock };
        meta->priv->processingTimes.set(GST_ELEMENT_CAST(userData), std::make_pair(gst_util_get_timestamp(), GST_CLOCK_TIME_NONE));
        return GST_PAD_PROBE_OK;
    }, element, nullptr);

    gst_pad_add_probe(srcPad.get(), probeType, [](GstPad*, GstPadProbeInfo* info, gpointer userData) -> GstPadProbeReturn {
        auto* meta = getInternalVideoFrameMetadata(GST_PAD_PROBE_INFO_BUFFER(info));
        // Some decoders (such as theoradec) do not always copy the input meta to the output frame,
        // so we need to check the meta is valid here before accessing it.
        if (!meta)
            return GST_PAD_PROBE_OK;

        Locker locker { meta->priv->lock };
        auto* key = GST_ELEMENT_CAST(userData);
        auto [startTime, oldStopTime] = meta->priv->processingTimes.get(key);
        meta->priv->processingTimes.set(key, std::make_pair(startTime, gst_util_get_timestamp()));
        return GST_PAD_PROBE_OK;
    }, element, nullptr);
}

VideoFrameMetadata webkitGstBufferGetVideoFrameMetadata(GstBuffer* buffer)
{
    if (!GST_IS_BUFFER(buffer))
        return { };

    VideoFrameMetadata videoFrameMetadata;
    if (GST_BUFFER_PTS_IS_VALID(buffer))
        videoFrameMetadata.mediaTime = fromGstClockTime(GST_BUFFER_PTS(buffer)).toDouble();

    auto* meta = getInternalVideoFrameMetadata(buffer);
    if (!meta)
        return videoFrameMetadata;

    auto processingDuration = MediaTime::zeroTime();
    for (auto& [startTime, stopTime] : meta->priv->processingTimes.values())
        processingDuration += fromGstClockTime(GST_CLOCK_DIFF(startTime, stopTime));

    if (processingDuration != MediaTime::zeroTime())
        videoFrameMetadata.processingDuration = processingDuration.toDouble();

    auto videoSampleMetadata = meta->priv->videoSampleMetadata;
    if (!videoSampleMetadata)
        return videoFrameMetadata;

    if (videoSampleMetadata->captureTime)
        videoFrameMetadata.captureTime = videoSampleMetadata->captureTime->value();

    if (videoSampleMetadata->receiveTime)
        videoFrameMetadata.receiveTime = videoSampleMetadata->receiveTime->value();

    videoFrameMetadata.rtpTimestamp = videoSampleMetadata->rtpTimestamp;
    return videoFrameMetadata;
}

#undef GST_CAT_DEFAULT

#endif
