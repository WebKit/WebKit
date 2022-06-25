/*
 *  Copyright (C) 2022 Igalia, S.L
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
#include "GStreamerVideoSinkCommon.h"

#if ENABLE(VIDEO)

#include "MediaPlayerPrivateGStreamer.h"
#include <gst/app/gstappsink.h>

using namespace WebCore;

GST_DEBUG_CATEGORY(webkit_gst_video_sink_common_debug);
#define GST_CAT_DEFAULT webkit_gst_video_sink_common_debug

void webKitVideoSinkSetMediaPlayerPrivate(GstElement* appSink, MediaPlayerPrivateGStreamer* player)
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_gst_video_sink_common_debug, "webkitvideosinkcommon", 0, "WebKit Video Sink Common utilities");
    });

    g_signal_connect(appSink, "new-sample", G_CALLBACK(+[](GstElement* sink, MediaPlayerPrivateGStreamer* player) -> GstFlowReturn {
        GRefPtr<GstSample> sample = adoptGRef(gst_app_sink_pull_sample(GST_APP_SINK(sink)));
        GstBuffer* buffer = gst_sample_get_buffer(sample.get());
        GST_TRACE_OBJECT(sink, "new-sample with PTS=%" GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_PTS(buffer)));
        player->triggerRepaint(sample.get());
        return GST_FLOW_OK;
    }), player);
    g_signal_connect(appSink, "new-preroll", G_CALLBACK(+[](GstElement* sink, MediaPlayerPrivateGStreamer* player) -> GstFlowReturn {
        GRefPtr<GstSample> sample = adoptGRef(gst_app_sink_pull_preroll(GST_APP_SINK(sink)));
        GstBuffer* buffer = gst_sample_get_buffer(sample.get());
        GST_DEBUG_OBJECT(sink, "new-preroll with PTS=%" GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_PTS(buffer)));
        player->triggerRepaint(sample.get());
        return GST_FLOW_OK;
    }), player);

    GRefPtr<GstPad> pad = adoptGRef(gst_element_get_static_pad(appSink, "sink"));
    gst_pad_add_probe(pad.get(), static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_PUSH | GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM | GST_PAD_PROBE_TYPE_EVENT_FLUSH | GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM), [](GstPad*, GstPadProbeInfo* info, gpointer userData) -> GstPadProbeReturn {
        auto* player = static_cast<MediaPlayerPrivateGStreamer*>(userData);

        if (info->type & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM) {
            if (GST_EVENT_TYPE(GST_PAD_PROBE_INFO_EVENT(info)) != GST_EVENT_TAG)
                return GST_PAD_PROBE_OK;
            GstTagList* tagList;
            gst_event_parse_tag(GST_PAD_PROBE_INFO_EVENT(info), &tagList);
            player->updateVideoOrientation(tagList);
            return GST_PAD_PROBE_OK;
        }

        if (info->type & GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM) {
            auto* query = GST_PAD_PROBE_INFO_QUERY(info);
            if (GST_QUERY_TYPE(query) == GST_QUERY_ALLOCATION) {
                gst_query_add_allocation_meta(query, GST_VIDEO_META_API_TYPE, nullptr);
                return GST_PAD_PROBE_OK;
            }

            // In some platforms (e.g. OpenMAX on the Raspberry Pi) when a resolution change occurs the
            // pipeline has to be drained before a frame with the new resolution can be decoded.
            // In this context, it's important that we don't hold references to any previous frame
            // (e.g. m_sample) so that decoding can continue.
            // We are also not supposed to keep the original frame after a flush.
            if (GST_QUERY_TYPE(query) != GST_QUERY_DRAIN)
                return GST_PAD_PROBE_OK;
            GST_DEBUG("Acting upon DRAIN query");
        }
        if (info->type & GST_PAD_PROBE_TYPE_EVENT_FLUSH) {
            if (GST_EVENT_TYPE(GST_PAD_PROBE_INFO_EVENT(info)) != GST_EVENT_FLUSH_START)
                return GST_PAD_PROBE_OK;
            GST_DEBUG("Acting upon flush-start event");
        }

#if USE(GSTREAMER_GL)
        player->flushCurrentBuffer();
#endif
        return GST_PAD_PROBE_OK;
    }, player, nullptr);
}

#endif // ENABLE(VIDEO)
