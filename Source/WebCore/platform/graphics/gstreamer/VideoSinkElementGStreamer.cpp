/*
 * Copyright (C) 2022 Igalia S.L
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
#include "VideoSinkElementGStreamer.h"

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "GStreamerCommon.h"
#include "MediaPlayerPrivateGStreamer.h"
#include <wtf/glib/WTFGType.h>

using namespace WebCore;

struct _WebKitVideoSinkElementPrivate {
    MediaPlayerPrivateGStreamer* mediaPlayerPrivate;
    GRefPtr<GstCaps> allowedCaps;
    GstVideoInfo info;
    GstCaps* currentCaps { nullptr };
};

GST_DEBUG_CATEGORY_STATIC(webkit_video_sink_element_debug);
#define GST_CAT_DEFAULT webkit_video_sink_element_debug

static GstStaticPadTemplate sinkTemplate = GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

#define webkit_video_sink_element_parent_class parent_class
WEBKIT_DEFINE_TYPE_WITH_CODE(WebKitVideoSinkElement, webkit_video_sink_element, GST_TYPE_VIDEO_SINK,
    GST_DEBUG_CATEGORY_INIT(webkit_video_sink_element_debug, "webkitvideosinkelement", 0, "webkit video sink element"))

static gboolean sinkProposeAllocation(GstBaseSink*, GstQuery* query)
{
    gst_query_add_allocation_meta(query, GST_VIDEO_META_API_TYPE, nullptr);
    return TRUE;
}

static GstFlowReturn sinkShowFrame(GstVideoSink* videoSink, GstBuffer* buffer)
{
    auto* sink = WEBKIT_VIDEO_SINK_ELEMENT(videoSink);
    auto sample = adoptGRef(gst_sample_new(buffer, sink->priv->currentCaps, nullptr, nullptr));
    sink->priv->mediaPlayerPrivate->triggerRepaint(sample.get());
    return GST_FLOW_OK;
}

static gboolean sinkQuery(GstBaseSink* baseSink, GstQuery* query)
{
    switch (GST_QUERY_TYPE(query)) {
    case GST_QUERY_DRAIN: {
#if USE(GSTREAMER_GL)
        // In some platforms (e.g. OpenMAX on the Raspberry Pi) when a resolution change occurs the
        // pipeline has to be drained before a frame with the new resolution can be decoded.
        // In this context, it's important that we don't hold references to any previous frame
        // (e.g. m_sample) so that decoding can continue.
        // We are also not supposed to keep the original frame after a flush.
        auto* sink = WEBKIT_VIDEO_SINK_ELEMENT(baseSink);
        sink->priv->mediaPlayerPrivate->flushCurrentBuffer();
#endif
        break;
    }
    default:
        break;
    }
    return GST_CALL_PARENT_WITH_DEFAULT(GST_BASE_SINK_CLASS, query, (baseSink, query), FALSE);
}

static gboolean sinkEvent(GstBaseSink* baseSink, GstEvent* event)
{
    auto* sink = WEBKIT_VIDEO_SINK_ELEMENT(baseSink);

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_TAG: {
        GstTagList* tagList;
        gst_event_parse_tag(event, &tagList);
        sink->priv->mediaPlayerPrivate->updateVideoOrientation(tagList);
        break;
    }
    default:
        break;
    }

    return GST_CALL_PARENT_WITH_DEFAULT(GST_BASE_SINK_CLASS, event, (baseSink, event), FALSE);
}

static gboolean sinkSetCaps(GstBaseSink* baseSink, GstCaps* caps)
{
    auto* sink = WEBKIT_VIDEO_SINK_ELEMENT(baseSink);
    auto* priv = sink->priv;

    GST_DEBUG_OBJECT(sink, "Current caps %" GST_PTR_FORMAT ", setting caps %" GST_PTR_FORMAT, priv->currentCaps, caps);

    GstVideoInfo videoInfo;
    gst_video_info_init(&videoInfo);
    if (!gst_video_info_from_caps(&videoInfo, caps)) {
        GST_ERROR_OBJECT(sink, "Invalid caps %" GST_PTR_FORMAT, caps);
        return FALSE;
    }

    priv->info = videoInfo;
    gst_caps_replace(&priv->currentCaps, caps);
    return TRUE;
}

static GstCaps* sinkGetCaps(GstBaseSink* baseSink, GstCaps* filter)
{
    auto* sink = WEBKIT_VIDEO_SINK_ELEMENT(baseSink);
    GstCaps* caps;

    GST_OBJECT_LOCK(sink);
    if ((caps = sink->priv->allowedCaps.get())) {
        if (filter)
            caps = gst_caps_intersect_full(filter, caps, GST_CAPS_INTERSECT_FIRST);
        else
            gst_caps_ref(caps);
    }

    GST_DEBUG_OBJECT(sink, "Got caps %" GST_PTR_FORMAT, caps);
    GST_OBJECT_UNLOCK(sink);
    return caps;
}

static void sinkGetTimes(GstBaseSink* baseSink, GstBuffer* buffer,  GstClockTime* start, GstClockTime* end)
{
    auto* sink = WEBKIT_VIDEO_SINK_ELEMENT(baseSink);

    if (GST_BUFFER_TIMESTAMP_IS_VALID(buffer)) {
        *start = GST_BUFFER_TIMESTAMP(buffer);
        if (GST_BUFFER_DURATION_IS_VALID(buffer))
            *end = *start + GST_BUFFER_DURATION(buffer);
        else if (GST_VIDEO_INFO_FPS_N(&sink->priv->info))
            *end = *start + gst_util_uint64_scale_int(GST_SECOND, GST_VIDEO_INFO_FPS_D(&sink->priv->info), GST_VIDEO_INFO_FPS_N(&sink->priv->info));
    }
}

static void sinkConstructed(GObject* object)
{
    GST_CALL_PARENT(G_OBJECT_CLASS, constructed, (object));

    auto* sink = WEBKIT_VIDEO_SINK_ELEMENT(object);
    g_object_set(sink, "enable-last-sample", FALSE, nullptr);
}

static void webkit_video_sink_element_class_init(WebKitVideoSinkElementClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->constructed = sinkConstructed;

    GstBaseSinkClass* baseSinkClass = GST_BASE_SINK_CLASS(klass);
    baseSinkClass->propose_allocation = GST_DEBUG_FUNCPTR(sinkProposeAllocation);
    baseSinkClass->event = GST_DEBUG_FUNCPTR(sinkEvent);
    baseSinkClass->query = GST_DEBUG_FUNCPTR(sinkQuery);
    baseSinkClass->set_caps = GST_DEBUG_FUNCPTR(sinkSetCaps);
    baseSinkClass->get_caps = GST_DEBUG_FUNCPTR(sinkGetCaps);
    baseSinkClass->get_times = GST_DEBUG_FUNCPTR(sinkGetTimes);

    GstVideoSinkClass* videoSinkClass = GST_VIDEO_SINK_CLASS(klass);
    videoSinkClass->show_frame = GST_DEBUG_FUNCPTR(sinkShowFrame);

    GstElementClass* elementClass = GST_ELEMENT_CLASS(klass);
    gst_element_class_add_pad_template(elementClass, gst_static_pad_template_get(&sinkTemplate));
    gst_element_class_set_static_metadata(elementClass, "WebKit video sink", "Sink/Video", "Renders video", "Philippe Normand <philn@igalia.com>");
}

void webKitVideoSinkElementSetMediaPlayerPrivate(WebKitVideoSinkElement* sink, WebCore::MediaPlayerPrivateGStreamer* player)
{
    sink->priv->mediaPlayerPrivate = player;
}

void webKitVideoSinkElementSetCaps(WebKitVideoSinkElement* sink, GRefPtr<GstCaps>&& caps)
{
    sink->priv->allowedCaps = WTFMove(caps);
}

#endif
