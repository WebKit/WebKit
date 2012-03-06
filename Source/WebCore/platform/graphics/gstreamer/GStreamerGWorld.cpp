/*
 *  Copyright (C) 2010 Igalia S.L
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "GStreamerGWorld.h"
#if ENABLE(VIDEO) && USE(GSTREAMER) && !defined(GST_API_VERSION_1)

#include "GRefPtrGStreamer.h"
#include <gst/gst.h>
#include <gst/interfaces/xoverlay.h>
#include <gst/pbutils/pbutils.h>

#if PLATFORM(GTK)
#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h> // for GDK_WINDOW_XID
#endif
#endif

using namespace std;

namespace WebCore {

gboolean gstGWorldSyncMessageCallback(GstBus* bus, GstMessage* message, gpointer data)
{
    ASSERT(GST_MESSAGE_TYPE(message) == GST_MESSAGE_ELEMENT);

    GStreamerGWorld* gstGWorld = static_cast<GStreamerGWorld*>(data);
    const GstStructure* structure = gst_message_get_structure(message);

    if (gst_structure_has_name(structure, "prepare-xwindow-id")
        || gst_structure_has_name(structure, "have-ns-view"))
        gstGWorld->setWindowOverlay(message);
    return TRUE;
}

PassRefPtr<GStreamerGWorld> GStreamerGWorld::createGWorld(GstElement* pipeline)
{
    return adoptRef(new GStreamerGWorld(pipeline));
}

GStreamerGWorld::GStreamerGWorld(GstElement* pipeline)
    : m_pipeline(pipeline)
{
    // XOverlay messages need to be handled synchronously.
    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline));
    gst_bus_set_sync_handler(bus, gst_bus_sync_signal_handler, this);
    g_signal_connect(bus, "sync-message::element", G_CALLBACK(gstGWorldSyncMessageCallback), this);
    gst_object_unref(bus);
}

GStreamerGWorld::~GStreamerGWorld()
{
    exitFullscreen();

    m_pipeline = 0;
}

bool GStreamerGWorld::enterFullscreen()
{
    if (m_dynamicPadName)
        return false;

    if (!m_videoWindow)
        m_videoWindow = PlatformVideoWindow::createWindow();

    GstElement* platformVideoSink = gst_element_factory_make("autovideosink", "platformVideoSink");
    GstElement* colorspace = gst_element_factory_make("ffmpegcolorspace", "colorspace");
    GstElement* queue = gst_element_factory_make("queue", "queue");
    GstElement* videoScale = gst_element_factory_make("videoscale", "videoScale");

    // Get video sink bin and the tee inside.
    GRefPtr<GstElement> videoSink;
    GstElement* sinkPtr = 0;

    g_object_get(m_pipeline, "video-sink", &sinkPtr, NULL);
    videoSink = adoptGRef(sinkPtr);

    GRefPtr<GstElement> tee = adoptGRef(gst_bin_get_by_name(GST_BIN(videoSink.get()), "videoTee"));

    // Add and link a queue, ffmpegcolorspace, videoscale and sink in the bin.
    gst_bin_add_many(GST_BIN(videoSink.get()), platformVideoSink, videoScale, colorspace, queue, NULL);

    // Faster elements linking.
    gst_element_link_pads_full(queue, "src", colorspace, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(colorspace, "src", videoScale, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(videoScale, "src", platformVideoSink, "sink", GST_PAD_LINK_CHECK_NOTHING);

    // Link a new src pad from tee to queue.
    GRefPtr<GstPad> srcPad = adoptGRef(gst_element_get_request_pad(tee.get(), "src%d"));
    GRefPtr<GstPad> sinkPad = adoptGRef(gst_element_get_static_pad(queue, "sink"));
    gst_pad_link(srcPad.get(), sinkPad.get());

    m_dynamicPadName.set(gst_pad_get_name(srcPad.get()));

    // Synchronize the new elements with pipeline state. If it's
    // paused limit the state change to pre-rolling.
    GstState state;
    gst_element_get_state(m_pipeline, &state, 0, 0);
    if (state < GST_STATE_PLAYING)
        state = GST_STATE_READY;

    gst_element_set_state(platformVideoSink, state);
    gst_element_set_state(videoScale, state);
    gst_element_set_state(colorspace, state);
    gst_element_set_state(queue, state);

    // Query the current media segment informations and send them towards
    // the new tee branch downstream.

    GstQuery* query = gst_query_new_segment(GST_FORMAT_TIME);
    gboolean queryResult = gst_element_query(m_pipeline, query);

    if (!queryResult) {
        gst_query_unref(query);
        return true;
    }

    GstFormat format;
    gint64 position;
    if (!gst_element_query_position(m_pipeline, &format, &position))
        position = 0;

    gdouble rate;
    gint64 startValue, stopValue;
    gst_query_parse_segment(query, &rate, &format, &startValue, &stopValue);

    GstEvent* event = gst_event_new_new_segment(FALSE, rate, format, startValue, stopValue, position);
    gst_pad_push_event(srcPad.get(), event);

    gst_query_unref(query);
    return true;
}

void GStreamerGWorld::exitFullscreen()
{
    if (!m_dynamicPadName)
        return;

    // Get video sink bin and the elements to remove.
    GRefPtr<GstElement> videoSink;
    GstElement* sinkPtr = 0;

    g_object_get(m_pipeline, "video-sink", &sinkPtr, NULL);
    videoSink = adoptGRef(sinkPtr);

    GRefPtr<GstElement> tee = adoptGRef(gst_bin_get_by_name(GST_BIN(videoSink.get()), "videoTee"));
    GRefPtr<GstElement> platformVideoSink = adoptGRef(gst_bin_get_by_name(GST_BIN(videoSink.get()), "platformVideoSink"));
    GRefPtr<GstElement> queue = adoptGRef(gst_bin_get_by_name(GST_BIN(videoSink.get()), "queue"));
    GRefPtr<GstElement> colorspace = adoptGRef(gst_bin_get_by_name(GST_BIN(videoSink.get()), "colorspace"));
    GRefPtr<GstElement> videoScale = adoptGRef(gst_bin_get_by_name(GST_BIN(videoSink.get()), "videoScale"));

    // Get pads to unlink and remove.
    GRefPtr<GstPad> srcPad = adoptGRef(gst_element_get_static_pad(tee.get(), m_dynamicPadName.get()));
    GRefPtr<GstPad> sinkPad = adoptGRef(gst_element_get_static_pad(queue.get(), "sink"));

    // Block data flow towards the pipeline branch to remove. No need
    // for pad blocking if the pipeline is paused.
    GstState state;
    gst_element_get_state(m_pipeline, &state, 0, 0);
    if (state < GST_STATE_PLAYING || gst_pad_set_blocked(srcPad.get(), true)) {

        // Unlink and release request pad.
        gst_pad_unlink(srcPad.get(), sinkPad.get());
        gst_element_release_request_pad(tee.get(), srcPad.get());

        // Unlink, remove and cleanup queue, ffmpegcolorspace, videoScale and sink.
        gst_element_unlink_many(queue.get(), colorspace.get(), videoScale.get(), platformVideoSink.get(), NULL);
        gst_bin_remove_many(GST_BIN(videoSink.get()), queue.get(), colorspace.get(), videoScale.get(), platformVideoSink.get(), NULL);
        gst_element_set_state(platformVideoSink.get(), GST_STATE_NULL);
        gst_element_set_state(videoScale.get(), GST_STATE_NULL);
        gst_element_set_state(colorspace.get(), GST_STATE_NULL);
        gst_element_set_state(queue.get(), GST_STATE_NULL);
    }

    m_dynamicPadName.clear();
}

void GStreamerGWorld::setWindowOverlay(GstMessage* message)
{
    GstObject* sink = GST_MESSAGE_SRC(message);

    if (!GST_IS_X_OVERLAY(sink))
        return;

    if (g_object_class_find_property(G_OBJECT_GET_CLASS(sink), "force-aspect-ratio"))
        g_object_set(sink, "force-aspect-ratio", TRUE, NULL);

    if (m_videoWindow) {
        m_videoWindow->prepareForOverlay(message);

// gst_x_overlay_set_window_handle was introduced in -plugins-base
// 0.10.31, just like the macro for checking the version.
#ifdef GST_CHECK_PLUGINS_BASE_VERSION
        gst_x_overlay_set_window_handle(GST_X_OVERLAY(sink), m_videoWindow->videoWindowId());
#else
        gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(sink), m_videoWindow->videoWindowId());
#endif
    }
}

}
#endif // ENABLE(VIDEO) && USE(GSTREAMER) && !defined(GST_API_VERSION_1)
